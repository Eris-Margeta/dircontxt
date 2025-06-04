# Compiler and C_STANDARD
CC = clang
C_STANDARD = -std=c11 # C11 standard

# Directories
SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
TARGET_DIR = $(BUILD_DIR)/bin

# Target executable name
TARGET = $(TARGET_DIR)/dircontxt

# Source files (find all .c files in SRC_DIR)
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Object files (replace .c with .o and put them in OBJ_DIR)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Compilation flags
# -g: Add debug information
# -Wall: Enable all common warnings
# -Wextra: Enable more warnings
# -pedantic: Issue all warnings demanded by strict ISO C
# CFLAGS for development (with debug symbols and more warnings)
CFLAGS = $(C_STANDARD) -g -Wall -Wextra -pedantic -I$(SRC_DIR) # -I$(SRC_DIR) to find local .h files
# CFLAGS for release (optimized, no debug symbols typically)
# CFLAGS_RELEASE = $(C_STANDARD) -O2 -Wall -I$(SRC_DIR) -DNDEBUG

# Linker flags
LDFLAGS =

# Default target (called when you just run `gmake`)
all: $(TARGET)

# Rule to link the target executable
$(TARGET): $(OBJS)
	@mkdir -p $(TARGET_DIR) # Ensure bin directory exists
	@echo "LD $@"
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Rule to compile .c files into .o files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR) # The "| $(OBJ_DIR)" is an order-only prerequisite
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to create the object directory (order-only prerequisite)
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# Phony targets (targets that don't represent actual files)
.PHONY: all clean run debug_run help

# Clean up build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET_DIR)/$(TARGET) # In case target is just 'dircontxt' without path

# Run the program with a test directory (assuming 'test_dir' exists)
run: $(TARGET)
	@echo "Running $(TARGET) on ./test_dir ..."
	mkdir -p test_dir/subdir
	echo "Hello from file1.txt" > test_dir/file1.txt
	echo "Data in subdir_file.md" > test_dir/subdir/subdir_file.md
	echo ".DS_Store" > test_dir/.dircontxtignore
	echo "ignored_file.tmp" >> test_dir/.dircontxtignore
	echo "ignored_folder/" >> test_dir/.dircontxtignore
	touch test_dir/ignored_file.tmp
	mkdir -p test_dir/ignored_folder
	touch test_dir/ignored_folder/something.txt
	$(TARGET) test_dir
	@echo "Output should be in $(BUILD_DIR) (likely test_dir.dircontxt if parent of test_dir is BUILD_DIR, or in parent of project if test_dir is at project root)"
	@echo "Consider placing output in project root for easier inspection for now."
	@echo "To inspect binary: hexdump -C test_dir.dircontxt (or similar if not in parent)"


# Run with debug logging enabled (if not already default)
# For this, ensure NDEBUG is NOT defined. Our CFLAGS for 'all' already omit NDEBUG.
debug_run: $(TARGET)
	@echo "Running $(TARGET) with debug output on ./test_dir ..."
	mkdir -p test_dir/subdir
	echo "Hello from file1.txt" > test_dir/file1.txt
	echo "Data in subdir_file.md" > test_dir/subdir/subdir_file.md
	echo ".DS_Store" > test_dir/.dircontxtignore
	echo "ignored_file.tmp" >> test_dir/.dircontxtignore
	echo "ignored_folder/" >> test_dir/.dircontxtignore
	touch test_dir/ignored_file.tmp
	mkdir -p test_dir/ignored_folder
	touch test_dir/ignored_folder/something.txt
	$(TARGET) test_dir
	@echo "Inspect output and logs."


help:
	@echo "Available targets:"
	@echo "  all         : Build the $(TARGET) executable (default)."
	@echo "  clean       : Remove all build artifacts."
	@echo "  run         : Build and run $(TARGET) on a sample 'test_dir'."
	@echo "  debug_run   : Build and run $(TARGET) on 'test_dir' (ensures debug logs)."
	@echo "  help        : Show this help message."

# Tell Make that these are not files
.SECONDARY: $(OBJS) # Keeps .o files around for inspection if needed, not strictly necessary
