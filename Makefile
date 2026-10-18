ARCH ?= x86_64
BUILD_DIR ?= $(CURDIR)/build
BUILD_TYPE = Debug 
TARGET = all

BUILD_FILE = $(BUILD_DIR)/build.ninja

.PHONY: help all configure build build-only bundle clean nuke

all: build

help:
	@echo "vcat build driver"
	@echo
	@echo "Targets:"
	@echo "  make [ARCH=..]                  configure + build (default)"
	@echo "  make configure                  configure CMake preset into BUILD_DIR"
	@echo "  make build                      build via CMake into BUILD_DIR"
	@echo "  make build-only                 build via CMake into BUILD_DIR (no project config)"
	@echo "  make bundle                     bundle binary + all linked deps"
	@echo "  make clean                      clean targets"
	@echo "  make nuke                       remove BUILD_DIR and the bundle"
	@echo
	@echo "Variables:"
	@echo "  ARCH           x86_64 | arm64   (default: x86_64)"
	@echo "  BUILD_DIR      build dir (default: build/)"
	@echo "  BUILD_TYPE     debug | release (default: release)"
	@echo
	@echo "Example:"
	@echo "  make build ARCH=arm64 BUILD_DIR=build/arm64"

configure:
	cmake --preset $(ARCH) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

$(BUILD_FILE): configure
	@true

build: $(BUILD_FILE)
	cmake --build $(BUILD_DIR) --target $(TARGET)

build-only:
	cmake --build $(BUILD_DIR) --target $(TARGET)

bundle: 
	scripts/bundle.sh $(ARCH) $(BUILD_DIR)

nuke:
	@echo " ⠀⠀⠀⠀⠀⠀⠀⣀⣠⣀⣀⠀⠀⣀⣤⣤⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⣀⣠⣤⣤⣾⣿⣿⣿⣿⣷⣾⣿⣿⣿⣿⣿⣶⣿⣿⣿⣶⣤⡀⠀⠀⠀⠀"
	@echo "⠀⢠⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⠀⠀⠀⠀"
	@echo "⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⡀⠀"
	@echo "⠀⢀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀"
	@echo "⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠿⠟⠁⠀"
	@echo "⠀⠀⠻⢿⡿⢿⣿⣿⣿⣿⠟⠛⠛⠋⣀⣀⠙⠻⠿⠿⠋⠻⢿⣿⣿⠟⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠈⠉⣉⣠⣴⣷⣶⣿⣿⣿⣿⣶⣶⣶⣾⣶⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⠋⠈⠛⠿⠟⠉⠻⠿⠋⠉⠛⠁⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣶⣷⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⢀⣀⣠⣤⣤⣤⣤⣶⣿⣿⣷⣦⣤⣤⣤⣤⣀⣀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⢰⣿⠛⠉⠉⠁⠀⠀⠀⢸⣿⣿⣧⠀⠀⠀⠀⠉⠉⠙⢻⣷⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠙⠻⠷⠶⣶⣤⣤⣤⣿⣿⣿⣿⣦⣤⣤⣴⡶⠶⠟⠛⠁⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⣿⣿⣿⣿⣷⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	@echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠒⠛⠛⠛⠛⠛⠛⠛⠛⠛⠛⠓⠀⠀⠀⠀⠀⠀⠀⠀⠀"
	rm -rf $(BUILD_DIR) artifacts

clean:
	cmake --build $(BUILD_DIR) --target clean

