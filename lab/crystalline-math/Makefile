# Crystalline Math Library Makefile
# ⚠️ CRITICAL: ALL WORK MUST USE THE 'audit' FEATURE BRANCH ⚠️

# Compiler and flags
CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -Werror -O2 -fPIC -std=c11
CFLAGS += -I./include
CFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -D_GNU_SOURCE

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
LIB_DIR = lib
TEST_DIR = tests

# Source files (exclude _legacy files)
CORE_SRCS = $(filter-out %_legacy.c, $(wildcard $(SRC_DIR)/core/*.c))

ABACUS_SRCS = $(wildcard $(SRC_DIR)/bigint/*.c)
NTT_SRCS = $(wildcard $(SRC_DIR)/ntt/*.c)
GEOMETRY_SRCS = $(wildcard $(SRC_DIR)/geometry/*.c)
PRIME_SRCS = $(wildcard $(SRC_DIR)/prime/*.c)
CRYPTO_SRCS = $(wildcard $(SRC_DIR)/crypto/*.c)
VISUALIZATION_SRCS = $(wildcard $(SRC_DIR)/visualization/*.c)
COMPACT_SRCS = $(filter-out %_legacy.c, $(wildcard $(SRC_DIR)/compact/*.c))
PLATONIC_SRCS = $(filter-out %_legacy.c, $(wildcard $(SRC_DIR)/platonic/*.c))
BABYLONIAN_SRCS = $(wildcard $(SRC_DIR)/babylonian/*.c)
ABACUS88D_SRCS = $(wildcard $(SRC_DIR)/abacus88d/*.c)

ALL_SRCS = $(CORE_SRCS) $(ABACUS_SRCS) $(NTT_SRCS) $(GEOMETRY_SRCS) $(PRIME_SRCS) $(CRYPTO_SRCS) $(VISUALIZATION_SRCS) $(COMPACT_SRCS) $(PLATONIC_SRCS) $(BABYLONIAN_SRCS) $(THREADING_SRCS) $(ABACUS88D_SRCS)

# Object files
CORE_OBJS = $(CORE_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

ABACUS_OBJS = $(ABACUS_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
NTT_OBJS = $(NTT_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
GEOMETRY_OBJS = $(GEOMETRY_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
PRIME_OBJS = $(PRIME_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
CRYPTO_OBJS = $(CRYPTO_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
COMPACT_OBJS = $(COMPACT_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
PLATONIC_OBJS = $(PLATONIC_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
BABYLONIAN_OBJS = $(BABYLONIAN_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
THREADING_OBJS = $(THREADING_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
ABACUS88D_OBJS = $(ABACUS88D_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

ALL_OBJS = $(CORE_OBJS) $(ABACUS_OBJS) $(NTT_OBJS) $(GEOMETRY_OBJS) $(PRIME_OBJS) $(CRYPTO_OBJS) $(COMPACT_OBJS) $(PLATONIC_OBJS) $(BABYLONIAN_OBJS) $(THREADING_OBJS) $(ABACUS88D_OBJS)

# Library files
STATIC_LIB = $(LIB_DIR)/libcrystallinemath.a
SHARED_LIB = $(LIB_DIR)/libcrystallinemath.so

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/tests/%)

# Default target
.PHONY: all
all: directories $(STATIC_LIB) $(SHARED_LIB)

# Create necessary directories
.PHONY: directories
directories:
	@mkdir -p $(BUILD_DIR)/core
	
	@mkdir -p $(BUILD_DIR)/bigint
	@mkdir -p $(BUILD_DIR)/ntt
	@mkdir -p $(BUILD_DIR)/geometry
	@mkdir -p $(BUILD_DIR)/prime
	@mkdir -p $(BUILD_DIR)/platonic
	@mkdir -p $(BUILD_DIR)/crypto
	@mkdir -p $(BUILD_DIR)/compact
	@mkdir -p $(BUILD_DIR)/tests
	@mkdir -p $(LIB_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Build static library
$(STATIC_LIB): $(ALL_OBJS)
	@echo "Creating static library $@..."
	@$(AR) rcs $@ $^
	@echo "Static library created: $@"

# Build shared library
$(SHARED_LIB): $(ALL_OBJS)
	@echo "Creating shared library $@..."
	@$(CC) -shared -Wl,-z,noexecstack -o $@ $^ -lm
	@echo "Shared library created: $@"

# Build tests
.PHONY: tests
tests: directories $(STATIC_LIB) $(TEST_BINS)

$(BUILD_DIR)/tests/%: $(TEST_DIR)/%.c $(STATIC_LIB)
	@echo "Building test $@..."
	@$(CC) $(CFLAGS) $< $(STATIC_LIB) -o $@

# Run tests
.PHONY: test
test: tests
	@echo "Running tests..."
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo "All tests passed!"

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -rf $(LIB_DIR)
	@echo "Clean complete."

# Install libraries (optional)
.PHONY: install
install: all
	@echo "Installing libraries..."
	@mkdir -p /usr/local/lib
	@mkdir -p /usr/local/include/math
	@cp $(STATIC_LIB) /usr/local/lib/
	@cp $(SHARED_LIB) /usr/local/lib/
	@cp -r $(INC_DIR)/math/* /usr/local/include/math/
	@ldconfig
	@echo "Installation complete."

# Uninstall libraries (optional)
.PHONY: uninstall
uninstall:
	@echo "Uninstalling libraries..."
	@rm -f /usr/local/lib/libcrystallinemath.a
	@rm -f /usr/local/lib/libcrystallinemath.so
	@rm -rf /usr/local/include/math
	@ldconfig
	@echo "Uninstall complete."

# Show help
.PHONY: help
help:
	@echo "Crystalline Math Library Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build static and shared libraries (default)"
	@echo "  tests      - Build test executables"
	@echo "  test       - Build and run all tests"
	@echo "  clean      - Remove build artifacts"
	@echo "  install    - Install libraries to /usr/local"
	@echo "  uninstall  - Remove installed libraries"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  CC         - C compiler (default: gcc)"
	@echo "  CFLAGS     - Compiler flags"
	@echo "  BUILD_DIR  - Build directory (default: build)"
	@echo "  LIB_DIR    - Library output directory (default: lib)"

# Debug: Show variables
.PHONY: debug
debug:
	@echo "Source files:"
	@echo "  CORE_SRCS: $(CORE_SRCS)"
	
	@echo "  ABACUS_SRCS: $(ABACUS_SRCS)"
        @echo "  NTT_SRCS: $(NTT_SRCS)"
	@echo "  GEOMETRY_SRCS: $(GEOMETRY_SRCS)"
	@echo "  PRIME_SRCS: $(PRIME_SRCS)"
	@echo "  CRYPTO_SRCS: $(CRYPTO_SRCS)"
	@echo ""
	@echo "Object files:"
	@echo "  ALL_OBJS: $(ALL_OBJS)"
	@echo ""
	@echo "Libraries:"
	@echo "  STATIC_LIB: $(STATIC_LIB)"
	@echo "  SHARED_LIB: $(SHARED_LIB)"