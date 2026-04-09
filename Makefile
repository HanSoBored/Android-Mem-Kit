# Android-Mem-Kit Makefile
# Built for Android Security Research

# ============================================================================
# Configuration
# ============================================================================

NDK_HOME ?= $(ANDROID_NDK_HOME)
ifeq ($(NDK_HOME),)
  $(error ANDROID_NDK_HOME or NDK_HOME environment variable must be set)
endif

ANDROID_ABI ?= arm64-v8a
ANDROID_PLATFORM ?= android-35
BUILD_DIR = build/$(ANDROID_ABI)
OBJ_DIR = $(BUILD_DIR)/obj
LIB_DIR = $(BUILD_DIR)/lib

HOST_TAG = linux-x86_64
TOOLCHAIN = $(NDK_HOME)/toolchains/llvm/prebuilt/$(HOST_TAG)/bin

ifeq ($(ANDROID_ABI),arm64-v8a)
  TARGET = aarch64-linux-android
  SH_ARCH = arm64
else ifeq ($(ANDROID_ABI),armeabi-v7a)
  TARGET = armv7a-linux-androideabi
  SH_ARCH = arm
else
  $(error Unsupported ABI: $(ANDROID_ABI))
endif

CC = $(TOOLCHAIN)/$(TARGET)$(subst android-,,$(ANDROID_PLATFORM))-clang
AR = $(TOOLCHAIN)/llvm-ar
STRIP = $(TOOLCHAIN)/llvm-strip

# ============================================================================
# Colors & Fancy Output
# ============================================================================

WHITE  := \033[1;37m
GREEN  := \033[1;32m
YELLOW := \033[1;33m
RESET  := \033[0m

# Function to print progress
# Args: $(1) Action, $(2) Target, $(3) Percentage
define print_progress
	@printf "$(WHITE)[%3d%%] $(GREEN)%s $(WHITE)%s$(RESET)\n" $(3) $(1) $(2)
endef

# ============================================================================
# Flags & Sources
# ============================================================================

CFLAGS = -Wall -Wextra -fPIC -O2 -std=c11 -Wno-macro-redefined
INCLUDES = -Iinclude \
           -Ideps/xdl/xdl/src/main/cpp/include \
           -Ideps/shadowhook/shadowhook/src/main/cpp/include \
           -Ideps/shadowhook/shadowhook/src/main/cpp \
           -Ideps/shadowhook/shadowhook/src/main/cpp/common \
           -Ideps/shadowhook/shadowhook/src/main/cpp/arch/$(SH_ARCH) \
           -Ideps/shadowhook/shadowhook/src/main/cpp/third_party/bsd \
           -Ideps/shadowhook/shadowhook/src/main/cpp/third_party/xdl \
           -Ideps/shadowhook/shadowhook/src/main/cpp/third_party/lss

LDFLAGS = -shared -llog -landroid

MEMKIT_SRCS = src/memory.c src/hooking.c src/hooking_flags.c src/intercept.c src/records.c src/runtime_config.c src/dl_callbacks.c src/il2cpp.c src/xdl_wrapper.c
SH_DIR = deps/shadowhook/shadowhook/src/main/cpp
SH_SRCS = $(SH_DIR)/arch/$(SH_ARCH)/sh_inst.c \
          $(SH_DIR)/arch/$(SH_ARCH)/sh_glue.S \
          $(SH_DIR)/common/bytesig.c \
          $(SH_DIR)/common/sh_errno.c \
          $(SH_DIR)/common/sh_log.c \
          $(SH_DIR)/common/sh_ref.c \
          $(SH_DIR)/common/sh_trampo.c \
          $(SH_DIR)/common/sh_util.c \
          $(SH_DIR)/nothing/sh_nothing.c \
          $(SH_DIR)/sh_elf.c \
          $(SH_DIR)/sh_enter.c \
          $(SH_DIR)/sh_hub.c \
          $(SH_DIR)/sh_island.c \
          $(SH_DIR)/sh_jni.c \
          $(SH_DIR)/sh_linker.c \
          $(SH_DIR)/sh_recorder.c \
          $(SH_DIR)/sh_safe.c \
          $(SH_DIR)/sh_switch.c \
          $(SH_DIR)/sh_task.c \
          $(SH_DIR)/sh_xdl.c \
          $(SH_DIR)/shadowhook.c \
          $(SH_DIR)/third_party/xdl/xdl.c \
          $(SH_DIR)/third_party/xdl/xdl_iterate.c \
          $(SH_DIR)/third_party/xdl/xdl_linker.c \
          $(SH_DIR)/third_party/xdl/xdl_lzma.c \
          $(SH_DIR)/third_party/xdl/xdl_util.c

ifeq ($(SH_ARCH),arm64)
  SH_SRCS += $(SH_DIR)/arch/arm64/sh_a64.c
else
  SH_SRCS += $(SH_DIR)/arch/arm/sh_a32.c $(SH_DIR)/arch/arm/sh_t32.c
endif

ALL_SRCS = $(MEMKIT_SRCS) $(SH_SRCS)
MEMKIT_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(MEMKIT_SRCS))
SH_OBJS_C = $(patsubst $(SH_DIR)/%.c,$(OBJ_DIR)/shadowhook/%.o,$(filter %.c,$(SH_SRCS)))
SH_OBJS_S = $(patsubst $(SH_DIR)/%.S,$(OBJ_DIR)/shadowhook/%.o,$(filter %.S,$(SH_SRCS)))
ALL_OBJS = $(MEMKIT_OBJS) $(SH_OBJS_C) $(SH_OBJS_S)

# Progress calculation
TOTAL_FILES := $(words $(ALL_SRCS))
CUR_FILE = 0
define update_progress
	$(eval CUR_FILE=$(shell echo $$(($(CUR_FILE)+1))))
	$(eval PERCENT=$(shell echo $$(($(CUR_FILE)*100/$(TOTAL_FILES)))))
endef

# ============================================================================
# Targets
# ============================================================================

.PHONY: all clean setup directories banner help test test-clean

all: banner directories $(LIB_DIR)/libmemkit.so

help:
	@echo "Usage: make [TARGET] [VARIABLES]"
	@echo ""
	@echo "$(YELLOW)Targets:$(RESET)"
	@echo "  all          Default target, builds the shared library"
	@echo "  clean        Remove build directory"
	@echo "  setup        Initialize/Update git submodules"
	@echo "  help         Show this help message"
	@echo ""
	@echo "$(YELLOW)Variables:$(RESET)"
	@echo "  ANDROID_ABI       Target ABI (arm64-v8a, armeabi-v7a). Default: arm64-v8a"
	@echo "  ANDROID_PLATFORM  Android API level (e.g., android-35). Default: android-35"
	@echo "  NDK_HOME          Path to Android NDK. Defaults to \$$ANDROID_NDK_HOME"
	@echo ""
	@echo "$(YELLOW)Examples:$(RESET)"
	@echo "  make ANDROID_ABI=armeabi-v7a"
	@echo "  make ANDROID_PLATFORM=android-30"
	@echo ""

banner:
	@echo "$(WHITE)Target:$(RESET)"
	@echo "$(WHITE) - ABI      : $(ANDROID_ABI)$(RESET)"
	@echo "$(WHITE) - Platform : $(ANDROID_PLATFORM)$(RESET)"
	@echo ""

setup:
	@echo "$(WHITE)Updating submodules...$(RESET)"
	@git submodule update --init --recursive

directories:
	@mkdir -p $(OBJ_DIR)/src
	@mkdir -p $(OBJ_DIR)/shadowhook/arch/$(SH_ARCH)
	@mkdir -p $(OBJ_DIR)/shadowhook/common
	@mkdir -p $(OBJ_DIR)/shadowhook/nothing
	@mkdir -p $(OBJ_DIR)/shadowhook/third_party/xdl
	@mkdir -p $(OBJ_DIR)/tests
	@mkdir -p $(LIB_DIR)

$(LIB_DIR)/libmemkit.so: $(ALL_OBJS)
	@printf "$(WHITE)[100%%] $(YELLOW)[LD] $(WHITE)%s$(RESET)\n" "$@"
	@$(CC) $(LDFLAGS) -o $@ $^
	@$(STRIP) --strip-unneeded $@
	@echo ""
	@echo "$(GREEN)BUILD SUCCESSFUL!$(RESET)"

# Main sources
$(OBJ_DIR)/src/%.o: src/%.c
	$(call update_progress)
	$(call print_progress,"[CC]","$<",$(PERCENT))
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ShadowHook C
$(OBJ_DIR)/shadowhook/%.o: $(SH_DIR)/%.c
	$(call update_progress)
	$(call print_progress,"[CC]","$<",$(PERCENT))
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ShadowHook ASM
$(OBJ_DIR)/shadowhook/%.o: $(SH_DIR)/%.S
	$(call update_progress)
	$(call print_progress,"[AS]","$<",$(PERCENT))
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "$(WHITE)Cleaning build directory...$(RESET)"
	@echo ""
	@rm -rf build/

# ============================================================================
# Unit Tests
# ============================================================================

TEST_SRCS = tests/xdl_wrapper_test.c
TEST_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SRCS))
TEST_BIN = $(LIB_DIR)/xdl_wrapper_test

test: banner directories $(LIB_DIR)/libmemkit.so $(TEST_BIN)
	@echo ""
	@echo "$(WHITE)Test binary built successfully:$(RESET) $(TEST_BIN)"
	@echo "$(YELLOW)Note:$(RESET) This is an Android binary. Push to device to run:"
	@echo "  adb push $(TEST_BIN) /data/local/tmp/"
	@echo "  adb shell /data/local/tmp/xdl_wrapper_test"
	@echo ""

$(TEST_BIN): $(TEST_OBJS) $(LIB_DIR)/libmemkit.so
	@printf "$(WHITE)[TEST] $(YELLOW)[LD] $(WHITE)%s$(RESET)\n" "$@"
	@$(CC) $(LDFLAGS) -o $@ $(TEST_OBJS) -L$(LIB_DIR) -lmemkit

$(OBJ_DIR)/tests/%.o: tests/%.c
	$(call update_progress)
	$(call print_progress,"[CC]","$<",$(PERCENT))
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test-clean:
	@echo "$(WHITE)Cleaning test binaries...$(RESET)"
	@rm -f $(TEST_BIN)
