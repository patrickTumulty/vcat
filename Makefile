ARCH ?= native
BUILD_DIR ?= $(CURDIR)/build
INSTALL_PREFIX ?= $(CURDIR)/dist
BUILD_TYPE = Debug 
TARGET = all

BUILD_FILE = $(BUILD_DIR)/build.ninja

.PHONY: help all configure build bundle install clean

all: build

help:
	@echo "vcat build driver"
	@echo
	@echo "Targets:"
	@echo "  make [ARCH=..]                  configure + build (default)"
	@echo "  make configure                  configure CMake preset into BUILD_DIR"
	@echo "  make build                      build via CMake into BUILD_DIR"
	@echo "  make build-only                 build via CMake into BUILD_DIR (no project config)"
	@echo "  make install [INSTALL_PREFIX=]  bundle binary + all linked deps"
	@echo "  make clean                      remove BUILD_DIR and the bundle"
	@echo
	@echo "Variables:"
	@echo "  ARCH           native | arm64   (default: native)"
	@echo "  BUILD_DIR      build dir (default: build/)"
	@echo "  BUILD_TYPE     debug | release (default: release)"
	@echo "  INSTALL_PREFIX bundle output dir (default: dist/)"
	@echo
	@echo "Example:"
	@echo "  make install ARCH=arm64 INSTALL_PREFIX=/opt/vcat-arm64"
	@echo "  make build ARCH=arm64 BUILD_DIR=build/arm64"

configure:
	cmake --preset $(ARCH) -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

$(BUILD_FILE): configure
	@true

build: $(BUILD_FILE)
	cmake --build $(BUILD_DIR) --target $(TARGET)

build-only:
	cmake --build $(BUILD_DIR) --target $(TARGET)

bundle: install

install: build
	scripts/bundle.sh $(ARCH) $(INSTALL_PREFIX) $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(INSTALL_PREFIX)
