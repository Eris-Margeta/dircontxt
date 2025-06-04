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
# This will include reader_core.c and reader_main.c, leading to warnings if they are empty.
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
# CFLAGS for release (optimized, no debug symbols typically, defines NDEBUG)
CFLAGS_RELEASE = $(C_STANDARD) -O2 -Wall -I$(SRC_DIR) -DNDEBUG

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

# Rule to create the object directory (order-only prerequisite for the compilation rule)
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# Phony targets (targets that don't represent actual files)
.PHONY: all clean run debug_run help release

# Clean up build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	# The following line might be redundant if TARGET includes TARGET_DIR,
	# but can be useful if TARGET was defined as just the executable name.
	rm -f $(TARGET_DIR)/$(notdir $(TARGET)) # Remove target from bin dir
	rm -f $(notdir $(TARGET)) # Remove target if it ended up in project root by mistake

# Run the program with a test directory (assuming 'test_dir' exists or will be created)
run: $(TARGET)
	@echo "Running $(TARGET) on ./test_dir ..."
	rm -rf test_dir # Clean previous test_dir to ensure fresh run
	rm -f test_dir.dircontxt # Clean previous output file
	mkdir -p test_dir/subdir
	mkdir -p test_dir/another_empty_dir
	echo "Hello from file1.txt in root." > test_dir/file1.txt
	echo "Data in subdir_file.md." > test_dir/subdir/subdir_file.md
	echo "Another file for testing: file2.c" > test_dir/file2.c
	echo "// This is .dircontxtignore" > test_dir/.dircontxtignore
	echo ".DS_Store" >> test_dir/.dircontxtignore
	echo "ignored_file.tmp" >> test_dir/.dircontxtignore
	echo "ignored_folder/" >> test_dir/.dircontxtignore
	echo "*.log" >> test_dir/.dircontxtignore
	echo "specific_dir/specific_file_to_ignore.txt" >> test_dir/.dircontxtignore
	touch test_dir/ignored_file.tmp
	touch test_dir/some_app.log
	mkdir -p test_dir/ignored_folder
	touch test_dir/ignored_folder/something.txt
	mkdir -p test_dir/specific_dir
	touch test_dir/specific_dir/specific_file_to_ignore.txt
	touch test_dir/specific_dir/another_file.txt
	$(TARGET) test_dir
	@echo "--- Run complete ---"
	@echo "Output dircontxt file should be 'test_dir.dircontxt' in the project root."
	@echo "To inspect binary: hexdump -C test_dir.dircontxt"


# Run with debug logging enabled (if not already default).
# Our CFLAGS for 'all' already omit NDEBUG, so debug logs should be on.
# This target is essentially the same as 'run' unless CFLAGS are changed for 'all'.
debug_run: $(TARGET)
	@echo "Running $(TARGET) with debug output on ./test_dir (same as 'run' if NDEBUG not defined)..."
	$(MAKE) run # Just re-run the 'run' target, which will rebuild if necessary

# Build for release (optimized, no debug symbols, NDEBUG defined)
release: clean
	@echo "Building for release..."
	$(MAKE) CFLAGS="$(CFLAGS_RELEASE)" all
	@echo "Release build complete. Executable at $(TARGET)"


help:
	@echo "Available targets:"
	@echo "  all         : Build the $(TARGET) executable (default, debug build)."
	@echo "  clean       : Remove all build artifacts."
	@echo "  run         : Build and run $(TARGET) on a sample 'test_dir' (debug build)."
	@echo "  debug_run   : Alias for 'run' (ensures debug logs if CFLAGS are set for debug)."
	@echo "  release     : Build $(TARGET) for release (optimized, NDEBUG defined)."
	@echo "  help        : Show this help message."

# Tell Make that these are not files.
# .SECONDARY: $(OBJS) # Optional: Keeps .o files around if needed, not strictly necessary
