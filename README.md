# dircontxt: An Intelligent Context Builder for LLMs

**`dircontxt` is a command-line utility that creates structured, versioned, and diff-aware snapshots of codebases. It is specifically designed to provide Large Language Models (LLMs) with a complete and easily navigable context of a software project.**

The tool addresses a primary challenge in using AI for code analysis: providing comprehensive project context in a way that is both token-efficient and understandable to the model. It transforms a complex directory into a single, self-contained text file that allows an AI to grasp the project's architecture, navigate its files, and track its evolution over time.

### Structured for Clarity and Navigation

Instead of an unstructured block of text, `dircontxt` provides the LLM with a clear, machine-readable manifest. This allows the model to "see" the project layout and jump directly to relevant files using unique IDs.

**Example Output Structure:**
```
[DIRCONTXT_LLM_SNAPSHOT_V1.2]

<DIRECTORY_TREE>
[D]  (ID:ROOT, ...)
  [F] Makefile (ID:F001, ...)
  [D] src (ID:D002, ...)
    [F] src/main.c (ID:F003, ...)
    [F] src/utils.h (ID:F004, ...)
</DIRECTORY_TREE>

<FILE_CONTENT_START ID="F003" PATH="src/main.c">
// The full content of main.c ...
</FILE_CONTENT_END>

<FILE_CONTENT_START ID="F004" PATH="src/utils.h">
// The full content of utils.h ...
</FILE_CONTENT_END>
```
This format is significantly more effective than simple concatenation, enabling more precise and context-aware analysis by the AI.

## Core Features

-   **Structured Manifest**: Generates a clear directory tree with unique IDs for every file and folder, enabling efficient navigation by the LLM.
-   **Token Efficiency**: Intelligently detects and excludes binary files, replacing their content with a simple placeholder to save valuable context window space.
-   **Clipboard Integration**: Instantly copy a project's entire context to the clipboard for immediate use with an LLM, leaving no files behind.
-   **Automatic Versioning & Diffing**: Automatically versions each snapshot and, upon detecting changes, generates a concise diff file that highlights additions, modifications, and removals.
-   **Hierarchical Ignore System**: A powerful three-tiered ignore system (`.gitignore`-like syntax) provides precise control over which files are included in the snapshot.
-   **Cross-Platform**: Written in C with a simple `Makefile` for easy compilation on POSIX-compliant systems like macOS and Linux.

---

## Installation

The tool is built from source using a standard C compiler and `make`. The result is a single, self-contained executable.

### 1. Prerequisites

Ensure you have the necessary build tools installed on your system.

-   **For macOS:**
    Install the Xcode Command Line Tools, which provide `git`, `clang`, and `make`.
    ```bash
    xcode-select --install
    ```

-   **For Debian / Ubuntu Linux:**
    Install the `build-essential` package, which provides `git`, `gcc`, and `make`. For clipboard functionality, you will also need `xclip` (for X11) or `wl-copy` (for Wayland).
    ```bash
    sudo apt-get update && sudo apt-get install build-essential git xclip -y
    ```

### 2. Compile and Install

These commands will clone the repository, compile an optimized executable, and install it to `/usr/local/bin`, making it available system-wide.

```bash
# Clone the repository
git clone https://github.com/your-username/dircontxt.git
cd dircontxt

# Compile the optimized release version
make release

# Install the binary to a standard system path
# This makes the 'dctx' command available everywhere.
# Note: This may require administrator privileges.
sudo cp build/bin/dircontxt /usr/local/bin/dctx

# Verify the installation by checking the version
dctx --version
```

### 3. Add to Shell PATH (If Necessary)

The `/usr/local/bin` directory is in the default `PATH` on most systems. If the `dctx --version` command fails, you may need to add this directory to your shell's configuration file.

-   **For Zsh (`.zshrc`):**
    ```bash
    echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.zshrc
    source ~/.zshrc
    ```

-   **For Bash (`.bash_profile` or `.bashrc`):**
    ```bash
    echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.bash_profile
    source ~/.bash_profile
    ```

---

## First-Time Setup: Configuration

`dircontxt` is controlled by two optional configuration files in `~/.config/dircontxt/`. It is recommended to create these on first use.

1.  **Create the Configuration Directory:**
    ```bash
    mkdir -p ~/.config/dircontxt
    ```

2.  **Create the `config` File:**
    This file controls the program's output behavior when writing files.
    ```bash
    cat << EOF > ~/.config/dircontxt/config
    # ---------------------------------------------------
    # dircontxt Global Configuration
    # ---------------------------------------------------
    # OUTPUT_MODE: Controls which user-facing text files are generated.
    # The binary .dircontxt file is always created to enable versioning.
    #
    #   - both:   (Default) Creates the .dircontxt and the .llmcontext.txt files.
    #   - binary: Creates only the .dircontxt file and removes old text/diff files.
    OUTPUT_MODE=both
    EOF
    ```

3.  **Create the Global `ignore` File:**
    This file provides a universal set of ignore patterns for all projects, similar to a global `.gitignore`.
    ```bash
    cat << 'EOF' > ~/.config/dircontxt/ignore
    # Universal Global Ignore Rules for 'dircontxt'

    # Version Control
    .git/
    .svn/
    .hg/

    # Node.js
    node_modules/
    .pnpm-store/
    .npm/
    dist/
    build/
    .next/
    .output/
    .astro/
    .cache/

    # Python
    .venv/
    venv/
    env/
    __pycache__/
    *.pyc
    .pytest_cache/
    .coverage
    .ruff_cache/
    .mypy_cache/

    # C / C++ / Rust
    build/
    target/
    cmake-build-*/
    *.o
    *.a
    *.so

    # Deployment & Platform Artifacts
    .vercel/
    .netlify/
    .serverless/
    .terraform/

    # OS & Editor Files
    .DS_Store
    Thumbs.db
    .idea/
    .vscode/*
    !/.vscode/extensions.json
    !/.vscode/launch.json
    *.swp

    # Logs & Environment
    logs/
    *.log
    .env
    .env.local
    !.env.example
    EOF
    ```

---

## Usage Guide

The command takes the path to the directory you wish to snapshot and an optional flag.

```bash
dctx <directory_path> [options]
```

**Arguments & Options:**
-   `directory_path`: The directory to snapshot. Defaults to the current directory (`.`) if omitted.
-   `-c, --clipboard`: Copies the full context to the system clipboard instead of writing a `.llmcontext.txt` file. This mode is "traceless" and automatically deletes the temporary binary file after execution, leaving no artifacts on disk.
-   `-h, --help`: Shows the help message.
-   `-v, --version`: Shows the application version.

**Example 1: Snapshotting to a file (default behavior)**
```bash
# Snapshot the current project
cd ~/DEV/my-project
dctx .
```

**Example 2: Copying a project's context directly to clipboard**
```bash
# Analyze a project from another location without creating any files
dctx ~/Documents/another-project --clipboard
```

### Output Location and Permissions

When writing files, the output (`.dircontxt`, `.llmcontext.txt`, and diffs) is always created in the **parent directory** of the target. This design prevents the output from being included in subsequent snapshots.

Therefore, you must have **write permissions** for the parent directory of your target folder.

---

## Understanding the Output Files

### 1. The Binary State File (`.dircontxt`)

This file is the core of the versioning system.

-   **Purpose**: A compact, machine-readable archive of the project's state. It serves as the "memory" of the last run, enabling comparison for diff generation.
-   **IMPORTANT**: **Do not delete this file between runs.** Deleting it will reset the versioning, and the next snapshot will start over at `V1` instead of creating an incremental version and a diff file. (This file is automatically cleaned up when using `--clipboard` mode).

### 2. The LLM Snapshot (`.llmcontext.txt`)

This is the primary output file intended for use with an LLM.

-   **Purpose**: Provides a complete, structured, and single-file view of the project.
-   **Header**: Contains a version number (e.g., `V1.2`) that is automatically incremented with each run.
-   **Directory Tree**: A manifest of all included files and directories, each assigned a unique ID.
-   **File Content**: The full content of every text file, enclosed in `<FILE_CONTENT_START>` blocks that reference the ID from the manifest.

### 3. The Diff File (`-diff.txt`)

This file is generated only when changes are detected between the current and previous snapshots.

-   **Purpose**: Provides a concise summary of changes, ideal for focused analysis of recent modifications.
-   **Contents**:
    1.  A header showing the version change (e.g., `V1.1 -> V1.2`).
    2.  A `<CHANGES_SUMMARY>` block listing all added, modified, and removed files.
    3.  The complete, updated `<DIRECTORY_TREE>`.
    4.  The full content of **only the files that were added or modified**.

## The Ignore System

`dircontxt` uses a three-tiered hierarchy to determine which files to exclude. Rules are processed in order, and the **last rule that matches a file determines its inclusion or exclusion**.

1.  **Hardcoded Defaults (Lowest Priority)**
    The program has a built-in list of essential patterns to always ignore, such as `.git/`, `node_modules/`, and `.DS_Store`.

2.  **Global Ignore File (Medium Priority)**
    The file at `~/.config/dircontxt/ignore` applies to all projects. It is the ideal place for editor-specific or system files. Rules here override the hardcoded defaults.

3.  **Project Ignore File (Highest Priority)**
    A `.dircontxtignore` file in the root of the target directory provides project-specific rules. It has the highest precedence and can override any global or default rules (e.g., using a negation pattern like `!important.log` to re-include a file that was globally ignored).

## For Developers

If you modify the source code, you can use the `Makefile` to rebuild.

-   `make`: Build a debug version.
-   `make release`: Build an optimized release version.
-   `make clean`: Remove all build artifacts.
