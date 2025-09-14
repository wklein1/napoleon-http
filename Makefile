BIN_NAME := napoleon_httpd
INCLUDE_DIRS := include/ include/http/ include/app/ include/adapters/ include/core/ \
			 include/router/ include/filesystem include/redirects ports/posix/
SRC_DIRS := src src/http src/adapters src/core/ src/router/ src/filesystem ports/posix app 
BUILD_DIR := build

BUILD_MODE = debug

OUT_DIR = $(BUILD_DIR)/$(BUILD_MODE)
OBJ_DIR = $(OUT_DIR)/obj
DEP_DIR = $(OUT_DIR)/dep
BIN_DIR = $(OUT_DIR)

BIN = $(OUT_DIR)/$(BIN_NAME)

CC = gcc
CFLAGS = -Wall -Wextra
OPT_DEBUG := -O0
OPT_RELEASE := -O3
DEPFLAGS := -MMD -MP

CC_CMD = $(CC) $(CFLAGS) $(foreach D, $(INCLUDE_DIRS), -I$(D))

CFILES := $(foreach D, $(SRC_DIRS), $(wildcard $(D)/*.c))
OBJECTS =$ (patsubst %.c,$(OBJ_DIR)/%.o,$(CFILES))
DEPFILES =$ (patsubst %.c,$(DEP_DIR)/%.d,$(CFILES))

DOXYGEN ?= doxygen
DOXYFILE ?= doxygen.txt
DOCS_OUT_DIR := docs/doxygen

args ?=
ARGS ?= $(args)

quiet ?= 1
QUIET ?= $(quiet)

.PHONY: all debug release clean run docs clean-docs

all: debug

debug: BUILD_MODE=debug
debug: CFLAGS += $(OPT_DEBUG) -g -DDEBUG
debug: $(BIN)

release: BUILD_MODE=release 
release: CFLAGS += $(OPT_RELEASE)
release: $(BIN)


clean: 
	rm -rf $(BUILD_DIR)

run: 
	./$(BIN) $(ARGS)

docs:
	@echo "Generating Doxygen HTML into $(DOCS_OUT_DIR) (make quiet=0 to see cmd output)"
ifeq ($(QUIET),1)
	@$(DOXYGEN) $(DOXYFILE) > /dev/null 2>&1
else
	@$(DOXYGEN) $(DOXYFILE)
endif

clean-docs:
	rm -rf $(DOCS_OUT_DIR) 

$(BIN):$(OBJECTS)
	@mkdir -p $(BIN_DIR)
	@$(CC_CMD) -o $@ $(OBJECTS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@mkdir -p $(DEP_DIR)/$(dir $*)
	@$(CC_CMD) -c $< -o $@ $(DEPFLAGS) -MF $(DEP_DIR)/$*.d

-include $(DEPFILES)

