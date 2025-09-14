ROOT := ..
INCLUDE_DIRS := $(ROOT)/include $(ROOT)/include/http $(ROOT)/include/adapters \
                $(ROOT)/include/core $(ROOT)/include/router \
                $(ROOT)/include/filesystem $(ROOT)/ports/posix $(ROOT)/app
SRC_DIRS := $(ROOT)/src $(ROOT)/src/http $(ROOT)/src/adapters \
            $(ROOT)/src/core $(ROOT)/src/router $(ROOT)/src/filesystem \
            $(ROOT)/ports/posix $(ROOT)/app

BUILD_DIR := build
MODE ?=
OBJ_DIR := $(BUILD_DIR)/obj$(if $(MODE),-$(MODE),)
DEP_DIR := $(BUILD_DIR)/dep$(if $(MODE),-$(MODE),)
BIN_DIR   := $(BUILD_DIR)

INT_BIN  ?= $(BIN_DIR)/int_parse_harness

INT_HARNESS_SRC  := harness/int_parse_harness.c

CC := afl-clang-fast
CFLAGS := -Wall -Wextra -O1 -g -fno-omit-frame-pointer
DEPFLAGS := -MMD -MP
INCLUDES := $(foreach D,$(INCLUDE_DIRS),-I$(D))

CFILES_ALL := $(foreach D,$(SRC_DIRS),$(wildcard $(D)/*.c))
CFILES := $(filter-out $(ROOT)/src/main.c $(ROOT)/src/server.c,$(CFILES_ALL))

OBJECTS_FROM_C := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CFILES))
OBJECTS := $(patsubst $(ROOT)/%,%,$(OBJECTS_FROM_C))

INT_HARNESS_OBJ  := $(OBJ_DIR)/$(INT_HARNESS_SRC:.c=.o)

DEPFILES := $(patsubst $(ROOT)/%.c,$(DEP_DIR)/%.d,$(CFILES))

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
DICT_DIR := dicts
OUT_DIR := out

INT_DIR := int

SEEDS_INT  ?= $(SEEDS_DIR)/$(INT_DIR)

OUT_INT  ?= $(OUT_DIR)/$(INT_DIR)

DICT_INT   ?= $(DICT_DIR)/http_int.dict

TIMEOUT ?= 1000+

.PHONY: all clean build-parse-int  run-parse-int run-parse-unit-cmplog  internal-build-parse-integration

all: build-parse-int

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@) $(dir $(DEP_DIR)/$*)
	$(CC) $(CFLAGS) $(EXTRA_FLAGS) $(INCLUDES) $(DEPFLAGS) -MF $(DEP_DIR)/$*.d -c $< -o $@


$(INT_BIN): $(OBJECTS) $(INT_HARNESS_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC_CMD) -v $^ -o $@

-include $(DEPFILES)

clean:
	rm -rf $(BUILD_DIR)

build-parse-int:
	$(DOCKER_RUN) bash -lc '\
	  EXTRA_FLAGS="-fsanitize=address,undefined" MODE="" \
	  make -f Makefile internal-build-parse-integration '

internal-build-parse-integration: $(INT_BIN)

run-parse-int: build-parse-int
	$(DOCKER_RUN) bash -lc '\
	    afl-fuzz -i $(SEEDS_INT) -o $(OUT_INT) -m none -t $(TIMEOUT) -x $(DICT_INT) -- \
	    ./build/int_parse_harness '

run-parse-int-cmplog: build-parse-int
	$(DOCKER_RUN) bash -lc '\
	    AFL_USE_ASAN=1 AFL_LLVM_ONLY_FSRV=1 AFL_LLVM_CMPLOG=1 EXTRA_FLAGS="" MODE=cmplog\
	    make -f Makefile internal-build-parse-integration INT_BIN=build/int_parse_harness_cmplog && \
	    afl-fuzz -i $(SEEDS_INT) -o $(OUT_INT) -m none -t $(TIMEOUT) -x $(DICT_INT) \
	    -c ./build/int_parse_harness_cmplog -- ./build/int_parse_harness'

