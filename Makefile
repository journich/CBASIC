# CBASIC - Microsoft BASIC 1.1 Compatible Interpreter
# Makefile for building the C17 implementation
#
# Copyright (c) 2025 Tim Buchalka
# Licensed under the MIT License
#
# Targets:
#   make          - Build the interpreter
#   make debug    - Build with debug symbols
#   make release  - Build optimized release
#   make test     - Build and run tests
#   make clean    - Remove build artifacts
#   make install  - Install to system (requires root)

# Compiler settings - Use C17 standard for maximum portability
CC ?= cc
CSTD = -std=c17

# Warning flags - Be strict about code quality
WARNINGS = -Wall -Wextra -Wpedantic -Werror=implicit-function-declaration

# Include directories
INCLUDES = -I./include

# Source files
SRCDIR = src
SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/interpreter.c \
          $(SRCDIR)/tokenizer.c \
          $(SRCDIR)/expression.c \
          $(SRCDIR)/statements.c \
          $(SRCDIR)/variables.c \
          $(SRCDIR)/functions.c \
          $(SRCDIR)/io.c \
          $(SRCDIR)/error.c

# Test source files
TESTDIR = tests
TEST_SOURCES = $(TESTDIR)/test_main.c \
               $(TESTDIR)/test_tokenizer.c \
               $(TESTDIR)/test_expression.c \
               $(TESTDIR)/test_statements.c \
               $(TESTDIR)/test_functions.c \
               $(TESTDIR)/test_integration.c

# Object files (excluding main.c for test builds)
LIB_SOURCES = $(filter-out $(SRCDIR)/main.c, $(SOURCES))

# Output directories
BUILDDIR = build
BINDIR = bin

# Output binary
TARGET = $(BINDIR)/cbasic
TEST_TARGET = $(BINDIR)/cbasic_test

# Default build flags (can be overridden)
CFLAGS ?= -O2

# Link flags
LDFLAGS = -lm

# Platform-specific adjustments
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS specific flags
    LDFLAGS +=
endif
ifeq ($(UNAME_S),Linux)
    # Linux specific flags
    LDFLAGS +=
endif

# Installation prefix
PREFIX ?= /usr/local

# Default target
all: release

# Create directories
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Release build (optimized)
release: CFLAGS = -O2 -DNDEBUG
release: $(TARGET)

# Debug build (with symbols and no optimization)
debug: CFLAGS = -g -O0 -DDEBUG
debug: $(TARGET)

# Sanitizer build (for finding memory issues)
sanitize: CFLAGS = -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
sanitize: LDFLAGS += -fsanitize=address,undefined
sanitize: $(TARGET)

# Build the main target
$(TARGET): $(SOURCES) | $(BINDIR)
	$(CC) $(CSTD) $(CFLAGS) $(WARNINGS) $(INCLUDES) $(SOURCES) -o $(TARGET) $(LDFLAGS)
	@echo "Built $(TARGET)"

# Build tests
$(TEST_TARGET): $(LIB_SOURCES) $(TEST_SOURCES) | $(BINDIR)
	$(CC) $(CSTD) -g -O0 $(WARNINGS) $(INCLUDES) $(LIB_SOURCES) $(TEST_SOURCES) -o $(TEST_TARGET) $(LDFLAGS)
	@echo "Built $(TEST_TARGET)"

# Run tests
test: $(TEST_TARGET)
	@echo "Running tests..."
	./$(TEST_TARGET)

# Run tests with valgrind (memory checking)
memcheck: $(TEST_TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TEST_TARGET)

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR) $(BINDIR)
	rm -f *.o

# Install to system
install: release
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/cbasic
	@echo "Installed cbasic to $(PREFIX)/bin/"

# Uninstall from system
uninstall:
	rm -f $(PREFIX)/bin/cbasic

# Run the interpreter interactively
run: release
	./$(TARGET)

# Run a BASIC program
run-%: release
	./$(TARGET) $*.bas

# Format code (if clang-format is available)
format:
	@if command -v clang-format > /dev/null; then \
		find $(SRCDIR) $(TESTDIR) -name '*.c' -o -name '*.h' | xargs clang-format -i; \
		echo "Code formatted"; \
	else \
		echo "clang-format not found"; \
	fi

# Generate documentation (if doxygen is available)
docs:
	@if command -v doxygen > /dev/null; then \
		doxygen Doxyfile; \
	else \
		echo "doxygen not found"; \
	fi

# Show help
help:
	@echo "CBASIC Makefile targets:"
	@echo "  make          - Build release version"
	@echo "  make debug    - Build with debug symbols"
	@echo "  make release  - Build optimized release"
	@echo "  make sanitize - Build with sanitizers"
	@echo "  make test     - Build and run tests"
	@echo "  make memcheck - Run tests with valgrind"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make install  - Install to $(PREFIX)/bin"
	@echo "  make run      - Run the interpreter"
	@echo "  make format   - Format source code"
	@echo ""
	@echo "Variables:"
	@echo "  CC=$(CC)"
	@echo "  CFLAGS=$(CFLAGS)"
	@echo "  PREFIX=$(PREFIX)"

.PHONY: all release debug sanitize test memcheck clean install uninstall run format docs help
