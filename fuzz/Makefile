ROOT := ..
INCLUDE_DIRS := $(ROOT)/include $(ROOT)/include/http $(ROOT)/include/adapters \
                $(ROOT)/include/core $(ROOT)/include/router \
                $(ROOT)/include/filesystem $(ROOT)/ports/posix $(ROOT)/app

SRC_DIRS     := $(ROOT)/src $(ROOT)/src/http $(ROOT)/src/adapters \
            	$(ROOT)/src/core $(ROOT)/src/router $(ROOT)/src/filesystem \
            	$(ROOT)/ports/posix $(ROOT)/app

BUILD_DIR := build
MODE ?=
OBJ_DIR := $(BUILD_DIR)/obj$(if $(MODE),-$(MODE),)
DEP_DIR := $(BUILD_DIR)/dep$(if $(MODE),-$(MODE),)
BIN_DIR := $(BUILD_DIR)

UNIT_BIN ?= $(BIN_DIR)/unit_parse_harness
INT_BIN  ?= $(BIN_DIR)/int_parse_harness
E2E_BIN  ?= $(BIN_DIR)/e2e_server_harness

UNIT_HARNESS_SRC := harness/unit_parse_harness.c
INT_HARNESS_SRC  := harness/int_parse_harness.c
E2E_HARNESS_SRC  := harness/e2e_server_harness.c

CC := afl-cc --afl-llvm
CFLAGS ?= -Wall -Wextra -O1 -g -fno-omit-frame-pointer
CFLAGS_CMPLOG ?= -O3 -g0
DEPFLAGS := -MMD -MP

AFL_ENVS 		?= AFL_USE_ASAN=1 AFL_USE_UBSAN=1
AFL_ENVS_CMPLOG ?= AFL_LLVM_CMPLOG=1 AFL_USE_ASAN=0 AFL_USE_UBSAN=0

INCLUDES := $(foreach D,$(INCLUDE_DIRS),-I$(D))

CFILES_ALL := $(foreach D,$(SRC_DIRS),$(wildcard $(D)/*.c))
CFILES     := $(filter-out $(ROOT)/src/main.c,$(CFILES_ALL))

OBJECTS  := $(patsubst $(ROOT)/%.c,$(OBJ_DIR)/%.o,$(CFILES))
DEPFILES := $(patsubst $(ROOT)/%.c,$(DEP_DIR)/%.d,$(CFILES))

UNIT_HARNESS_OBJ := $(OBJ_DIR)/$(UNIT_HARNESS_SRC:.c=.o)
INT_HARNESS_OBJ  := $(OBJ_DIR)/$(INT_HARNESS_SRC:.c=.o)
E2E_HARNESS_OBJ  := $(OBJ_DIR)/$(E2E_HARNESS_SRC:.c=.o)

HARNESS_DEPFILES := $(addprefix $(DEP_DIR)/, \
					$(UNIT_HARNESS_SRC:.c=.d) \
                    $(INT_HARNESS_SRC:.c=.d) \
                    $(E2E_HARNESS_SRC:.c=.d))

CC_CMD = $(CC) $(CFLAGS) $(EXTRA_FLAGS)

IMG ?= aflplusplus/aflplusplus
WORKDIR := /work
RAMDISK := /ramdisk
DOCKER_RUN := docker run --rm -it \
  --user $(shell id -u):$(shell id -g) \
  --mount type=bind,src="$(abspath $(ROOT))",dst=$(WORKDIR) \
  --mount type=tmpfs,destination=$(RAMDISK) \
  -e AFL_TMPDIR=$(RAMDISK) \
  -w $(WORKDIR)/fuzz $(IMG)

SEEDS_DIR := seeds
DICT_DIR  := dicts
OUT_DIR   := out

UNIT_DIR := unit
INT_DIR  := int
E2E_DIR  := e2e

SEEDS_UNIT ?= $(SEEDS_DIR)/$(UNIT_DIR)
SEEDS_INT  ?= $(SEEDS_DIR)/$(INT_DIR)
SEEDS_E2E  ?= $(SEEDS_DIR)/$(E2E_DIR)

OUT_UNIT ?= $(OUT_DIR)/$(UNIT_DIR)
OUT_INT  ?= $(OUT_DIR)/$(INT_DIR)
OUT_E2E  ?= $(OUT_DIR)/$(E2E_DIR)

DICT  ?= $(DICT_DIR)/http.dict 

TIMEOUT ?= 1000+

.PHONY: all clean \
		build-parse-int build-parse-unit build-server-e2e \
		run-parse-unit run-parse-int run-server-e2e \
		run-parse-unit-cmplog run-parse-int-cmplog run-server-e2e-cmplog \
        internal-build-parse-unit internal-build-parse-integration internal-build-parse-end-to-end

all: build-parse-unit build-parse-int build-server-e2e

$(OBJ_DIR)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@) $(dir $(DEP_DIR)/$*)
	$(CC) $(CFLAGS) $(INCLUDES) \
	$(DEPFLAGS) -MF $(patsubst $(ROOT)/%.c,$(DEP_DIR)/%.d,$<) \
	-c $< -o $@

$(OBJ_DIR)/harness/%.o: harness/%.c
	@mkdir -p $(dir $@) $(dir $(patsubst $(OBJ_DIR)/%.o,$(DEP_DIR)/%.d,$@))
	$(CC) $(CFLAGS) $(INCLUDES) \
	$(DEPFLAGS) -MF $(patsubst $(OBJ_DIR)/%.o,$(DEP_DIR)/%.d,$@) \
	  -c $< -o $@

$(UNIT_BIN): $(OBJECTS) $(UNIT_HARNESS_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC_CMD) $^ -o $@

$(INT_BIN): $(OBJECTS) $(INT_HARNESS_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC_CMD) $^ -o $@

$(E2E_BIN): $(OBJECTS) $(E2E_HARNESS_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC_CMD) $^ -o $@

-include $(DEPFILES) $(HARNESS_DEPFILES)

clean:
	rm -rf $(BUILD_DIR)

build-parse-unit:
	$(DOCKER_RUN) bash -lc '\
	$(AFL_ENVS) \
	make -f Makefile internal-build-parse-unit '

build-parse-int:
	$(DOCKER_RUN) bash -lc '\
	$(AFL_ENVS) \
	make -f Makefile internal-build-parse-integration '

build-server-e2e:
	$(DOCKER_RUN) bash -lc '\
	$(AFL_ENVS) \
	make -f Makefile internal-build-parse-end-to-end '

internal-build-parse-unit: $(UNIT_BIN)
internal-build-parse-integration: $(INT_BIN)
internal-build-parse-end-to-end: $(E2E_BIN)

run-parse-unit: build-parse-unit
	$(DOCKER_RUN) bash -lc '\
	  afl-fuzz -i $(SEEDS_UNIT) -o $(OUT_UNIT) -m none -t $(TIMEOUT) -x $(DICT) -- \
	    ./$(UNIT_BIN) '

run-parse-int: build-parse-int
	$(DOCKER_RUN) bash -lc '\
	  afl-fuzz -i $(SEEDS_INT) -o $(OUT_INT) -m none -t $(TIMEOUT) -x $(DICT) -- \
	    ./$(INT_BIN) '

run-server-e2e: build-server-e2e
	$(DOCKER_RUN) bash -lc '\
	  afl-fuzz -i $(SEEDS_E2E) -o $(OUT_E2E) -m none -t $(TIMEOUT) -x $(DICT) -- \
	    ./$(E2E_BIN) '


CMPLOG_SUFFIX ?= _cmplog
UNIT_BIN_CMPLOG ?= $(UNIT_BIN)$(CMPLOG_SUFFIX)
INT_BIN_CMPLOG  ?= $(INT_BIN)$(CMPLOG_SUFFIX)
E2E_BIN_CMPLOG  ?= $(E2E_BIN)$(CMPLOG_SUFFIX)

run-parse-unit-cmplog: build-parse-unit
	$(DOCKER_RUN) bash -lc '\
	    $(AFL_ENVS_CMPLOG)  CFLAGS="$(CFLAGS_CMPLOG)" MODE=cmplog\
		make -f Makefile internal-build-parse-unit UNIT_BIN=build/unit_parse_harness_cmplog && \
		afl-fuzz -i $(SEEDS_UNIT) -o $(OUT_UNIT) -m none -t $(TIMEOUT) -x $(DICT) \
	    -c ./$(UNIT_BIN_CMPLOG) -- ./$(UNIT_BIN)'


run-parse-int-cmplog: build-parse-int
	$(DOCKER_RUN) bash -lc '\
	    $(AFL_ENVS_CMPLOG) CFLAGS="$(CFLAGS_CMPLOG)" MODE=cmplog\
	    make -f Makefile internal-build-parse-integration INT_BIN=build/int_parse_harness_cmplog && \
	    afl-fuzz -i $(SEEDS_INT) -o $(OUT_INT) -m none -t $(TIMEOUT) -x $(DICT) \
	    -c ./$(INT_BIN_CMPLOG) -- ./$(INT_BIN)'


run-server-e2e-cmplog: build-server-e2e
	$(DOCKER_RUN) bash -lc '\
	    $(AFL_ENVS_CMPLOG) CFLAGS="$(CFLAGS_CMPLOG)" MODE=cmplog\
	    make -f Makefile internal-build-parse-end-to-end E2E_BIN=build/e2e_server_harness_cmplog && \
	    afl-fuzz -i $(SEEDS_E2E) -o $(OUT_E2E) -m none -t $(TIMEOUT) -x $(DICT) \
	    -c ./$(E2E_BIN_CMPLOG) -- ./$(E2E_BIN)'
