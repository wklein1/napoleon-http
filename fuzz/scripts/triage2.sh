#!/usr/bin/env bash
# "triage2.sh" in help text

set -euo pipefail

#-----------------------------------------------------------
#                         Defaults
#-----------------------------------------------------------

MODE="${MODE:-}"                       # stack|coverage|combined
LINK_METHOD="${LINK_METHOD:-hard}"     # hard|sym|copy
INCLUDE_LINES="${INCLUDE_LINES:-0}"    # 0/1
STACK_FRAMES="${STACK_FRAMES:-3}"      # > 1
THRESHOLD="${THRESHOLD:-90}"           # 0 - 100
RECURSIVE="${RECURSIVE:-0}"            # 0/1
KEEP_LAYOUT="${KEEP_LAYOUT:-0}"        # 0/1
OVERWRITE="${OVERWRITE:-0}"            # 0/1
VERBOSE="${VERBOSE:-0}"                # 0/1
DEBUG_LOG="${DEBUG_LOG:-0}"            # 0/1

DOCKER_TIMEOUT="${DOCKER_TIMEOUT:-20s}"
IMAGE="${IMAGE:-aflplusplus/aflplusplus}"
HOST_DIR="${HOST_DIR:-$(pwd)}"
WORK_DIR="${WORK_DIR:-/work}"

ASAN_OPTS_DEFAULT='abort_on_error=1:detect_leaks=0:symbolize=1:allocator_may_return_null=1'
UBSAN_OPTS_DEFAULT='print_stacktrace=1:halt_on_error=1:report_error_type=1'
asan_ops=()  
ubsan_ops=()

STAGE1_LOG="${STAGE1_LOG-}"
LOG_FILE=""
OUT_BASE="${OUT_BASE-}" 
ENV_FILE="${ENV_FILE:-}"

SEP=$'\x1f'

#-----------------------------------------------------------
#                         Logger
#-----------------------------------------------------------

if [[ -t 2 && -z ${NO_COLOR:-} ]]; then
  C_RESET=$'\033[0m'
  C_ERROR=$'\033[38;5;160m'
  C_WARN=$'\033[38;5;208m'
  C_INFO=$'\033[2;32m'
else
  C_RESET='' C_ERROR='' C_WARN='' C_INFO=''
fi

debug_log_key_value(){
  if (( DEBUG_LOG == 0 )); then return 0; fi
  local section="$1"; shift
  printf '[%s] %s' "$(date +%H:%M:%S)" "$section" >> "$LOG_FILE"
  local key_value
  for key_value in "$@"; do printf ' | %s' "$key_value" >> "$LOG_FILE"; done
  printf '\n' >> "$LOG_FILE"
}

log_to_file(){
  [[ -n "$2" ]] || return 0
  printf "[%s] %s\n" "$(date +%H:%M:%S)" "$1" >> "$2"
}

modulate_log(){
  local fd="$1" color="$2" prefix="$3" ensure_nl="$4"; shift 4
  local format message nl=""

   if (( $# )); then
      format=$1; shift
      printf -v message -- "$format" "$@"
    else
      message=$format
    fi

  while [[ $message == *$'\n' ]]; do
    message=${message%$'\n'}
    nl+=$'\n'
  done

  if (( ensure_nl )) && [[ $nl != *$'\n' ]]; then
    nl+=$'\n'
  fi

  printf '%s%s%s%s%s' "$color" "$prefix" "$message" "$C_RESET" "$nl" >&"$fd"

  if (( DEBUG_LOG )) && [[ -n ${LOG_FILE:-} ]]; then
    log_to_file "${prefix}${message}" "$LOG_FILE"
  fi
}

LOG_INFO()  { (( VERBOSE )) && modulate_log 2 "$C_INFO"  "[INFO]  " 1 "$@"; }
LOG_WARN()  { modulate_log 2 "$C_WARN"  "[WARN]  " 1 "$@"; }
LOG_ERROR() { modulate_log 2 "$C_ERROR" "[ERROR] " 1 "$@"; }

LOG_INFO_PLAIN()  { (( VERBOSE )) && modulate_log 2 "$C_INFO"  "" 1 "$@"; }
LOG_WARN_PLAIN()  { modulate_log 2 "$C_WARN"  "" 1 "$@"; }
LOG_ERROR_PLAIN() { modulate_log 2 "$C_ERROR" "" 1 "$@"; }

LOG_INFO_PLAIN_INLINE()  { modulate_log 2 "$C_INFO"  "" 0 "$@"; }
LOG_WARN_PLAIN_INLINE()  { modulate_log 2 "$C_WARN"  "" 0 "$@"; }
LOG_ERROR_PLAIN_INLINE() { modulate_log 2 "$C_ERROR" "" 0 "$@"; }

LOG_DEFAULT()           { modulate_log 2 "" "" 0 "$@"; }

#-----------------------------------------------------------
#                      Env-file loader
#-----------------------------------------------------------

load_env_file(){
  local env_file="$1"
  [[ -r "$env_file" ]] || return 0

  local USE_WHITELIST=0
  local -a whitelist=( MODE THRESHOLD STACK_FRAMES ASAN_OPTIONS UBSAN_OPTIONS LINK_METHOD )
  

  while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"

    [[ -z "$line" || "${line:0:1}" == "#" ]] && continue

    if [[ $line =~ ^(export[[:space:]]+)?([A-Z0-9_]+)=(.*)$ ]]; then
      key="${BASH_REMATCH[2]}"
      raw_val="${BASH_REMATCH[3]}"

      if (( USE_WHITELIST )); then
        local ok=0
        for k in "${whitelist[@]}"; do
          [[ "$k" == "$key" ]] && ok=1 && break
        done
        if (( ok == 0 )); then
          LOG_WARN "Ignoring env var not on whitelist: %s" "$key"
          continue
        fi
      fi

      if [[ $raw_val =~ ^\"(.*)\"$ ]]; then
        val="${BASH_REMATCH[1]}"
      elif [[ $raw_val =~ ^\'(.*)\'$ ]]; then
        val="${BASH_REMATCH[1]}"
      else
        val="$raw_val"
      fi

      export "$key=$val"
      LOG_INFO "Loaded env: %s" "$key"
    else
      LOG_WARN "Ignoring invalid line in env file: %s" "$line"
    fi
  done < "$env_file"
}

if [[ -n "${ENV_FILE:-}" ]]; then
  load_env_file "$ENV_FILE"
fi

#-----------------------------------------------------------
#                            Help
#-----------------------------------------------------------

show_help(){
 cat <<'EOF'

Usage
  triage2.sh -m stack|coverage|combined [options] <crash_dir> <binary>

Summary
  Cluster crash inputs by call stack, coverage, or both (strict AND).
  Runs the instrumented target inside a Docker container ($IMAGE; default: aflplusplus/aflplusplus).
  Stacks and coverage are gathered in exactly one container run each (internal loop).

Precedence & configuration
  Every setting can come from:
    1) CLI flag (highest precedence)
    2) Environment variable (middle)
    3) Built-in default (lowest)
  Example: THRESHOLD=95 ./triage2.sh -m coverage ./crashes ./binary
  You can also preload env via ENV_FILE=/path/to/.env

Positional arguments
  <crash_dir>   Directory with crash inputs (files).
  <binary>      Target binary (must be executable).

──────────────────────────── Input & discovery

  -s, --stage1-log <file>    Use Stage-1 triage log for stack parsing (stack/combined only).
                             If omitted, stacks are reproduced in one container run.
  -r, --recursive            Recurse into <crash_dir>.

  Env: STAGE1_LOG (default: unset), RECURSIVE

──────────────────────────── Output & placement

  -o, --output <dir>         Output directory.
  -k, --keep, --keep-layout  Keep relative directory layout (only effective with -r).
  -l, --link-method <mode>   Place files by: hard | sym | copy (default: hard; falls back to copy)
  -O, --overwrite-existing   Allow overwriting existing files in output dirs.
                                
  Defaults:
    If --output is omitted:
      - with (-r AND -k): triage/stage2
      - otherwise:        <crash_dir>/dedup

    If --overwrite-existing is omitted:
      existing files get suffix "(1)", "(2)" etc.

  Env: OUT_BASE, KEEP_LAYOUT, LINK_METHOD, OVERWRITE

──────────────────────────── Clustering / comparison logic

  -m, --mode <mode>          stack | coverage | combined   (required)
  -f, --frames <n>           frames to include in stack signature (>=1, default: 3)
  -n, --include-lines        Include source line numbers in stack signature
  
  -t, --threshold, --coverage-threshold <pct>   Coverage similarity percent 0..100 (default: 90)

  Env: MODE, STACK_FRAMES, INCLUDE_LINES, THRESHOLD

  Modes:
    stack     Cluster by stack signature (first N frames; add lines with -n).
    coverage  Cluster by coverage key (exact; or >= threshold if using similarity).
    combined  Strict AND: same stack AND coverage similarity >= threshold (flat groups).

──────────────────────────── Docker / runtime

  -i, --image <name>                  Docker image (default: aflplusplus/aflplusplus)
  -w, --workdir, --work-dir <dir>     Container mount point (default: /work)
  -H, --hostdir, --host-dir <dir>     Host directory to mount (default: $PWD)
  -T, --timeout <duration>            Per-input timeout used inside the container (default: 20s).
                                      Accepts plain seconds or a suffix: s, m, h, d.

  Env: IMAGE, WORK_DIR, HOST_DIR, DOCKER_TIMEOUT
       (DOCKER_TIMEOUT default: 20s; passed to `timeout` inside the container)

──────────────────────────── Sanitizer options (passed into container)

  -a, --asan-opts     "[m:]<opts>"  Set ASAN_OPTIONS  (full replace, use "m:<opts>" for merge)
  --asan-opts-merge   "<opts>"      Merge into current ASAN_OPTIONS

  -u, --ubsan-opts    "[m:]<opts>"  Set UBSAN_OPTIONS (full replace, use "m:<opts>" for merge)
  --ubsan-opts-merge  "<opts>"      Merge into current UBSAN_OPTIONS

  Examples:
    ./triage2.sh -m stack -a "detect_leaks=1:allocator_may_return_null=0"   ./crashes ./bin
    ./triage2.sh -m stack -a "m:detect_leaks=1:allocator_may_return_null=0" ./crashes ./bin
    ./triage2.sh -m stack --asan-opts-merge "allocator_may_return_null=0"   ./crashes ./bin

  Env:
    ASAN_OPTIONS 
      (default inside script: abort_on_error=1:detect_leaks=0:symbolize=1:allocator_may_return_null=1 )
    UBSAN_OPTIONS 
      (default inside script: print_stacktrace=1:halt_on_error=1:report_error_type=1 )

──────────────────────────── Other

  -v, --verbose              Verbose progress output
  -d, --debug, --debug-log   Write a detailed clustering decision log
  -h, --help                 Show this help and exit

Env: VERBOSE, DEBUG_LOG, NO_COLOR, ENV_FILE

Requirements
  - Docker must be available in PATH.
  - For --mode stack or --mode combined: 
      The target binary should be built with ASan and/or UBSan and with symbolization enabled 
      Without sanitizers you will likely get no stack frames.
  - For --mode coverage/combined: 
      `afl-showmap` must be present in the Docker image.
  - <crash_dir>, <binary>, and the output directory must all reside under --hostdir.

Container log format
  Files:
    Stack:    <OUT_BASE>/stack_reproduce.log
    Coverage: <OUT_BASE>/coverage.log
    Debug:    <OUT_BASE>/triage2.log
  Per input:
    <<<BEGIN>>> base=<basename> path=<path-as-seen>
    ... tool output ...
    <<<END>>>   base=<basename>

Matching when reading logs
  1) Prefer matching on 'path=' (tries: original CLI arg, absolute host path,
     hostdir-relative path, and the resolved container path). Suffix match as fallback.
  2) If no path match is found, fall back to 'base=' only (first match wins).
     This avoids collisions when different inputs share the same basename.

Examples
  Stack-only clustering, first 4 frames, include line numbers:
  ./triage2.sh -m stack -f 4 -n ./crashes ./binary

  Coverage clustering with a 95% similarity threshold:
  THRESHOLD=95 ./triage2.sh -m coverage ./crashes ./binary
    (or) ./triage2.sh -m coverage -t 95 ./crashes ./binary

  Combined (stack + coverage), preserve subdirectory layout:
  ./triage2.sh -m combined -r -k ./crashes ./binary
EOF
}


#-----------------------------------------------------------
#                        CLI parsing 
#-----------------------------------------------------------

args=()
while (( $# > 0 )); do
  case "${1:-}" in
    #Input & discovery
    -s|--stage1-log)                        STAGE1_LOG="${2}"; shift 2 ;;
    -r|--recursive)                         RECURSIVE=1; shift ;;
    #Output & placement
    -o|--output)                            OUT_BASE="${2}"; shift 2 ;;
    -k|--keep|--keep-layout)                KEEP_LAYOUT=1; shift ;;
    -l|--link-method)                       LINK_METHOD="${2}"; shift 2 ;;
    -O|--overwrite-existing)                OVERWRITE=1; shift ;;
    #Clustering / comparison logic
    -m|--mode)                              MODE="${2}"; shift 2 ;;
    -f|--frames)                            STACK_FRAMES="${2}"; shift 2 ;;
    -n|--include-lines)                     INCLUDE_LINES=1; shift ;;
    -t|--threshold|--coverage-threshold)    THRESHOLD="${2}"; shift 2 ;;  
    #Docker / runtime
    -i|--image)                             IMAGE="${2}"; shift 2 ;;
    -w|--workdir|--work-dir)                WORK_DIR="${2}"; shift 2 ;;
    -H|--hostdir|--host-dir)                HOST_DIR="${2}"; shift 2 ;;
    -T|--timeout)                           DOCKER_TIMEOUT="${2}"; shift 2 ;;
    #Sanitizer options 
    -a|--asan-opts)                         asan_ops+=("$2"); shift 2 ;;
    --asan-opts-merge)                      asan_ops+=("m:$2"); shift 2 ;;
    -u|--ubsan-opts)                        ubsan_ops+=("$2"); shift 2 ;;
    --ubsan-opts-merge)                     ubsan_ops+=("m:$2"); shift 2 ;;
    #Other
    -v|--verbose)                           VERBOSE=1; shift ;;
    -d|--debug|--debug-log)                 DEBUG_LOG=1; shift ;;
    -h|--help)                              show_help; exit 0 ;;
    --)                                     shift; break ;;
    -*)                                     LOG_ERROR "Unknown option: %s" "$1"; show_help; exit 1 ;;
    *)                                      args+=("$1"); shift ;;
  esac
done
set -- "${args[@]}"

CRASH_DIR="${1:-}"
TARGET_BIN="${2:-}"


#-----------------------------------------------------------
#                       CLI Validation 
#-----------------------------------------------------------

command -v docker >/dev/null 2>&1 || { LOG_ERROR "docker not found in PATH."; exit 1; }

[[ -z "$CRASH_DIR" || -z "$TARGET_BIN" ]] && { show_help; exit 1; }
[[ -d "$CRASH_DIR" ]] || { LOG_ERROR "crash_dir '$CRASH_DIR' is not a directory."; exit 1; }
CRASH_DIR="${CRASH_DIR%/}"
[[ -f "$TARGET_BIN" && -x "$TARGET_BIN" ]] || { LOG_ERROR "binary '$TARGET_BIN' not found or not executable."; exit 1; }

if [[ -n "$STAGE1_LOG" && ! -r "$STAGE1_LOG" ]]; then
  LOG_ERROR "--stage1-log '$STAGE1_LOG' not readable."; exit 1
fi

case "$LINK_METHOD" in
  hard|sym|copy) ;;
  *) LOG_ERROR "invalid --link-method '$LINK_METHOD' (use hard|sym|copy)"; exit 1 ;;
esac

case "$MODE" in
  stack|coverage|combined) ;;
  *) LOG_ERROR "Invalid --mode '$MODE'"; { show_help; exit 1; };;
esac

if ! [[ "$THRESHOLD" =~ ^[0-9]+$ ]] || (( THRESHOLD < 0 || THRESHOLD > 100 )); then
   LOG_ERROR "--threshold must be an integer between 0 and 100."; exit 1
fi

if ! [[ "$STACK_FRAMES" =~ ^[0-9]+$ ]] || (( STACK_FRAMES < 1 )); then
   LOG_ERROR "--frames must be an integer greater than 0."; exit 1
fi

if ! [[ "$DOCKER_TIMEOUT" =~ ^[0-9]+([smhd])?$ ]]; then
  LOG_ERROR "--timeout expects a duration like 20s, 2m, 1h, or plain seconds."; exit 1

fi

#-----------------------------------------------------------
#                     Path/CSV helpers
#-----------------------------------------------------------

escape_csv(){ local s=${1-}; s=${s//\"/\"\"}; printf '%s' "$s"; }

resolve_realpath(){
  local input="$1"
  local dir_part file_part
  dir_part="$(cd "$(dirname "$input")" 2>/dev/null && pwd -P)" || return 1
  file_part="$(basename "$input")"
  printf '%s/%s\n' "$dir_part" "$file_part"
}


absolute_path(){
  case "$1" in
    /*) resolve_realpath "$1" ;;
    *)  resolve_realpath "$PWD/$1" ;;
  esac
}

require_under_host_dir(){
  local abs host_dir_abs
  if ! abs="$(resolve_realpath "$1")"; then
    local parent base
    parent="$(resolve_realpath "$(dirname "$1")")" || return 1
    base="$(basename "$1")"
    abs="$parent/$base"
  fi
  host_dir_abs="$(resolve_realpath "$HOST_DIR")" || return 1

  [[ "$abs" == "$host_dir_abs"/* ]] || {
    LOG_ERROR "'$abs' is outside of HOST_DIR='$HOST_DIR'."
    LOG_ERROR_PLAIN "  Use --hostdir to mount a parent directory that contains CRASH_DIR, TARGET_BIN, and OUT_BASE."
    exit 1
  }
}

to_container_path(){
  local host_abs host_dir_abs rel
  require_under_host_dir "$1" || exit 1
  host_abs="$(resolve_realpath "$1")"
  host_dir_abs="$(resolve_realpath "$HOST_DIR")"
  rel="${host_abs#"$host_dir_abs"/}"
  printf '%s/%s\n' "$WORK_DIR" "$rel"
}

same_file(){
  local f1="$1" f2="$2"
  [ -e "$f1" ] && [ -e "$f2" ] || return 1
  [ "$(resolve_realpath "$f1")" = "$(resolve_realpath "$f2")" ]
}

find_unique_name(){
  local base="$1"
  local ext=""
  local n=1
  while [ -e "$base$ext" ]; do
    ext="($n)"
    n=$((n+1))
  done
  printf '%s%s\n' "$base" "$ext"
}


link_into_group_dir(){
  local src_file="$1" group_dir="$2"
  mkdir -p  "$group_dir"

  local base_name dst_path src_abs dst_abs
  base_name="${src_file##*/}"
  dst_path="${group_dir%/}/$base_name"

  src_abs="$(resolve_realpath "$src_file")" || {
    LOG_ERROR "ERROR: cannot resolve '$src_file'"
    return 1
  }

  if [ -e "$dst_path" ]; then
    dst_abs="$(resolve_realpath "$dst_path" 2>/dev/null || echo "$dst_path")"

    if same_file "$src_abs" "$dst_abs"; then
      LOG_INFO "keep existing '$dst_path' (identical)"
      return 0
    fi

    if (( OVERWRITE )); then
      LOG_WARN "overwriting '$dst_path'"
      rm -f "$dst_path"
    else
      dst_path="$(find_unique_name "$dst_path")"
      LOG_INFO "renaming to avoid overwrite → $dst_path"
    fi
  fi

  case "$LINK_METHOD" in
    hard)
      ln "$src_file" "$dst_path" 2>/dev/null \
        || cp -p "$src_file" "$dst_path" 2>/dev/null || true
      ;;
    sym)
      ln -s "$src_abs" "$dst_path" 2>/dev/null \
        || cp -p "$src_file" "$dst_path" 2>/dev/null || true
      ;;
    copy|*)
      cp -p "$src_file" "$dst_path" 2>/dev/null || true
      ;;
  esac
}


dirname_safe(){
  local p="$1"
  if [[ "$p" == */* ]]; then
    printf '%s\n' "${p%/*}"
  else
    printf '%s\n' ""
  fi
}

relative_parent_of_source(){
  local file="$1"
  local rel="${file#"$CRASH_DIR"/}"
  if [[ "$rel" == "$file" ]]; then
    printf '%s\n' ""
  else
    dirname_safe "$rel"
  fi
}

destination_base_for_file(){
  local src_file="$1"
  if (( RECURSIVE && KEEP_LAYOUT )); then
    local rel_parent; rel_parent="$(relative_parent_of_source "$src_file")"
    if [[ -n "$rel_parent" ]]; then
      printf '%s/%s\n' "$OUT_BASE" "$rel_parent"
    else
      printf '%s\n' "$OUT_BASE"
    fi
  else
    printf '%s\n' "$OUT_BASE"
  fi
}

#-----------------------------------------------------------
#                      Mount validation
#-----------------------------------------------------------

assert_safe_host_dir(){
  local bad_exact=("/" "/root" "/home")
  local bad_prefix=("/etc" "/var" "/usr" "/bin" "/sbin" "/lib" "/opt")

  local host_abs
  host_abs="$(cd "$HOST_DIR" 2>/dev/null && pwd -P)" || {
    LOG_ERROR "HOST_DIR '$HOST_DIR' not accessible."; exit 1; }

  for path in "${bad_exact[@]}"; do
    if [[ "$host_abs" == "$path" ]]; then
      LOG_ERROR "HOST_DIR='$HOST_DIR' is unsafe to mount into container."
      LOG_ERROR_PLAIN "    Please specify a narrower directory (not system root or home)."
      exit 1
    fi
  done
  for path in "${bad_prefix[@]}"; do
    case "$host_abs" in
      "$path"|"$path"/*)
        LOG_ERROR "ERROR: HOST_DIR='$HOST_DIR' is unsafe to mount into container."
        exit 1
      ;;
    esac
  done
}

assert_safe_host_dir

LOG_WARN "Docker: mounting host directory: '%s' → '%s'\n" "$HOST_DIR" "$WORK_DIR"

require_under_host_dir "$CRASH_DIR"
require_under_host_dir "$TARGET_BIN"

#-----------------------------------------------------------
#                           Setup
#-----------------------------------------------------------

if [[ -z "${OUT_BASE:-}" ]]; then
  if (( RECURSIVE && KEEP_LAYOUT )); then
    OUT_BASE="triage/stage2"
  else
    OUT_BASE="${CRASH_DIR%/}/dedup"
  fi
fi

require_under_host_dir "$OUT_BASE"

mkdir -p "$OUT_BASE" || { LOG_ERROR "Error: Cannot create output dir: %s" "$OUT_BASE"; exit 1; }

MANIFEST="${OUT_BASE%/}/triage2_manifest.csv"
echo 'crashfile,group_key,group_dir,group_name,bug_type,exitcode,stack_source,stack_frames,cov_hits' > "$MANIFEST"

STACK_LOG="${OUT_BASE%/}/stack_reproduce.log"
COVERAGE_LOG="${OUT_BASE%/}/coverage.log"
LOG_FILE="${OUT_BASE%/}/triage2.log"

if [[ -z "${STAGE1_LOG:-}" && ( "$MODE" == "stack" || "$MODE" == "combined" ) ]]; then
  : > "$STACK_LOG"
fi

if [[ "$MODE" == "coverage" || "$MODE" == "combined" ]]; then
  : > "$COVERAGE_LOG"
fi

if (( DEBUG_LOG )); then : > "$LOG_FILE"; fi
#-----------------------------------------------------------
#                      Sanitizer options
#-----------------------------------------------------------

merge_opts(){
  awk -v base_options="$1" -v override_options="$2" '
    function add_key_value_pairs(input_string, value_map, order_array, order_length,   split_count,i,pair,key,value,parts) {
      split_count = split(input_string, parts, ":")
      for (i = 1; i <= split_count; i++) {
        pair = parts[i]
        if (pair == "") continue
        key   = pair; sub(/=.*/, "", key)
        value = pair; sub(/^[^=]*=/, "", value)
        if (!(key in value_map)) {
          order_length++
          order_array[order_length] = key
        }
        value_map[key] = value
      }
      return order_length
    }
    BEGIN {
      base_order_length = add_key_value_pairs(base_options,     base_map,     base_order, 0)
      over_order_length = add_key_value_pairs(override_options, override_map, over_order, 0)

      for (key in override_map) base_map[key] = override_map[key]

      output_length = 0
      for (i = 1; i <= base_order_length; i++) output_keys[++output_length] = base_order[i]
      for (i = 1; i <= over_order_length; i++) {
        key = over_order[i]; seen = 0
        for (j = 1; j <= output_length; j++) if (output_keys[j] == key) { seen = 1; break }
        if (!seen) output_keys[++output_length] = key
      }

      for (i = 1; i <= output_length; i++) {
        key = output_keys[i]; printf "%s=%s", key, base_map[key]
        if (i < output_length) printf ":"
      }
      printf "\n"
    }'
}


finalize_sanitizer_opts(){
  local base_options="$1"; shift
  local result_options="$base_options"
  local token payload

  for token in "$@"; do
    if [[ "$token" == m:* ]]; then
      payload="${token#m:}"
      [[ -z "$payload" ]] && continue
      result_options="$(merge_opts "$result_options" "$payload")"
    else
      result_options="$token"
    fi
  done

  printf '%s\n' "$result_options"
}

ASAN_OPTS="$(finalize_sanitizer_opts "${ASAN_OPTIONS-$ASAN_OPTS_DEFAULT}" "${asan_ops[@]}")"
UBSAN_OPTS="$(finalize_sanitizer_opts "${UBSAN_OPTIONS-$UBSAN_OPTS_DEFAULT}" "${ubsan_ops[@]}")"


print_effective_config(){ (
  bool() { if (( ${1:-0} )); then printf 'yes'; else printf 'no'; fi; }
  print_conf() { printf '  %-22s %s\n' "$1" "$2"; }

  printf "\n"
  printf "=== Effective configuration ===\n"
  print_conf "Env file"                "${ENV_FILE:-<none>}"
  print_conf "Mode"                    "${MODE}"
  print_conf "Crash dir"               "${CRASH_DIR}"
  print_conf "Binary"                  "${TARGET_BIN}"
  print_conf "Output base"             "${OUT_BASE}"
  print_conf "Recursive"               "$(bool "${RECURSIVE}")"
  print_conf "Keep layout"             "$(bool "${KEEP_LAYOUT}")"
  print_conf "Link method"             "${LINK_METHOD}"
  print_conf "Overwrite existing"      "$(bool "${OVERWRITE}")"
  print_conf "Stack frames"            "${STACK_FRAMES}"
  print_conf "Include line numbers"    "$(bool "${INCLUDE_LINES}")"
  print_conf "Coverage threshold (%)"  "${THRESHOLD}"
  print_conf "Docker image"            "${IMAGE}"
  print_conf "Host dir"                "${HOST_DIR}"
  print_conf "Work dir (container)"    "${WORK_DIR}"
  print_conf "Docker timeout"          "${DOCKER_TIMEOUT}"
  print_conf "Stage-1 log"             "${STAGE1_LOG:-<none>}"
  print_conf "ASAN_OPTIONS"            "${ASAN_OPTS}"
  print_conf "UBSAN_OPTIONS"           "${UBSAN_OPTS}"
  print_conf "Verbose output"          "$(bool "${VERBOSE}")"
  print_conf "Debug log"               "$(bool "${DEBUG_LOG}")"
  printf "===============================\n\n"
); }

if (( VERBOSE )); then print_effective_config >&2; fi
if (( DEBUG_LOG )); then print_effective_config >> "$LOG_FILE"; fi

#-----------------------------------------------------------
#                Stack/Bug parsing helpers
#-----------------------------------------------------------

#deprecated
extract_block_by_basename(){
  local base_name="$1"
  local log_path="$2"

  awk -v want="$base_name" '
    function trim_cr(string){
       sub(/\r$/,"",string); 
       return string 
    }
    BEGIN{ in_block=0 }

    /^<<<BEGIN>>> /{
      line=$0
      sub(/^.*base=/,"",line)
      sub(/ path=.*$/,"",line)
      in_block=(line==want); block=""
      next
    }

    in_block && /^<<<END>>> /{ print block; exit }
    in_block { block = block trim_cr($0) ORS }
  ' "$log_path"
}

extract_block_by_file(){
  local host_file_path="$1"
  local log_path="$2"

  local base_name absolute_host_path container_path original_arg_path relative_under_host host_dir_abs
  base_name="${host_file_path##*/}"
  original_arg_path="$host_file_path"

  absolute_host_path="$(resolve_realpath "$host_file_path")" || return 1
  host_dir_abs="$(resolve_realpath "$HOST_DIR")" || return 1

  relative_under_host=""
  if [[ "$absolute_host_path" == "$host_dir_abs"/* ]]; then
    relative_under_host="${absolute_host_path#"$host_dir_abs/"}"
  fi

  container_path="$(to_container_path "$host_file_path" 2>/dev/null || true)"

  local candidate_paths_joined="${original_arg_path}${SEP}${container_path}${SEP}${absolute_host_path}${SEP}${relative_under_host}"

  awk -v desired_base_name="$base_name" \
      -v candidate_list_joined="$candidate_paths_joined" \
      -v SEP="$SEP" ' 
    function trim_carriage_return(string_value){ sub(/\r$/,"",string_value); return string_value }
    
    function ends_with(path,suffix){
      if (suffix=="" || path=="" || length(path)<length(suffix)) return 0
      return substr(path, length(path)-length(suffix)+1) == suffix
    }

    
    function push_record(rec_base,rec_path,rec_block){
      record_count++; 
      base_names[record_count]=rec_base;
      path_values[record_count]=rec_path;
      block_contents[record_count]=rec_block
    }

    BEGIN{
      record_count=0; in_block=0; current_base=""; current_path=""; current_block=""
      candidate_count=split(candidate_list_joined, candidate_list, SEP)
    }

    /^<<<BEGIN>>> /{
      in_block=0; current_block=""; current_path=""; current_base=""
      line_text=$0
      if (match(line_text, /base=([^ ]+)/, base_match)) current_base=base_match[1]
      if (match(line_text, / path=(.*)$/,  path_match))  current_path=path_match[1]
      if (current_path=="(null)") current_path=""
      in_block=1
      next
    }

    in_block && /^<<<END>>> /{ push_record(current_base,current_path,current_block); in_block=0; next }

    in_block { current_block = current_block trim_carriage_return($0) ORS; next }

    END{
      for (i=1;i<=record_count;i++){
        if (base_names[i]==desired_base_name){
          for (cand_idx=1;cand_idx<=candidate_count;cand_idx++){
            if (candidate_list[cand_idx]!="" && path_values[i]==candidate_list[cand_idx]){
              print block_contents[i]; 
              exit 
            }
          }
        }
      }
      raw_arg=candidate_list[1]; rel_under_host=candidate_list[4]
      for(i=1;i<=record_count;i++){ 
        if (base_names[i]==desired_base_name){
          if(raw_arg!="" && ends_with(path_values[i],("/" raw_arg))){
            print block_contents[i];
            exit 
          }
          if(rel_under_host!=""&& ends_with(path_values[i],("/" rel_under_host))){
            print block_contents[i];
            exit
          }
        }
      }
      for(i=1;i<=record_count;i++){
        if(base_names[i]==desired_base_name){
          print block_contents[i]; 
          exit
        }
      }
      exit 1
    }
  ' "$log_path"
}


extract_asan_from_block(){
  awk '
    /ERROR:[[:space:]]*AddressSanitizer:/ {
      sub(/\r/,""); summary=$0; bug="unknown";
      if (match($0,/AddressSanitizer[[:space:]]*:[[:space:]]*([A-Za-z0-9_+-]+)/,bug_match)) 
        bug=bug_match[1];
      print summary "\n" bug; exit
    }
  '
}

extract_ubsan_from_block(){
  awk '
    /runtime error:/ {
      sub(/\r/,""); summary=$0; low=tolower($0); bug="unknown";
      if (low ~ /shift/ && (low ~ /out.*bounds/ || low ~ /exponent/)) bug="shift-out-of-bounds";
      else if (low ~ /division by zero/) bug="division-by-zero";
      else if (low ~ /overflow/) bug="integer-overflow";
      else if (low ~ /out[- ]of[- ]bounds/) bug="out-of-bounds";
      else if (low ~ /null.*(pointer|value)?/) bug="null";
      else if (low ~ /misalign/) bug="misaligned-pointer";
      else if (low ~ /type-mismatch|bad-cast|invalid-cast/) bug="type-mismatch";
      else if (low ~ /vla/) bug="vla-bound";
      print summary "\n" bug; exit
    }
  '
}

extract_exitcode_from_block(){
  awk -F'[ =]+' '/^<<<EXIT>>>/ {exit_code=$NF} END{ if(exit_code=="") exit_code="NA"; print exit_code }'
}

derive_stack_signature_from_text(){
  awk -v max_frames="$STACK_FRAMES" -v include_lines="$INCLUDE_LINES" '
    BEGIN{
      capture_frames   = 0
      frame_count      = 0
      sanitizer_tag    = "no_stack"
      signature        = ""
    }

    /AddressSanitizer:/           {if (sanitizer_tag == "no_stack") sanitizer_tag = "asan";  capture_frames = 1; next}
    /UndefinedBehaviorSanitizer:/ {if (sanitizer_tag == "no_stack") sanitizer_tag = "ubsan"; capture_frames = 1; next}
    /runtime error:/              {if (sanitizer_tag == "no_stack") sanitizer_tag = "ubsan"; capture_frames = 1; next}

    capture_frames==1 && /^[ \t]*#[0-9]+[ \t]+/ {
      raw_line = $0
      sub(/\r$/, "", raw_line)

      func_name    = ""
      base_file    = ""
      line_number  = ""

      if(match(raw_line, / ([^ ]+\.(c|cc|cpp|cxx|h|hpp|hh)):([0-9]+)/, file_with_line_match)){
        base_file   = file_with_line_match[1]
        line_number = file_with_line_match[3]
        gsub(/^.*\//, "", base_file)
      }
      else if(match(raw_line, / ([^ ]+\.(c|cc|cpp|cxx|h|hpp|hh))/, file_only_match)){
        base_file = file_only_match[1]
        gsub(/^.*\//, "", base_file)
      }

      if(match(raw_line, / in ([^ ]+)/, func_match)){
        func_name = func_match[1]
      }else if (match(raw_line, /^ *#[0-9]+ +([^ ]+)/, symbol_match)){
        func_name = symbol_match[1]
      }

      if(base_file != "" || func_name != ""){
        if(include_lines && line_number != "")
          signature = signature base_file ":" line_number "|" func_name ";"
        else
          signature = signature base_file "|" func_name ";"
        frame_count++
        if(frame_count >= max_frames) capture_frames = 0
      }
      next
    }

    capture_frames==1 && /^[ \t]*$/ { capture_frames = 0; next }

    END{
      if (signature == "") print sanitizer_tag ":no_stack";
      else                 print sanitizer_tag ":" signature;
    }
  '
}


normalize_bug(){
  local raw_bug norm_bug
  raw_bug="$(printf '%s' "${1:-unknown}" | tr '[:upper:]' '[:lower:]')"
  norm_bug="${raw_bug// /-}"; norm_bug="${norm_bug//_/-}"
  case "$norm_bug" in
    heap-buffer-overflow|heap-buffer-overrun) echo "heap-buffer-overflow" ;;
    stack-buffer-overflow|stack-buffer-overrun|dynamic-stack-buffer-overflow) echo "stack-buffer-overflow" ;;
    use-after-free|heap-use-after-free|stack-use-after-return) echo "use-after-free" ;;
    double-free) echo "double-free" ;;
    negative-size-param) echo "negative-size-param" ;;
    out-of-bounds|index-out-of-bounds|array-out-of-bounds) echo "out-of-bounds" ;;
    shift-out-of-bounds) echo "shift-out-of-bounds" ;;
    integer-overflow|signed-integer-overflow|unsigned-integer-overflow) echo "integer-overflow" ;;
    null|null-pointer|null-deref|null-dereference) echo "null" ;;
    misaligned-pointer|misalignment|alignment) echo "misaligned-pointer" ;;
    type-mismatch|bad-cast|invalid-cast) echo "type-mismatch" ;;
    vla-bound|vla|variable-length-array) echo "vla-bound" ;;
    object-size) echo "object-size" ;;
    division-by-zero) echo "division-by-zero" ;;
    segv|segfault) echo "segfault" ;;
    bus) echo "bus" ;;
    ill|sigill|illegal-instruction) echo "illegal-instruction" ;;
    ubsan) echo "ubsan" ;;
    *) echo "unknown" ;;
  esac
}

bug_type_from_stack_block(){
  local block="$1"
  local asan_bug ubsan_bug bug_raw
  
  asan_bug="$(printf '%s' "$block" | extract_asan_from_block  | tail -n 1 || true)"
  ubsan_bug="$(printf '%s' "$block" | extract_ubsan_from_block | tail -n 1 || true)"

  if [[ -n "$asan_bug" ]]; then
    bug_raw="$asan_bug"
  elif [[ -n "$ubsan_bug" ]]; then
    bug_raw="$ubsan_bug"
  else
    bug_raw="unknown"
  fi
  normalize_bug "$bug_raw"
}

#-----------------------------------------------------------
#                Coverage parsing helpers
#-----------------------------------------------------------

coverage_key_from_block(){
  awk '
    BEGIN { unique_key_count = 0 }

    /^[[:space:]]*[0-9]+:[0-9]+[[:space:]]*$/ {
      hit_id = $1
      sub(/:.*/, "", hit_id)
      hit_seen[hit_id] = 1
      next
    }

    END {
      for (hit_id in hit_seen) unique_keys_array[++unique_key_count] = hit_id

      for (i = 2; i <= unique_key_count; i++) {
        temp_key = unique_keys_array[i]
        j = i - 1
        while (j > 0 && unique_keys_array[j] > temp_key) {
          unique_keys_array[j + 1] = unique_keys_array[j]
          j--
        }
        unique_keys_array[j + 1] = temp_key
      }

      output_string = ""
      for (i = 1; i <= unique_key_count; i++) {
        output_string = output_string (i > 1 ? " " : "") unique_keys_array[i]
      }
      print output_string
    }'
}


coverage_similarity_percent(){
  local ids_a_str="$1"
  local ids_b_str="$2"

  local unique_a="" unique_b="" id

  for id in $ids_a_str; do
    [[ -n "$id" && " $unique_a " != *" $id "* ]] && unique_a="$unique_a $id"
  done

  for id in $ids_b_str; do
    [[ -n "$id" && " $unique_b " != *" $id "* ]] && unique_b="$unique_b $id"
  done

  local intersection=0 union=0

  for id in $unique_a; do
    [[ -z "$id" ]] && continue
    ((++union))
    [[ " $unique_b " == *" $id "* ]] && ((++intersection))
  done

  for id in $unique_b; do
    [[ -z "$id" ]] && continue
    [[ " $unique_a " == *" $id "* ]] || ((++union))
  done

  if (( union == 0 )); then
    echo 100
    return 0
  fi
  echo $(( intersection * 100 / union ))
}

#-----------------------------------------------------------
#                    Collect input files
#-----------------------------------------------------------

input_files=()

collect_input_files(){
  local dir="$1" recursive="$2"
  local maxdepth=()
  (( recursive )) || maxdepth=(-maxdepth 1)

  if [[ "$(resolve_realpath "$OUT_BASE")" == "$(resolve_realpath "$CRASH_DIR")" ]]; then
    find "$dir" "${maxdepth[@]}" \
      \( -type d \( -name 'stack_*' -o -name 'cov_*' -o -name 'group_*' -o -name 'triage' \) -prune \) -o \
      -type f \
        ! -name 'triage2_manifest.csv' \
        ! -name 'stack_reproduce.log' \
        ! -name 'coverage.log' \
        ! -name 'triage2.log' \
        ! -name 'README.txt' \
        ! -name 'triage.log' \
        ! -name 'manifest.csv' \
      -print0
    return
  fi

  local prune=()
  case "$OUT_BASE" in
    "$CRASH_DIR"/*) prune=(-path "$OUT_BASE" -prune -o) ;;
  esac

  find "$dir" "${maxdepth[@]}" \( "${prune[@]}" -type f \
    ! -name 'README.txt' \
    ! -name 'triage.log' \
    ! -name 'manifest.csv' \) -print0
}


while IFS= read -r -d '' f; do
  input_files+=("$f")
done < <(collect_input_files "$CRASH_DIR" "$RECURSIVE")

total_inputs=${#input_files[@]}
if (( total_inputs==0 )); then { LOG_ERROR "No files found in %s" "$CRASH_DIR"; exit 1; }; fi
LOG_INFO "Found %s files." "$total_inputs"

destination_roots=()
for crash_file in "${input_files[@]}"; do
  dest="$(destination_base_for_file "$crash_file")"
  present=0
  for d in "${destination_roots[@]}"; do
    if [[ "$d" == "$dest" ]]; then
      present=1
      break
    fi
  done
  if (( present == 0 )); then destination_roots+=("$dest"); fi
done

container_inputs=()
for crash_file in "${input_files[@]}"; do container_inputs+=("$(to_container_path "$crash_file")"); done

#-----------------------------------------------------------
#                    Container Runner
#-----------------------------------------------------------

timeout_to_ms(){
  local t="$1" num unit
  num="${t%%[!0-9]*}"
  unit="${t#$num}"
  case "$unit" in
    ""|"s")   echo $(( num * 1000 ));;
    "ms")     echo $(( num ));;
    "m")      echo $(( num * 60000 ));;
    "h")      echo $(( num * 3600000 ));;
    "d")      echo $(( num * 86400000 ));;
    *)        echo 2000;;
  esac
}

handle_runner_events(){
  local mode="$1" progress_label="$2"
  local -a start_times=()
  local end_time duration

  while IFS= read -r line; do
    case "$line" in
      EVENT*)
        IFS=$'\t' read -r tag event_type file_index total_files base_name <<< "$line"

        if [[ "$event_type" == "START" ]]; then
          start_times[$file_index]=$(date +%s)

          if ((VERBOSE)); then
            LOG_INFO_PLAIN "[%s] [%d/%d] (t=%s) %s: %s" \
            "$(date +%H:%M:%S)" "$file_index" "$total_files" "$DOCKER_TIMEOUT" "$progress_label" "$base_name"
          else
            [[ -t 2 ]] && LOG_DEFAULT "\r[%d/%d] (t=%s) %s: %s"  \
            "$file_index" "$total_files" "$DOCKER_TIMEOUT" "$progress_label" "$base_name"
          fi

        elif [[ $VERBOSE -eq 1 && "$event_type" == "END" && "$mode" == "stack" ]]; then
          end_time=$(date +%s)
          if [[ -n "${start_times[$file_index]:-}" ]]; then
            duration=$(( end_time - start_times[$file_index] ))
            LOG_INFO_PLAIN "[%s] End of iteration [%s/%s] %s, duration: %s\n\n" \
              "$(date +%H:%M:%S)" "$file_index" "$total_files" "$base_name" "$duration"
          else
            LOG_INFO "Missing start time for file index %s (%s)" \
              "$file_index" "$base_name"
          fi
        fi
        ;;
      *)
        printf "%s\n" "$line" >&2
        ;;
    esac
  done
}

run_container_loop(){
  local mode="$1" total_files="$2"
  local log_path progress_label inner_command

  case "$mode" in
    stack)
      log_path="$STACK_LOG"
      progress_label="Reproducing"
      inner_command='
        if timeout -k 2s "$DOCKER_TIMEOUT" "$TARGET_BIN" < "$input_file"; then
          exit_code=0
        else
          exit_code=$?
        fi
        printf "<<<EXIT>>> exit_code=%s\n" "$exit_code"
      '
      ;;
    coverage)
      log_path="$COVERAGE_LOG"
      progress_label="Collecting coverage"
      inner_command='
        afl-showmap -o - -m none -t "$SHOWMAP_TIMEOUT" -C -- "$TARGET_BIN" < "$input_file" 2>/dev/null || true
      '
      ;;
    *) LOG_ERROR "Invalid mode for container loop: %s" "$mode"; return 1 ;;
  esac

  docker run --rm -i \
    --user "$(id -u):$(id -g)" \
    --mount "type=bind,src=${HOST_DIR},dst=${WORK_DIR}" \
    -w "${WORK_DIR}" \
    -e "ASAN_OPTIONS=${ASAN_OPTS}" \
    -e "UBSAN_OPTIONS=${UBSAN_OPTS}" \
    -e "TARGET_BIN=$(to_container_path "$TARGET_BIN")" \
    -e "DOCKER_TIMEOUT=${DOCKER_TIMEOUT}" \
    -e "SHOWMAP_TIMEOUT=$(timeout_to_ms "$DOCKER_TIMEOUT")" \
    -e "LOG_PATH=$(to_container_path "$log_path")" \
    -e "TOTAL_FILES=$total_files" \
    -e "VERBOSE=$VERBOSE" \
    "$IMAGE" bash --noprofile --norc -c '
      set -euo pipefail
      : > "$LOG_PATH"
      file_index=0
      while IFS= read -r -d "" input_file; do
        file_index=$((file_index+1))
        base_name="${input_file##*/}"
        printf "EVENT\tSTART\t%d\t%d\t%s\n" "$file_index" "$TOTAL_FILES" "$base_name" >&2
        if [[ "${VERBOSE:-0}" -eq 1 ]]; then
          {
            printf "<<<BEGIN>>> base=%s path=%s\n" "$base_name" "$input_file"
            '"$inner_command"'
            printf "<<<END>>> base=%s\n" "$base_name"
          } 2>&1 | tee -a "$LOG_PATH" >&2
        else
          {
            printf "<<<BEGIN>>> base=%s path=%s\n" "$base_name" "$input_file"
            '"$inner_command"'
            printf "<<<END>>> base=%s\n" "$base_name"
          } >> "$LOG_PATH" 2>&1
        fi
        printf "EVENT\tEND\t%d\t%d\t%s\n" "$file_index" "$TOTAL_FILES" "$base_name" >&2
      done
    ' 2> >(handle_runner_events "$mode" "$progress_label")
}

#-----------------------------------------------------------
#            Stacktrace & Coverage Collection 
#-----------------------------------------------------------

set_map_value(){
  local keys_name="$1" vals_name="$2" key="$3" val="$4"
  local i len k
  eval "len=\${#$keys_name[@]}"
  for ((i=0; i<len; i++)); do
    eval "k=\${$keys_name[i]}"
    if [[ "$k" == "$key" ]]; then
      eval "$vals_name[i]=$(printf '%q' "$val")"
      return
    fi
  done
  eval "$keys_name[len]=$(printf '%q' "$key")"
  eval "$vals_name[len]=$(printf '%q' "$val")"
}

lookup_value(){
  local keys_name="$1" vals_name="$2" key="$3" outvar="$4"
  local i len k
  eval "len=\${#$keys_name[@]}"
  eval "$outvar=''"
  for ((i=0; i<len; i++)); do
    eval "k=\${$keys_name[i]}"
    if [[ "$k" == "$key" ]]; then
      eval "$outvar=\${$vals_name[i]}"
      return
    fi
  done
}

stack_signature_keys=(); stack_signature_vals=()
stack_source_keys=();    stack_source_vals=()
stack_frames_keys=();    stack_frames_vals=()
exitcode_keys=();        exitcode_vals=()
bug_type_keys=();        bug_type_vals=()
cov_key_keys=();         cov_key_vals=()
cov_hits_keys=();        cov_hits_vals=()


coverage_key_for_file(){
  local crash_file="$1"
  local block cov_key id count

  block="$(extract_block_by_file "$crash_file" "$COVERAGE_LOG" || true)"
  cov_key="$(coverage_key_from_block <<< "$block")"

  count=0
  for id in $cov_key; do
    [[ -n "$id" ]] && ((++count))
  done

  set_map_value cov_hits_keys cov_hits_vals "$crash_file" "$count"
  set_map_value cov_key_keys  cov_key_vals "$crash_file" "$cov_key"
  debug_log_key_value "COVERAGE" "file=$crash_file" "hits=$count" "key=$cov_key"
}

if [[ "$MODE" == "stack" || "$MODE" == "combined" ]]; then
  if [[ -n "$STAGE1_LOG" ]]; then
    LOG_INFO "Using Stage-1 log for stack parsing: %s\n" "$STAGE1_LOG"
  else
    LOG_DEFAULT "No Stage-1 log provided, reproducing crashes...\n\n"
    printf '%s\0' "${container_inputs[@]}" | run_container_loop "stack" "$total_inputs"
    STAGE1_LOG="$STACK_LOG"
    LOG_DEFAULT "done\n"
  fi
fi 
if [[ "$MODE" == "coverage" || "$MODE" == "combined" ]]; then
    LOG_DEFAULT "Collecting coverage...\n"
    printf '%s\0' "${container_inputs[@]}" | run_container_loop "coverage" "$total_inputs"
    LOG_DEFAULT "done\n"
fi

for crash_file in "${input_files[@]}"; do
  base_name="${crash_file##*/}"
  if [[ "$MODE" == "stack" || "$MODE" == "combined" ]]; then
      
    stack_block="$(extract_block_by_file "$crash_file" "$STAGE1_LOG" || true)"
    stack_sig_raw="$(derive_stack_signature_from_text <<< "$stack_block")"
    set_map_value stack_signature_keys stack_signature_vals "$crash_file" "$stack_sig_raw"
    set_map_value stack_source_keys    stack_source_vals    "$crash_file" "${stack_sig_raw%%:*}"
    set_map_value stack_frames_keys    stack_frames_vals    "$crash_file" "${stack_sig_raw#*:}"
    set_map_value exitcode_keys        exitcode_vals        "$crash_file" "$(extract_exitcode_from_block <<< "$stack_block")"
    set_map_value bug_type_keys        bug_type_vals        "$crash_file" "$(bug_type_from_stack_block "$stack_block")"
     
    lookup_value stack_source_keys stack_source_vals "$crash_file" stack_src_parsed
    lookup_value stack_frames_keys stack_frames_vals "$crash_file" stack_frames_parsed
    lookup_value bug_type_keys     bug_type_vals     "$crash_file" bug_normalized
    lookup_value exitcode_keys     exitcode_vals     "$crash_file" exit_code_parsed
    debug_log_key_value "STACK-PARSED" \
      "file=$crash_file" \
      "base=$base_name" \
      "source=${stack_src_parsed:-}" \
      "frames=${stack_frames_parsed:-}" \
      "bug=${bug_normalized:-unknown}" \
      "exit=${exit_code_parsed:-NA}"
  fi
  if [[ "$MODE" == "coverage" || "$MODE" == "combined" ]]; then
    cov_key=""
    coverage_key_for_file "$crash_file" 
    lookup_value cov_key_keys  cov_key_vals  "$crash_file" cov_key
    [[ -z "$cov_key" ]] && LOG_WARN "empty coverage key for %s" "$crash_file"
  fi
done


#-----------------------------------------------------------
#                     Manifest writer
#-----------------------------------------------------------

write_manifest_row(){
  local file_path="$1" group_key="$2" group_dir="$3"
  local crash_base; crash_base="${file_path##*/}"
  local group_name; group_name="${group_dir##*/}"
  local exit_code stack_source stack_frames cov_hits bug_type
  lookup_value exitcode_keys exitcode_vals "$file_path" exit_code
  lookup_value stack_source_keys stack_source_vals "$file_path" stack_source
  lookup_value stack_frames_keys stack_frames_vals "$file_path" stack_frames
  lookup_value cov_hits_keys    cov_hits_vals    "$file_path" cov_hits
  lookup_value bug_type_keys    bug_type_vals    "$file_path" bug_type

  printf '"%s","%s","%s","%s","%s","%s","%s","%s","%s"\n' \
    "$(escape_csv "$crash_base")" \
    "$(escape_csv "$group_key")" \
    "$(escape_csv "$group_dir")" \
    "$(escape_csv "$group_name")" \
    "$(escape_csv "${bug_type:-}")" \
    "${exit_code:-}" \
    "$(escape_csv "${stack_source:-}")" \
    "$(escape_csv "${stack_frames:-}")" \
    "$(escape_csv "${cov_hits:-}")" \
    >> "$MANIFEST"
}

#-----------------------------------------------------------
#                       Grouping
#-----------------------------------------------------------

assign_groups_by_exact_key(){ # "group_key<TAB>file_path"
  local destination_root="$1"
  mkdir -p "$destination_root"
  local -a group_keys=() group_dirs=()
  local group_counter=0 group_key file_path

  while IFS=$'\t' read -r group_key file_path; do
    [[ -z "$group_key" || -z "$file_path" ]] && continue
    local group_idx=-1 i
    for i in "${!group_keys[@]}"; do [[ "${group_keys[$i]}" == "$group_key" ]] && group_idx=$i && break; done
    if (( group_idx == -1 )); then
      group_counter=$((group_counter+1))
      group_idx=${#group_keys[@]}
      group_keys+=("$group_key")
      group_dirs+=("${destination_root%/}/stack_$(printf "%03d" "$group_counter")")

      debug_log_key_value "GROUP-NEW" "mode=stack" \
        "group=${destination_root%/}/stack_$(printf "%03d" "$group_counter")" \
        "rep=$group_key"
    fi

    debug_log_key_value "GROUP-ADD" "mode=stack" \
      "file=$file_path" \
      "group=${group_dirs[$group_idx]}" \
      "reason=stack-key-equal" \
      "key=$group_key"

    link_into_group_dir "$file_path" "${group_dirs[$group_idx]}"
    write_manifest_row "$file_path" "$group_key" "${group_dirs[$group_idx]}"
  done
}

assign_groups_by_coverage_threshold(){ # "coverage_key<TAB>file_path"
  local destination_root="$1"
  mkdir -p "$destination_root"
  local -a representative_keys=() group_dirs=()
  local group_counter=0 coverage_key file_path placed similarity i
  while IFS=$'\t' read -r coverage_key file_path; do
    [[ -z "$coverage_key" || -z "$file_path" ]] && continue
    placed=0
    # exact match 
    for i in "${!representative_keys[@]}"; do
      if [[ "$coverage_key" == "${representative_keys[$i]}" ]]; then

        debug_log_key_value "GROUP-ADD" "mode=cov" \
          "file=$file_path" \
          "group=${group_dirs[$i]}" \
          "reason=exact" \
          "key=$coverage_key"
        
        link_into_group_dir "$file_path" "${group_dirs[$i]}"
        write_manifest_row "$file_path" "$coverage_key" "${group_dirs[$i]}"
        placed=1; break
      fi
    done
    if (( placed==1 )); then continue; fi
    # <100
    if (( THRESHOLD < 100 )); then
      for i in "${!representative_keys[@]}"; do
        similarity="$(coverage_similarity_percent "$coverage_key" "${representative_keys[$i]}")"
        
        debug_log_key_value "COMPARE" "mode=cov" \
          "file=$file_path" \
          "key=$coverage_key" \
          "rep=${representative_keys[$i]}" \
          "sim=${similarity}" \
          "thresh=${THRESHOLD}" \
          "match=$([[ $similarity -ge $THRESHOLD ]] && echo yes || echo no)"
        
        if (( similarity >= THRESHOLD )); then

          debug_log_key_value "GROUP-ADD" "mode=cov" \
            "file=$file_path" \
            "group=${group_dirs[$i]}" \
            "reason=similarity" \
            "sim=${similarity}" \
            "thresh=${THRESHOLD}"

          link_into_group_dir "$file_path" "${group_dirs[$i]}"
          write_manifest_row "$file_path" "$coverage_key" "${group_dirs[$i]}"
          placed=1; break
        fi
      done
    fi
    if (( placed==1 )); then continue; fi
    # new group
    debug_log_key_value "COMPARE" "mode=cov" \
      "file=$file_path" \
      "key=$coverage_key" \
      "match=no" \
      "thresh=${THRESHOLD}"

    group_counter=$((group_counter+1))
    local new_group_dir="${destination_root%/}/cov_$(printf "%03d" "$group_counter")"
    mkdir -p "$new_group_dir"

    debug_log_key_value "GROUP-NEW" "mode=cov" \
      "group=$new_group_dir" \
      "rep=$coverage_key"

    link_into_group_dir "$file_path" "$new_group_dir"

    debug_log_key_value "GROUP-ADD" "mode=cov" \
      "file=$file_path" \
      "group=$new_group_dir" \
      "reason=new-group"

    write_manifest_row "$file_path" "$coverage_key" "$new_group_dir"
    representative_keys+=("$coverage_key")
    group_dirs+=("$new_group_dir")
  done
}

assign_groups_by_AND_flat(){
  local destination_root="$1"
  mkdir -p "$destination_root"
  local -a rep_stack=() rep_cov=() group_dirs=()
  local group_counter=0 stack_sig cov_key file_path placed similarity i

  while IFS=$'\t' read -r stack_sig cov_key file_path; do
    [[ -z "$file_path" ]] && continue
    placed=0
    for i in "${!rep_stack[@]}"; do
      stack_equal="no"
      if [[ "${rep_stack[$i]}" == "$stack_sig" ]]; then
        stack_equal="yes"
        similarity="$(coverage_similarity_percent "$cov_key" "${rep_cov[$i]}")"

        debug_log_key_value "COMPARE" "mode=combined" \
          "file=$file_path" \
          "stack_equal=$stack_equal" \
          "sim=${similarity}" \
          "thresh=${THRESHOLD}" \
          "rep_group=${group_dirs[$i]}"

        if (( similarity >= THRESHOLD )); then
        debug_log_key_value "GROUP-ADD" "mode=combined" \
            "file=$file_path" \
            "group=${group_dirs[$i]}" \
            "reason=stack-equal+similarity" \
            "sim=${similarity}"
          link_into_group_dir "$file_path" "${group_dirs[$i]}"
          write_manifest_row "$file_path" "${stack_sig} && cov=${cov_key}" "${group_dirs[$i]}"
          placed=1; break
        fi
      else
        debug_log_key_value "COMPARE" "mode=combined" \
          "file=$file_path" \
          "stack_equal=$stack_equal" \
          "rep_group=${group_dirs[$i]}" \
          "action=skip-similarity"
      fi
    done

    if (( placed==1 )); then continue; fi
    group_counter=$((group_counter+1))
    local new_group_dir="${destination_root%/}/group_$(printf "%03d" "$group_counter")"
    mkdir -p "$new_group_dir"

    debug_log_key_value "GROUP-NEW" "mode=combined" \
      "group=$new_group_dir" \
      "rep_stack=$stack_sig" \
      "rep_cov=$cov_key"

    link_into_group_dir "$file_path" "$new_group_dir"
    write_manifest_row "$file_path" "${stack_sig} && cov=${cov_key}" "$new_group_dir"

    debug_log_key_value "GROUP-ADD" "mode=combined" \
      "file=$file_path" \
      "group=$new_group_dir" \
      "reason=new-group"

    rep_stack+=("$stack_sig")
    rep_cov+=("$cov_key")
    group_dirs+=("$new_group_dir")
  done
}

case "$MODE" in
  stack)
    for destination_root in "${destination_roots[@]}"; do
      LOG_DEFAULT "Grouping by stack → %s\n" "$destination_root"
      {
        for crash_file in "${input_files[@]}"; do
          [[ "$(destination_base_for_file "$crash_file")" != "$destination_root" ]] && continue
          lookup_value stack_signature_keys stack_signature_vals "$crash_file" stack_sig
          printf '%s\t%s\n' "$stack_sig" "$crash_file"
        done
      } | assign_groups_by_exact_key "$destination_root"
    done
    ;;
  coverage)
    for destination_root in "${destination_roots[@]}"; do
      LOG_DEFAULT "Grouping by coverage (threshold=%s%%) → %s\n" "$THRESHOLD" "$destination_root"
      {
        for crash_file in "${input_files[@]}"; do
          [[ "$(destination_base_for_file "$crash_file")" != "$destination_root" ]] && continue
          lookup_value cov_key_keys     cov_key_vals     "$crash_file" cov_key
          printf '%s\t%s\n' "$cov_key" "$crash_file"
        done
      } | assign_groups_by_coverage_threshold "$destination_root"
    done
    ;;
combined)
    for destination_root in "${destination_roots[@]}"; do
      LOG_DEFAULT "Grouping combined (strict AND flat) → %s\n" "$destination_root"
      {
        for crash_file in "${input_files[@]}"; do
          [[ "$(destination_base_for_file "$crash_file")" != "$destination_root" ]] && continue
          lookup_value stack_signature_keys stack_signature_vals "$crash_file" stack_sig
          lookup_value cov_key_keys     cov_key_vals     "$crash_file" cov_key
          printf '%s\t%s\t%s\n' "$stack_sig" "$cov_key" "$crash_file"
        done
      } | assign_groups_by_AND_flat "$destination_root"
    done
    ;;
esac

#-----------------------------------------------------------
#                       Output validation
#-----------------------------------------------------------

if [[ "$MODE" == "stack" || "$MODE" == "combined" ]]; then
  all_no_stack=1
  for file in "${input_files[@]}"; do
    lookup_value stack_frames_keys stack_frames_vals "$file" frames_for_file
    if [[ -n "${frames_for_file:-}" && "${frames_for_file}" != "no_stack" ]]; then
      all_no_stack=0; break
    fi
  done
  if (( all_no_stack )); then
    LOG_WARN "No stack frames found for any input (got 'no_stack' everywhere)."
    LOG_WARN_PLAIN "  Hint: build the target with ASan/UBSan and ensure symbolization is enabled (ASAN_OPTIONS=symbolize=1)."
  fi
fi

if [[ "$MODE" == "coverage" || "$MODE" == "combined" ]]; then
  any_cov_hits=0
  for file in "${input_files[@]}"; do
    lookup_value cov_hits_keys cov_hits_vals "$file" hits_for_file
    if (( ${hits_for_file:-0} > 0 )); then
      any_cov_hits=1; break
    fi
  done
  if (( ! any_cov_hits )); then
    LOG_WARN "All coverage keys are empty."
    LOG_WARN_PLAIN "  Hint: run an AFL++-instrumented binary (afl-cc/afl-clang-fast)."
  fi
fi

LOG_DEFAULT "Stage 2 '%s' done → %s\n" "$MODE" "$OUT_BASE"
LOG_DEFAULT "Manifest: %s\n" "$MANIFEST"

if [[ -z "${STAGE1_LOG:-}" && ( "$MODE" == "stack" || "$MODE" == "combined" ) ]]; then
   [[ -f "$STACK_LOG" ]] && LOG_DEFAULT "Stack log: %s\n" "$STACK_LOG"
fi

if [[ "$MODE" == "coverage" || "$MODE" == "combined" ]]; then
   [[ -f "$COVERAGE_LOG" ]] && LOG_DEFAULT "Coverage log: %s\n" "$COVERAGE_LOG"
fi

if (( DEBUG_LOG )); then
  [[ -f "$LOG_FILE" ]] && LOG_DEFAULT "Debug log: %s\n" "$LOG_FILE"
fi