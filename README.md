Of course. The project is written in the **C programming language**, specifically following the **C11 standard**.

I have updated the entire project documentation to reflect all the recent changes, including a comprehensive guide to the advanced ignore file system. This new `README.md` is a complete replacement for the previous documentation files.

---

### `README.md`

```markdown
# dircontxt: Directory Context Snapshot Tool

**Language: C (C11 Standard)**

`dircontxt` is a high-performance command-line utility written in C that captures a complete, self-contained snapshot of a directory's structure and contents. It produces two distinct output files: a compact binary archive for machine use and a detailed text file specifically formatted for Large Language Models (LLMs), making it an ideal tool for project archiving, analysis, and AI-driven code introspection.

## Features

*   **High Performance**: Written in C for fast and efficient file processing.
*   **Recursive Directory Scanning**: Captures the entire hierarchy of files and subdirectories.
*   **Advanced Ignore System**: Utilizes a powerful three-tiered ignore system (`default` -> `global` -> `project`) with a `.gitignore`-like syntax, including negation.
*   **Dual-Output Format**:
    1.  A compact **binary (`.dircontxt`)** format for efficient storage.
    2.  A verbose **text (`.llmcontext.txt`)** format designed to be easily parsed and understood by AI models.
*   **Intelligent Binary Detection**: Identifies binary files and replaces their content with a placeholder in the text output to maintain readability.

---

## Installation (macOS)

Follow these steps to compile the application and set up the `dctx` command to be accessible from anywhere in your terminal.

### 1. Prerequisites

Ensure you have the **Xcode Command Line Tools** installed, which include `git`, `make`, and the `clang` compiler.
```bash
xcode-select --install
```

### 2. Clone and Compile the Project

Clone the repository and use the provided `Makefile` to compile the project. The `release` target is recommended for general use.

```bash
# Navigate to a directory for your development projects
cd ~/DEV

# Clone the repository (replace with your actual repository URL)
git clone https://github.com/your-username/dircontxt.git
cd dircontxt

# Compile the project and create an optimized executable
make release```
This creates the final executable at `build/bin/dircontxt`.

### 3. Set up the Global `dctx` Command

To make the command globally available, we'll create a simple wrapper script.

1.  **Get the executable's absolute path.** While inside the project directory, run:
    ```bash
    echo "$(pwd)/build/bin/dircontxt"
    ```
    Copy the full path that is printed (e.g., `/Users/yourname/DEV/dircontxt/build/bin/dircontxt`).

2.  **Create a script file for the command:**
    ```bash
    mkdir -p ~/.local/bin
    nano ~/.local/bin/dctx
    ```

3.  **Paste the following into the editor.** **Crucially, replace the placeholder in `APP_PATH` with the path you copied in the previous step.**
    ```sh
    #!/bin/zsh

    # --- CONFIGURATION ---
    # IMPORTANT: Replace this with the absolute path to YOUR executable.
    APP_PATH="/Users/yourname/DEV/dircontxt/build/bin/dircontxt"

    # --- SCRIPT LOGIC ---
    if [ ! -x "$APP_PATH" ]; then
      echo "Error: dircontxt executable not found at:" >&2
      echo "$APP_PATH" >&2
      echo "Please check the APP_PATH in ~/.local/bin/dctx" >&2
      exit 1
    fi

    # Use the first argument as the target, or default to the current directory ('.').
    TARGET_DIR="${1:-.}"

    # Execute the application
    "$APP_PATH" "$TARGET_DIR"
    ```
    Save and exit nano by pressing `Ctrl + X`, then `Y`, then `Enter`.

4.  **Make the script executable:**
    ```bash
    chmod +x ~/.local/bin/dctx
    ```

5.  **Add the script's directory to your Zsh PATH.** Open your Zsh configuration file:
    ```bash
    nano ~/.zshrc
    ```
    Add this line to the end of the file:
    ```sh
    # Add local bin directory for custom scripts
    export PATH="$HOME/.local/bin:$PATH"
    ```
    Save and exit.

6.  **Reload your shell configuration** to apply the changes:
    ```bash
    source ~/.zshrc
    ```
    Your `dctx` command is now ready to use from any directory.

---

## Usage Guide

#### Basic Syntax
```bash
dctx [directory_path]
```
*   `[directory_path]`: The path to the directory you wish to capture. If omitted, it defaults to the **current directory (`.`)**.

#### Output Location
The output files (`<dir_name>.dircontxt` and `<dir_name>.llmcontext.txt`) are always created in the **parent directory** of the target. This prevents the output files from being included in subsequent runs.

---

## Configuring Ignore Rules

`dctx` uses a powerful three-tiered system to exclude files and directories from the snapshot.

#### The Hierarchy of Rules

Rules are loaded from three sources in order of increasing precedence (later rules override earlier ones):

1.  **Default (Built-in) Rules - Lowest Priority**:
    The application has a hardcoded list of common patterns to ignore, including `.git/`, `node_modules/`, and `.DS_Store`. These are always active.

2.  **Global Ignore File - Medium Priority**:
    You can create a global ignore file at `~/.config/dircontxt/ignore`. Rules in this file apply to *every* project and override the defaults. This is perfect for editor configs or system files you always want to ignore.

3.  **Project Ignore File (`.dircontxtignore`) - Highest Priority**:
    A file named `.dircontxtignore` in the root of the directory you are capturing provides project-specific rules. These rules have the final say, overriding any conflicting global or default rules.

#### Rule Precedence: The Last Match Wins

The most important concept is that **the last rule that matches a file determines its fate.** This is how negation (`!`) works. If a file is ignored by a general rule but re-included by a later, more specific negation rule, it will be included in the final snapshot.

#### Pattern Syntax Guide

| Pattern Example         | Description                                                                                                                              |
| ----------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `# comments`            | Lines starting with `#` are ignored.                                                                                                     |
| `build/`                | A name ending in a `/` matches **only directories** with that name, anywhere in the project.                                             |
| `*.log`                 | A leading `*` acts as a suffix wildcard. This ignores any file or directory ending with `.log`.                                          |
| `my-file.tmp`           | A simple name with no slashes matches any file or directory with that name anywhere in the tree.                                         |
| `src/config.json`       | Patterns containing a `/` are matched against the full relative path from the project root. This only ignores `config.json` inside `src/`. |
| `!important.log`        | A leading `!` **negates** the pattern. This re-includes a file that was previously matched by an ignore rule.                              |
| `build/*`               | A wildcard at the end of a path ignores all files and folders *inside* the `build` directory, but not the directory itself.                |


#### Example Configuration

Imagine this `.dircontxtignore` file:
```
# Ignore all log files
*.log

# But re-include the important audit log
!logs/audit.log

# Ignore sensitive keys
secret.key
```
*   `debug.log` would be **ignored**.
*   `logs/audit.log` would be **included**, because the negation rule is the last one that matches it.
*   `secret.key` would be **ignored**.

---

## Building from Source

The project uses a standard `Makefile`. The most common targets are:
*   `make` or `make all`: Compiles a debug version of the executable.
*   `make release`: Cleans and compiles an optimized, stripped release executable.
*   `make test` or `make run`: Compiles and runs a comprehensive test case to validate ignore logic.
*   `make clean`: Removes all build artifacts and test outputs.
```
