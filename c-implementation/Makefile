# SAV IPFIX Validator Makefile
# Project: sav-ipfix-validator
# Date: 2025-12-08

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -std=c99
CFLAGS += -I./include
CFLAGS += $(shell pkg-config --cflags libfixbuf glib-2.0)

LDFLAGS = $(shell pkg-config --libs libfixbuf glib-2.0)

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = test
TOOLS_DIR = tools
EXAMPLES_DIR = examples
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin
LIB_DIR = $(BUILD_DIR)/lib

# Source files
LIB_SOURCES = $(wildcard $(SRC_DIR)/*.c)
LIB_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(LIB_SOURCES))

# Library
LIB_NAME = libsav_ipfix.a
LIB_TARGET = $(LIB_DIR)/$(LIB_NAME)

# Tools (to be implemented)
TOOLS_SOURCES = $(wildcard $(TOOLS_DIR)/*.c)
TOOLS_TARGETS = $(patsubst $(TOOLS_DIR)/%.c,$(BIN_DIR)/%,$(TOOLS_SOURCES))

# Test programs
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_TARGETS = $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/test_%,$(TEST_SOURCES))

# Targets
.PHONY: all clean lib tools tests examples help install

all: lib

lib: $(LIB_TARGET)

tools: lib $(TOOLS_TARGETS)

tests: lib $(TEST_TARGETS)

examples: lib
	@echo "Building examples..."
	@if [ -f $(EXAMPLES_DIR)/example_export.c ]; then \
		$(CC) $(CFLAGS) -o $(BIN_DIR)/example_export \
			$(EXAMPLES_DIR)/example_export.c \
			$(LIB_TARGET) $(LDFLAGS); \
	fi
	@if [ -f $(EXAMPLES_DIR)/example_collect.c ]; then \
		$(CC) $(CFLAGS) -o $(BIN_DIR)/example_collect \
			$(EXAMPLES_DIR)/example_collect.c \
			$(LIB_TARGET) $(LDFLAGS); \
	fi

# Build library
$(LIB_TARGET): $(LIB_OBJECTS) | $(LIB_DIR)
	@echo "Creating static library: $@"
	ar rcs $@ $^
	@echo "Library created successfully!"

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Build tools
$(BIN_DIR)/%: $(TOOLS_DIR)/%.c $(LIB_TARGET) | $(BIN_DIR)
	@echo "Building tool: $@"
	$(CC) $(CFLAGS) -o $@ $< $(LIB_TARGET) $(LDFLAGS)

# Build tests
$(BIN_DIR)/test_%: $(TEST_DIR)/%.c $(LIB_TARGET) | $(BIN_DIR)
	@echo "Building test: $@"
	$(CC) $(CFLAGS) -o $@ $< $(LIB_TARGET) $(LDFLAGS)

# Create directories
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(LIB_DIR):
	@mkdir -p $(LIB_DIR)

# Clean
clean:
	@echo "Cleaning build files..."
	rm -rf $(BUILD_DIR)
	rm -f $(EXAMPLES_DIR)/sample_collector $(EXAMPLES_DIR)/sample_exporter
	rm -f $(EXAMPLES_DIR)/*.ipfix
	@echo "Clean complete!"

# Run tests
test: tests
	@echo "Running tests..."
	@for test in $(TEST_TARGETS); do \
		if [ -f $$test ]; then \
			echo "Running $$test..."; \
			$$test || exit 1; \
		fi \
	done
	@echo "All tests passed!"

# Install (optional)
install: lib
	@echo "Installing library and headers..."
	@sudo mkdir -p /usr/local/lib
	@sudo mkdir -p /usr/local/include/sav_ipfix
	@sudo cp $(LIB_TARGET) /usr/local/lib/
	@sudo cp $(INC_DIR)/*.h /usr/local/include/sav_ipfix/
	@sudo ldconfig
	@echo "Installation complete!"

# Help
help:
	@echo "SAV IPFIX Validator - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build the library (default)"
	@echo "  lib       - Build static library"
	@echo "  tools     - Build command-line tools"
	@echo "  tests     - Build test programs"
	@echo "  examples  - Build example programs"
	@echo "  test      - Build and run all tests"
	@echo "  clean     - Remove all build files"
	@echo "  install   - Install library and headers (requires sudo)"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make          # Build library"
	@echo "  make tools    # Build all tools"
	@echo "  make test     # Run tests"
	@echo "  make clean    # Clean build"

# Show configuration
config:
	@echo "Build Configuration:"
	@echo "  CC       = $(CC)"
	@echo "  CFLAGS   = $(CFLAGS)"
	@echo "  LDFLAGS  = $(LDFLAGS)"
	@echo "  SRC_DIR  = $(SRC_DIR)"
	@echo "  BUILD    = $(BUILD_DIR)"
	@echo ""
	@echo "libfixbuf version:"
	@pkg-config --modversion libfixbuf
