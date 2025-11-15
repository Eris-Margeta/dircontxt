```markdown
# dircontxt Documentation

`dircontxt` is a command-line utility designed to capture a complete, self-contained snapshot of a directory's structure and contents. It produces two distinct output files: a compact binary archive and a detailed, AI-friendly text file, making it an ideal tool for project archiving, analysis, and interaction with Large Language Models (LLMs).

The tool is written in C for performance and portability, using a standard `Makefile` for easy compilation.

## Features

*   **Recursive Directory Scanning**: Captures the entire hierarchy of files and subdirectories.
*   **Advanced Ignore Rules**: Uses a powerful three-tiered system (default, global, and project-specific `.dircontxtignore`) with a syntax similar to `.gitignore` to exclude unnecessary files.
*   **Dual-Output Format**:
    1.  A compact **binary (`.dircontxt`)** format for efficient storage.
    2.  A verbose **text (`.llmcontext.txt`)** format specifically designed for AI models.
*   **Binary File Detection**: Intelligently identifies binary files and replaces their content with a placeholder in the text output to maintain readability.

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


# Clone the repository (replace with your actual repository URL)
git https://github.com/Eris-Margeta/dircontxt
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
source ~/.zshrc```

### Step 7: Verify the Installation

You're all set! Verify that the command works by checking its version.

```bash
dctx -v
# You should see: [INFO] dircontxt v0.1.0 starting.
```

---

## Rebuilding the Project

After making changes to the source code, you should perform a clean rebuild to ensure all updates are compiled correctly. For general use, creating a release build is recommended as it is optimized for performance and size.

From your project's root directory, run the following commands in your terminal:

1.  **Clean all previous build artifacts:**
    ```bash
    make clean
    ```
2.  **Build the optimized release version:**
    ```bash
    make release
    ```
This will create the final, stripped executable at `build/bin/dircontxt`. Your `dctx` command will automatically use this new version the next time you run it.

---

## `dctx` Usage and Configuration Guide

### Basic Usage

The `dctx` command processes a directory and generates a `.dircontxt` binary file and a `.llmcontext.txt` text file.

**Syntax:**
```bash
dctx [directory_path]
```
*   `[directory_path]`: The path to the directory you want to capture. If omitted, it defaults to the **current directory (`.`)**.

**Output Location:**
The output files are always created in the **parent directory** of the target you specify. This is a deliberate design choice to prevent the output files from being included in subsequent runs.

**Examples:**

1.  **Capture the current project:**
    ```bash
    # Navigate into your project folder
    cd ~/DEV/my-project

    # Run the command
    dctx .
    ```
    *Output will be created at `~/DEV/my-project.dircontxt` and `~/DEV/my-project.llmcontext.txt`.*

2.  **Capture a different project by path:**
    ```bash
    # Run the command from anywhere, targeting a specific folder
    dctx ~/Documents/another-project
    ```
    *Output will be created at `~/Documents/another-project.dircontxt` and `~/Documents/another-project.llmcontext.txt`.*

### Configuring Ignore Rules

Your `dctx` command uses a powerful three-tiered system to determine which files and directories to exclude from the snapshot.

#### The Hierarchy of Rules

Rules are loaded from three sources, in order from lowest to highest priority:

1.  **Default (Built-in) Rules - Lowest Priority**:
    The application has a hardcoded list of common patterns to ignore, such as `.git/`, `node_modules/`, and `.DS_Store`. These are always active.

2.  **Global Ignore File - Medium Priority**:
    You can create a global ignore file at `~/.config/dircontxt/ignore`. Rules in this file apply to *every* directory you run `dctx` on and will override the default rules.

3.  **Project Ignore File (`.dircontxtignore`) - Highest Priority**:
    A file named `.dircontxtignore` in the root of the directory you are capturing provides project-specific rules. These are the most important rules and override any conflicting global or default rules.

#### Rule Precedence: The Last Match Wins

The most important concept is that **the last rule in the combined list that matches a file determines its fate.** This is what allows negation (`!`) to work. If a file is ignored by a general rule but re-included by a later, more specific negation rule, it will be included in the final snapshot.

#### Pattern Syntax Guide

Here is how to write patterns in your `.dircontxtignore` or global ignore file:

| Pattern Example         | Description                                                                                                                              |
| ----------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `# comments`            | Lines starting with `#` are ignored.                                                                                                     |
| `build/`                | A name ending in a `/` matches **only directories**. This ignores any directory named `build` anywhere in the project.                     |
| `*.log`                 | A leading `*` acts as a wildcard. This ignores any file or directory ending with `.log`.                                                 |
| `my-file.tmp`           | A simple name matches any file or directory with that name anywhere in the tree.                                                         |
| `src/config.json`       | Patterns containing a `/` are matched against the full relative path from the project root. This only ignores `config.json` inside `src/`. |
| `!important.log`        | A leading `!` **negates** the pattern. This re-includes a file that was previously ignored by a more general rule.                         |
| `build/*`               | A wildcard at the end of a path ignores all files and folders *inside* the `build` directory, but not the directory itself.                |

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
```
