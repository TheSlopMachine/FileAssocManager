# File Association Manager - build with MinGW-w64 gcc
#
#   make build              release build -> build/FileAssocManager.exe
#   make build DEBUG=1      unoptimised build with debug symbols
#   make test               run the hash tests on this machine (needs a host C compiler)
#   make clean              remove build/
#   make build CROSS=x86_64-w64-mingw32-    cross-compile from Linux/macOS
#
# On Windows with a MinGW-w64 toolchain use `make` or `mingw32-make`.

NAME    := FileAssocManager
BUILD   := build
SRC_DIR := src
RES_DIR := res
TEST_DIR := tests

CROSS   ?=
CC      := $(CROSS)gcc
WINDRES := $(CROSS)windres
HOSTCC  ?= cc

CFLAGS  += -std=gnu99 -Wall -Wextra -municode -MMD -MP -I$(SRC_DIR) -I$(RES_DIR)
LDFLAGS += -mwindows -municode -static
LDLIBS  := -lcomctl32 -lcomdlg32 -ladvapi32 -lshell32 -lshlwapi -luxtheme -lgdi32 -luser32 -lole32 -luuid -lversion

ifeq ($(DEBUG),1)
CFLAGS  += -O0 -g
else
CFLAGS  += -O2
LDFLAGS += -s
endif

SRCS   := $(wildcard $(SRC_DIR)/*.c)
OBJS   := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SRCS)) $(BUILD)/app_rc.o
DEPS   := $(OBJS:.o=.d)
TARGET := $(BUILD)/$(NAME).exe

# Pick a directory-removal command that works from cmd.exe and from sh.
ifeq ($(shell echo %OS%),Windows_NT)
RMBUILD = if exist $(BUILD) rmdir /s /q $(BUILD)
else
RMBUILD = rm -rf $(BUILD)
endif

# The output directory shares its name with the `build` target, so create it here
# (a rule for it would be circular). `mkdir` without flags works in cmd.exe and sh.
ifneq ($(MAKECMDGOALS),clean)
ifeq ($(wildcard $(BUILD)),)
$(shell mkdir $(BUILD))
endif
endif

.DEFAULT_GOAL := build
.PHONY: build test clean

build: $(TARGET)
	@echo Built $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/app_rc.o: $(RES_DIR)/app.rc $(RES_DIR)/app.manifest $(RES_DIR)/app.ico $(RES_DIR)/resource.h
	$(WINDRES) -I$(RES_DIR) -O coff -i $< -o $@

# The hash code is plain C, so its tests run natively, not on Windows.
test: $(TEST_DIR)/uchash_test.c $(SRC_DIR)/uchash.c $(SRC_DIR)/uchash.h $(SRC_DIR)/uchash_tables.inc $(TEST_DIR)/vectors.inc
	$(HOSTCC) -std=c99 -Wall -Wextra -O2 -I$(SRC_DIR) -I$(TEST_DIR) $(TEST_DIR)/uchash_test.c $(SRC_DIR)/uchash.c -o $(BUILD)/uchash_test
	$(BUILD)/uchash_test

clean:
	$(RMBUILD)

-include $(DEPS)
