#!/usr/bin/env bash

set -euo pipefail

# ---------- Defaults ----------
LINK_MODE="${LINK_MODE:-hard}"  # hard | sym | copy
RECURSIVE=0
VERBOSE=0
DOCKER_TIMEOUT="${DOCKER_TIMEOUT:-20s}"

IMAGE="${IMAGE:-aflplusplus/aflplusplus}"
WORK_DIR="${WORK_DIR:-/work}"
HOST_DIR="${HOST_DIR:-$(pwd)}"
OUT_BASE="${OUT_BASE:-triage/stage1}"

ASAN_OPTS_DEFAULT='abort_on_error=1:detect_leaks=0:symbolize=1:allocator_may_return_null=1'
UBSAN_OPTS_DEFAULT='print_stacktrace=1'
ASAN_OPTS="${ASAN_OPTIONS:-$ASAN_OPTS_DEFAULT}"
UBSAN_OPTS="${UBSAN_OPTIONS:-$UBSAN_OPTS_DEFAULT}"

show_help(){
  cat <<'EOF'
Usage:
  ./triage1.sh [options] <crash_dir> <binary>

Options:
  --verbose (-v)               Verbose mode: show logs during execution
  --recursive (-r)             Recurse into <crash_dir>
  --timeout (-t) <dur>         Timeout per crash (default: 20s)
  --output (-o) <dir>          Override output directory (default: ./triage/stage1)
  --link (-l) {hard|sym|copy}  File linking method into result buckets (default: hard)
  --image (-i) <img>           Docker image (default: aflplusplus/aflplusplus)
  --hostdir (-H) <dir>         Host directory to mount into container (default: $PWD)
  --workdir (-w) <dir>         Container workdir mount point (default: /work)
  --asan-opts (-a) "<opts>"    Override ASAN_OPTIONS at runtime (colon-separated)
  --ubsan-opts (-u) "<opts>"   Override UBSAN_OPTIONS at runtime (colon-separated)
  --help (-h)                  Show this help

Example:
  ./triage1.sh -v --link sym \
    --asan-opts "abort_on_error=1:detect_leaks=0" \
    out/crashes/ build/my_target_harness

Output:
  <output>/triage.log
  <output>/manifest.csv
  <output>/<HIGH|MEDIUM|LOW|no-crash>/<bug_type>/*
EOF
}

# ---------- CLI parsing ----------
args=()
while [[ $# -gt 0 ]]; do
  case "${1:-}" in
    -v|--verbose)    VERBOSE=1; shift ;;
    -r|--recursive)  RECURSIVE=1; shift ;;
    -t|--timeout)    DOCKER_TIMEOUT="${2:?}"; shift 2 ;;
    -o|--output)     OUT_BASE="${2:?}"; shift 2 ;;
    -l|--link)       LINK_MODE="${2:-hard}"; shift 2 ;;
    -i|--image)      IMAGE="${2:?}"; shift 2 ;;
    -H|--hostdir)    HOST_DIR="${2:?}"; shift 2 ;;
    -w|--workdir)    WORK_DIR="${2:?}"; shift 2 ;;
    -a|--asan-opts)  ASAN_OPTS="${2:?}"; shift 2 ;;
    -u|--ubsan-opts) UBSAN_OPTS="${2:?}"; shift 2 ;;
    -h|--help)       show_help; exit 0 ;;
    --)              shift; break ;;
    -*)              printf "Unknown option: %s\n" $1 >&2; show_help; exit 1 ;;
    *)               args+=("$1"); shift ;;
  esac
done
set -- "${args[@]}"

CRASH_DIR="${1:-}"
TARGET_BIN="${2:-}"
[[ -z "$CRASH_DIR" || -z "$TARGET_BIN" ]] && { show_help; exit 1; }

# ---------- Setup ----------
mkdir -p "$OUT_BASE" || {
  printf "Error: Cannot create output dir: %s\n" $OUT_BASE >&2
  exit 1
}
SINGLE_LOG="${OUT_BASE}/triage.log"
: > "${SINGLE_LOG}"

MANIFEST="${OUT_BASE}/manifest.csv"
echo 'crashfile,detector,priority,bug_type,asan_summary,ubsan_summary,exitcode,repro_cmd' > "${MANIFEST}"

# ---------- Helper ---------- 
escape_csv(){ printf '%s' "$1" | sed 's/"/""/g'; }

sanitize_dirname(){ printf '%s' "$1" | tr -c 'A-Za-z0-9._@+-' '_' | cut -c1-120; }

to_container_path(){  
  local host_path="$1"
  local rel="${host_path#"$HOST_DIR"/}"
  printf '%s/%s' "$WORK_DIR" "$rel"
}

absolute_path(){
  case "$1" in
    /*) printf '%s\n' "$1" ;;
    *)  printf '%s/%s\n' "$PWD" "$1" ;;
  esac
}

link_file(){
  local src="$1" dst_dir="$2"
  mkdir -p "$dst_dir"
  local base dst
  base="${src##*/}"
  dst="${dst_dir%/}/${base}"

  if [[ -e "$dst" ]]; then
    [[ "${VERBOSE:-0}" -eq 1 ]] && printf "Skipping already linked: %s\n" $dst
    return
  fi

  case "${LINK_MODE}" in
    hard) ln "$src" "$dst" 2>/dev/null || cp -p "$src" "$dst" ;;
    sym)  ln -s "$(absolute_path "$src")" "$dst" 2>/dev/null || cp -p "$src" "$dst" ;;
    copy|*) cp -p "$src" "$dst" ;;
  esac
}

normalize_bug(){
  local x; x="$(printf "%s" ${1:-unknown} | tr '[:upper:]' '[:lower:]')"
  x="${x// /-}"; x="${x//_/-}"
  case "$x" in
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

priority_of_bug(){
  local norm sum_low
  norm="$(normalize_bug "${1:-unknown}")"
  sum_low="$(printf "%s" ${2:-} | tr '[:upper:]' '[:lower:]')"
  case "$norm" in
    heap-buffer-overflow|stack-buffer-overflow|use-after-free|double-free|negative-size-param)
      echo "HIGH" ;;
    out-of-bounds)
      if [[ "$sum_low" =~ (write|overflow) ]]; then echo "HIGH"; else echo "MEDIUM"; fi ;;
    shift-out-of-bounds|integer-overflow|misaligned-pointer|type-mismatch|vla-bound|object-size)
      echo "MEDIUM" ;;
    null|division-by-zero|segfault|bus|illegal-instruction|ubsan|unknown)
      echo "LOW" ;;
    *) echo "LOW" ;;
  esac
}

extract_asan_from_block(){
  awk '
    /ERROR:[[:space:]]*AddressSanitizer:/ {
      sub(/\r/,""); summary=$0; bug="unknown";
      if (match($0,/AddressSanitizer[[:space:]]*:[[:space:]]*([A-Za-z0-9_+-]+)/,m)) bug=m[1];
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
  awk -F'[ =]+' '/^<<<EXIT>>>/ {exit_code=$NF} END{ if(exit_code=="") exit_code=124; print exit_code }'
}

extract_block() {
  local base="$1"
  awk -v b="$base" '
    $0 ~ "^<<<BEGIN>>> base=" b "( |$)" { inside_block=1; block_content=""; next }
    inside_block { block_content = block_content $0 ORS; }
    $0 ~ "^<<<END>>> base=" b "( |$)" { inside_block=0; found=1; print block_content; exit }
    END { if (!found) printf "[!] WARN: No block for %s\n", b > "/dev/stderr" }
  ' "$SINGLE_LOG"
}


find_args=(-type f)
if [[ "$RECURSIVE" != "1" ]]; then
  find_args=(-maxdepth 1 -type f)
fi


files=()
while IFS= read -r -d '' file; do
  files+=("$file")
done < <(find "$CRASH_DIR" "${find_args[@]}" ! -name 'README.txt' -print0)
total=${#files[@]}


# ---------- Reproduce (Docker runner) ---------- 

printf '%s\0' "${files[@]}" | docker run --rm -i \
  --user "$(id -u):$(id -g)" \
  --mount "type=bind,src=${HOST_DIR},dst=${WORK_DIR}" \
  -w "${WORK_DIR}" \
  -e "ASAN_OPTIONS=${ASAN_OPTS}" \
  -e "UBSAN_OPTIONS=${UBSAN_OPTS}" \
  -e "TRIAGE_BIN=$(printf %q "$(to_container_path "$TARGET_BIN")")" \
  -e "TRIAGE_TIMEOUT=${DOCKER_TIMEOUT}" \
  -e "TRIAGE_LOG=$(printf %q "$(to_container_path "$SINGLE_LOG")")" \
  -e "TRIAGE_TOTAL=${total}" \
  -e "VERBOSE=${VERBOSE}" \
  "$IMAGE" bash -lc '
    set -euo pipefail
    : "${TRIAGE_BIN:?}"; : "${TRIAGE_TIMEOUT:?}"; : "${TRIAGE_LOG:?}"; : "${TRIAGE_TOTAL:?}"
    : > "$TRIAGE_LOG"
    idx=0
    while IFS= read -r -d "" crash_file; do
      idx=$((idx+1))
      base="${crash_file##*/}"

      if [[ "${VERBOSE:-0}" -eq 1 ]]; then
        printf "[$(date +%H:%M:%S)] [%d/%d] (t="${TRIAGE_TIMEOUT:?}s") Reproducing: %s\n" "$idx" "$TRIAGE_TOTAL" "$base"
      else
        printf "\r[%d/%d] (t="${TRIAGE_TIMEOUT:?}s") Reproducing: %s" "$idx" "$TRIAGE_TOTAL" "$base"
      fi

      printf "<<<BEGIN>>> base=%s path=%s\n" "$base" "$crash_file" >> "$TRIAGE_LOG"

      start_time=$(date +%s)
      if [[ "${VERBOSE:-0}" -eq 1 ]]; then
        if timeout -k 2s "$TRIAGE_TIMEOUT" sh -c "./${TRIAGE_BIN#"$PWD"/} < \"$crash_file\"" 2>&1 | tee -a "$TRIAGE_LOG"; then
          exit_code=0
        else
          exit_code=$?
        fi
      else
        if timeout -k 2s "$TRIAGE_TIMEOUT" sh -c "./${TRIAGE_BIN#"$PWD"/} < \"$crash_file\"" >> "$TRIAGE_LOG" 2>&1; then
          exit_code=0
        else
          exit_code=$?
        fi
      fi
      end_time=$(date +%s)
      elapsed=$((end_time - start_time))

      if [[ "${VERBOSE:-0}" -eq 1 ]]; then
        printf "<<<EXIT>>> exit_code=%s\n" "$exit_code" | tee -a "$TRIAGE_LOG"
        printf "<<<DURATION>>> %s seconds\n" "$elapsed"
        printf "\n"
      else
        printf "<<<EXIT>>> exit_code=%s\n" "$exit_code" >> "$TRIAGE_LOG"
      fi

      printf "<<<END>>> base=%s\n" "$base" >> "$TRIAGE_LOG"
    done
  '

# ---------- Parse & classify (Host) ----------

for ((i=0; i<total; i++)); do
  crash_file="${files[i]}"
  base="${crash_file##*/}"

    if [[ "${VERBOSE:-0}" -eq 1 ]]; then
        printf '[%d/%d] Parsing: %s\n' "$((i+1))" "$total" "$base"
    else
        printf '\r[%d/%d] Parsing: %s' "$((i+1))" "$total" "$base"
    fi


  block="$(extract_block "$base")"
  docker_exit_code="$(printf '%s' "$block" | extract_exitcode_from_block)"

  asan_fields=()
  while IFS= read -r line; do
    asan_fields+=("$line")
  done < <(printf '%s' "$block" | extract_asan_from_block || true)  
  asan_summary="${asan_fields[0]:-}"; asan_bug="${asan_fields[1]:-unknown}"

  ubsan_fields=()
  while IFS= read -r line; do 
    ubsan_fields+=("$line"); 
  done < <(printf '%s' "$block" | extract_ubsan_from_block || true)
  ubsan_summary="${ubsan_fields[0]:-}"; ubsan_bug="${ubsan_fields[1]:-unknown}"

  detector="none"; bug="unknown"; summary_for_prio=""
  if [[ -n "${asan_bug:-}" && "$asan_bug" != "unknown" ]]; then
    detector="asan"; bug="$asan_bug"; summary_for_prio="$asan_summary"
  elif [[ -n "${ubsan_bug:-}" && "$ubsan_bug" != "unknown" ]]; then
    detector="ubsan"; bug="$ubsan_bug"; summary_for_prio="$ubsan_summary"
  elif [[ "${docker_exit_code:-1}" -eq 124 ]]; then
    detector="timeout"; bug="hang"
  else
    detector="none"; bug="no-crash"
  fi

  norm_bug="$(normalize_bug "$bug")"
  if [[ "$detector" == "asan" || "$detector" == "ubsan" ]]; then
    prio="$(priority_of_bug "$norm_bug" "$summary_for_prio")"
  elif [[ "$detector" == "timeout" ]]; then
    prio="LOW"
  else
    prio="LOW"
  fi

  if [[ "$detector" == "none" ]]; then
    dst="${OUT_BASE}/no-crash"
  else
    dst="${OUT_BASE}/${prio}/$(sanitize_dirname "$norm_bug")"
  fi
  link_file "$crash_file" "$dst"

  repro="./${TARGET_BIN} < ${CRASH_DIR%/}/$base"
  printf '"%s","%s","%s","%s","%s","%s","%s","%s"\n' \
    "$base" "$detector" "$prio" "$norm_bug" \
    "$(escape_csv "${asan_summary:-}")" "$(escape_csv "${ubsan_summary:-}")" \
    "${docker_exit_code:-}" "$(escape_csv "$repro")" \
    >> "${MANIFEST}"
done

printf "\n"

printf " - Stage1 complete → %s\n" "${OUT_BASE}"
printf " - Log:      %s\n" "${SINGLE_LOG}"
printf " - Manifest: %s\n" "${MANIFEST}"
