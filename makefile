
EXE_NAME := mcc

CXX := g++

DEBUG_FLAGS := -g -O0 -fno-omit-frame-pointer
SUPER_DEBUG_FLAGS := -fsanitize=address,undefined
RELEASE_FLAGS := -O3 -DNDEBUG

CXXFLAGS := -std=c++20 -Wall -Iinclude -MMD -MP
CXXDEFS ?=
LDBONUS :=

WIN ?= 0

DEBUG ?= 0
EDITOR ?= 0
RELEASE ?= 0


ifeq ($(RELEASE),1)
	DEBUG = 0
	EDITOR = 0

	CXXDEFS += -DRELEASE=1
	CXXFLAGS += $(RELEASE_FLAGS)
	LDBONUS += $(RELEASE_FLAGS)
else
	ifeq ($(BETA),1)
		DEBUG = 0
		EDITOR = 0

		CXXDEFS += -DBETA=1
		CXXFLAGS += $(DEBUG_FLAGS)
		LDBONUS += $(DEBUG_FLAGS)
		ifeq ($(WIN),0)
			CXXFLAGS += $(SUPER_DEBUG_FLAGS)
			LDBONUS += $(SUPER_DEBUG_FLAGS)
		endif
	endif
endif
ifeq ($(EDITOR),1)
	VERBOSE = 1
	CXXDEFS += -DEDITOR=1
	DEBUG = 1
endif
ifeq ($(DEBUG),1)
	CXXFLAGS += $(DEBUG_FLAGS)
	LDBONUS += $(DEBUG_FLAGS)
	ifeq ($(WIN),0)
		CXXFLAGS += $(SUPER_DEBUG_FLAGS)
		LDBONUS += $(SUPER_DEBUG_FLAGS)
	endif

	CXXDEFS += -DDEBUG=1
else
	ifeq ($(FAKEDEBUG),1)
		CXXDEFS += -DDEBUG=1
	endif
endif

MAJOR ?= 0
MINOR ?= 1
PATCH ?= 0

GIT_COMMIT := $(shell git rev-parse --short=12 HEAD 2>/dev/null)
BUILD_TIME := $(shell date -u +"%d-%m-%Y")

CXXDEFS += -DBASE_MAJOR_VERSION=$(MAJOR) -DBASE_MINOR_VERSION=$(MINOR) -DBASE_PATCH_VERSION=$(PATCH) -DGIT_COMMIT=$(GIT_COMMIT) -DBUILD_TIME=$(BUILD_TIME)


VERBOSE ?= 0
F_EXT :=
BUILD_EXT := STANDARD
BUILD_PLAT := LINUX
ifeq ($(WIN),1)
	LIB = data.dll
	ENGINE_LIB = engine.dll
	CXX = x86_64-w64-mingw32-g++ -Wuninitialized
	F_EXT = .exe
	BUILD_PLAT := WIN
endif

ifeq ($(RELEASE),1)
	BUILD_EXT = RELEASE
endif
ifeq ($(BETA),1)
	BUILD_EXT = BETA
endif
ifeq ($(EDITOR),1)
	BUILD_EXT = EDITOR
else
	ifeq ($(DEBUG),1)
		BUILD_EXT = DEBUG
	endif
endif
ifeq ($(VERBOSE),1)
	CXXDEFS += -DVERBOSE=1
endif

BUILD_NAME := $(BUILD_PLAT)$(BUILD_EXT)
BUILD_DIR := build/$(BUILD_NAME)
EXPORT_DIR := export/$(BUILD_NAME)
TARGET := $(EXPORT_DIR)/$(EXE_NAME)$(F_EXT)

SRC_DIR := src

INCDIRS := $(shell find src include \
  \( -type d \( -name gcc -o -name mingw \) -prune \) -o -type d -print 2>/dev/null | sort -u)
INCDIRS += $(shell find cpp-baseutils include -type d -print 2>/dev/null | sort -u)
ifeq ($(WIN),1)
	INCDIRS += $(shell find src/engine/mingw -type d)
else
	INCDIRS += $(shell find src/engine/gcc -type d)
endif

INCLUDES := $(patsubst %,-I%,$(INCDIRS))
PKGS := sdl3 sdl3_image sdl3_mixer stb glew mesa mingw-w64-gcc cmake clang boost libsodium



CXXFLAGS += $(INCLUDES)
STATICINC := -I/usr/include/SDL3 -I/usr/include/SDL3_image -I/usr/include/SDL3_mixer -I/usr/include/GL
LDFLAGS := -lGL -lGLEW
CHANNEL_NAME := linux
CHANNEL_SUB :=
ifeq ($(WIN),1)
	CHANNEL_NAME = windows
	LDFLAGS = -Lsrc/engine/mingw -lglew32 -lopengl32 -lws2_32 -lmswsock
	STATICINC = -I/usr/x86_64-w64-mingw32/include/GL -Isrc/engine/mingw -Isrc/engine/mingw/archive -nostdinc++ -I/usr/x86_64-w64-mingw32/include -I/usr/x86_64-w64-mingw32/include/c++/16.1.0 -I/usr/x86_64-w64-mingw32/include/c++/16.1.0/x86_64-w64-mingw32
endif
ifeq ($(BETA),1)
	CHANNEL_SUB = -beta
endif


PKG_EXT_BAN ?= flp
EXT_BAN_PATTERNS = $(foreach ext,$(PKG_EXT_BAN),-o -name "*.$(ext)")

.PHONY: all package package_base can_push run clean copylibs_base build install

all: package
package_base:
	@rm -rf $(EXPORT_DIR)
	@mkdir -p $(EXPORT_DIR)

can_push:
	@if [ "$(EDITOR)" != "0" ]; then \
		echo "cannot push editor builds";\
		exit 1;\
	fi
	@if [ "$(BETA)" != "1" ] && [ "$(RELEASE)" != "1" ]; then \
		echo "can only push beta & release";\
		exit 1;\
	fi

$(TARGET): package
run: $(TARGET)
	$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

copylibs_base: build
ifeq ($(WIN),1)
	@echo "WIN=1: Copying DLLs into $(EXPORT_DIR)/lib ..."

	@# Copy all DLLs from src/engine/mingw
	@dlls="$$(ls -1 src/engine/mingw/*.dll 2>/dev/null || true)"; \
	if [ -n "$$dlls" ]; then \
		for f in $$dlls; do \
			echo "  $$f"; \
			cp -f "$$f" "$(EXPORT_DIR)/"; \
		done; \
	else \
		echo "  (no DLLs found in src/engine/mingw)"; \
	fi; \
	\
	# Copy other required DLLs (edit this list)
	@extras="/usr/x86_64-w64-mingw32/bin/libgcc_s_seh-1.dll /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll /usr/x86_64-w64-mingw32/bin/libstdc++-6.dll src/engine/mingw/gzip.exe"; if [ -n "$$extras" ]; then \
		for f in $$extras; do \
			echo "  $$f"; \
			cp -f "$$f" "$(EXPORT_DIR)/"; \
		done; \
	fi

else
	@mkdir -p "$(EXPORT_DIR)/lib"
	@echo "Copying dependent shared libraries into $(EXPORT_DIR)/lib ..."
	@# Collect absolute paths of libs from ldd and copy them.
	@libs="$$(ldd "$(TARGET)" 2>/dev/null \
		| awk '/=>/ {print $$3} /^\// {print $$1}' \
		| grep -v 'not found' \
		| sort -u)"; \
	echo $$libs ;for f in $$libs; do \
		if [ -e "$$f" ]; then \
		echo "  $$f"; \
		cp -f "$$f" "$(EXPORT_DIR)/lib/"; \
		fi \
	done
endif


install:
	@set -e; \
	for p in $(PKGS); do \
		if pacman -Qi "$$p" >/dev/null 2>&1; then \
			echo "Already installed: $$p"; \
		else \
			echo "Installing: $$p"; \
			sudo pacman -S --noconfirm --needed "$$p"; \
		fi; \
	done


SRCS := $(shell find $(SRC_DIR) -name '*.cpp' -not -name '*.test.cpp')
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CXXDEFS) -c $< -o $@
-include $(DEPS)


link: $(OBJS)
	@mkdir -p $(EXPORT_DIR)
	$(CXX) -Wl,--as-needed $(WIN_META_RES) $^ $(CXXFLAGS) $(LDFLAGS) $(SHARED_DEFS) -o $(TARGET)
package: package_base link

CXXFLAGS += -I./

