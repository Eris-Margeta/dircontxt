***

# dircontxt Documentation

`dircontxt` is a command-line utility designed to capture a complete, self-contained snapshot of a directory's structure and contents. It produces two distinct output files: a compact binary archive and a detailed, AI-friendly text file, making it an ideal tool for project archiving, analysis, and interaction with Large Language Models (LLMs).

The tool is written in C for performance and portability, using a standard `Makefile` for easy compilation.

## Features

*   **Recursive Directory Scanning**: Captures the entire hierarchy of files and subdirectories.
*   **Customizable Ignore Rules**: Uses a `.dircontxtignore` file, with a syntax similar to `.gitignore`, to exclude unnecessary files and folders (e.g., build artifacts, temporary files).
*   **Dual-Output Format**:
    1.  A compact **binary (`.dircontxt`)** format for efficient storage and machine-to-machine transfer.
    2.  A verbose **text (`.llmcontext.txt`)** format specifically designed to be parsed and understood by AI models.
*   **Binary File Detection**: Intelligently identifies binary files by extension and content analysis, replacing their content with a placeholder in the text output to maintain readability.

---

## Installation (macOS)

Follow these steps to compile the application and set up the `dctx` command to be accessible from anywhere in your terminal.

### Step 1: Prerequisites

Ensure you have the **Xcode Command Line Tools** installed, which include `git`, `make`, and the `clang` compiler. If you don't have them, run this command in your terminal:

```bash
xcode-select --install
```

### Step 2: Clone and Compile the Project

First, clone the project repository and compile the source code. The `release` target is recommended for general use as it creates an optimized executable.

```bash
# Go to a directory where you keep your development projects
cd ~/DEV

# Clone the repository (replace with your actual repository URL)
git clone https://github.com/your-username/dircontxt.git
cd dircontxt

# Compile the project for release
make release
```

This will create the executable at `build/bin/dircontxt`.

### Step 3: Create a Runner Script

To make the command globally available, we'll create a simple wrapper script.

1.  **Get the absolute path** to the compiled executable. While inside the `dircontxt` project directory, run:
    ```bash
    echo "$(pwd)/build/bin/dircontxt"
    ```
    Copy the output path. It should look something like `/Users/yourname/DEV/dircontxt/build/bin/dircontxt`.

2.  Create a standard directory for your personal scripts if you don't have one:
    ```bash
    mkdir -p ~/.local/bin
    ```

3.  Create the script file named `dctx`:
    ```bash
    nano ~/.local/bin/dctx
    ```

4.  Paste the following code into the nano editor. **Crucially, replace the placeholder path in `APP_PATH` with the one you copied in step 1.**

    ```sh
    #!/bin/zsh

    # --- CONFIGURATION ---
    # IMPORTANT: Replace this path with the absolute path to YOUR executable.
    APP_PATH="/Users/yourname/DEV/dircontxt/build/bin/dircontxt"

    # --- SCRIPT LOGIC ---
    if [ ! -x "$APP_PATH" ]; then
      echo "Error: dircontxt executable not found at:" >&2
      echo "$APP_PATH" >&2
      echo "Please check the APP_PATH in ~/.local/bin/dctx" >&2
      exit 1
    fi

    # Use the first argument as the target directory, or default to the current directory ('.').
    TARGET_DIR="${1:-.}"

    # Execute the application
    "$APP_PATH" "$TARGET_DIR"
    ```

5.  Save the file and exit nano by pressing `Ctrl + X`, then `Y`, then `Enter`.

### Step 4: Make the Script Executable

```bash
chmod +x ~/.local/bin/dctx
```

### Step 5: Add the Script Directory to your Zsh PATH

1.  Open your Zsh configuration file:
    ```bash
    nano ~/.zshrc
    ```

2.  Add the following line to the end of the file. This tells your shell to look for commands in your personal scripts folder.

    ```sh
    # Add local bin directory for custom scripts
    export PATH="$HOME/.local/bin:$PATH"
    ```

3.  Save and exit (`Ctrl + X`, `Y`, `Enter`).

### Step 6: Reload Your Shell Configuration

Apply the changes to your current terminal session:

```bash
source ~/.zshrc
```

### Step 7: Verify the Installation

You're all set! Verify that the command works by checking its version.

```bash
dctx -v
# You should see: [INFO] dircontxt v0.1.0 starting.
```

---

## Usage Guide

The `dctx` command is designed to be run from your terminal. It takes a single argument: the path to the directory you want to process.

### Basic Syntax

```bash
dctx <path_to_directory>
```

### How to Use It

The output files are always created in the **parent directory** of the target you specify. This prevents the output files from being included in subsequent runs on the same folder.

#### Example 1: Running on the Current Directory

This is the most common use case. Navigate to a project folder and run `dctx` on it.

```bash
# Navigate to a project you want to capture
cd ~/DEV/my-web-app

# Run the command on the current directory ('.')
dctx .

# Output will be created at ~/DEV/my-web-app.dircontxt
# and ~/DEV/my-web-app.llmcontext.txt
```

#### Example 2: Running on a Different Directory

You can run the command from anywhere by providing the path to the target directory.

```bash
# You are currently in your home directory
pwd
# /Users/yourname

# Run the command on a project located elsewhere
dctx ~/Documents/some_project

# Output will be created at ~/Documents/some_project.dircontxt
# and ~/Documents/some_project.llmcontext.txt
```

### Configuring Ignores with `.dircontxtignore`

To exclude files and directories from the snapshot, create a file named `.dircontxtignore` in the root of the target directory. The syntax is similar to `.gitignore`.

*   To ignore a directory, add its name followed by a slash: `node_modules/`
*   To ignore files by extension, use a wildcard: `*.log`
*   To ignore a specific file by name: `secret.key`
*   To ignore a file in a specific directory: `config/credentials.json`

**Example `.dircontxtignore` file:**

```
# Git directory
.git/

# Build artifacts
build/
dist/

# Dependencies
node_modules/

# Log files
*.log
*.tmp

# IDE / OS files
.vscode/
.idea/
.DS_Store

# Specific sensitive file
credentials.env
```

---

## Understanding the Output Files

For a target directory named `my-project`, `dctx` will generate two files:

### 1. The Binary Archive: `my-project.dircontxt`

This is a compact, machine-readable archive containing the complete directory snapshot.

*   **Purpose**: It serves as the single source of truth for the directory's state at the time of capture. It is optimized for storage and programmatic access.
*   **Structure**:
    1.  **Signature**: A unique 8-byte header (`DIRCTXTV`) to identify the file type.
    2.  **Header Section**: A serialized representation of the entire directory tree, including metadata for every file and folder (paths, timestamps, etc.).
    3.  **Data Section**: The concatenated, raw contents of every file in the tree.

This file is used by the `dctx` tool itself to generate the text-based LLM snapshot.

### 2. The LLM Snapshot: `my-project.llmcontext.txt`

This is a verbose, structured text file designed to be easily understood by an AI, like Gemini.

*   **Purpose**: To provide a complete and easily parsable context of a software project to an AI for tasks like code review, documentation, debugging, or analysis.
*   **Structure**:
    *   `[DIRCONTXT_LLM_SNAPSHOT_V1.2]`: A version header.
    *   `<INSTRUCTIONS>`: A short guide explaining how to read the file format.
    *   `<DIRECTORY_TREE>`: A manifest of all files and directories. Each entry includes:
        *   `[D]` for directory or `[F]` for file.
        *   The relative path of the item.
        *   A unique `ID` (e.g., `F001`, `D002`) used to link to its content.
        *   The `MOD` (Unix modification timestamp) and `SIZE` (in bytes).
    *   `<FILE_CONTENT_START ID="...">` and `</FILE_CONTENT_END>`: Blocks that contain the full content of each text file, linked by the ID from the directory tree.
    *   **Binary Placeholders**: If a file is identified as binary, its content is replaced with a placeholder (e.g., `[BINARY CONTENT PLACEHOLDER - Size: 13072 bytes]`) to keep the snapshot clean and readable.
