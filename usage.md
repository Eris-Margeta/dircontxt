Of course. Now that the code is fixed and the advanced ignore system is in place, it's time to put it all together.

Here is a complete guide covering how to rebuild the project and a detailed documentation on the correct usage of the `dctx` command, with a focus on configuring the new, powerful ignore system. This can replace the relevant sections in your `README.md`.

---

### Rebuilding the Project

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

### `dctx` Usage and Configuration Guide

#### Basic Usage

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

---

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

#### Complete Example

Imagine this file structure:

```
my-app/
├── .git/
│   └── config
├── node_modules/
│   └── some-lib/
├── logs/
│   ├── debug.log
│   └── audit.log
├── src/
│   └── main.c
└── secret.key```

And this `.dircontxtignore` file inside `my-app/`:

```
# Ignore all log files
*.log

# But re-include the important audit log
!audit.log

# Ignore sensitive keys
secret.key
```

When you run `dctx my-app`, here is how the rules would be applied:

*   `.git/config`: **Ignored**. Matched by the hardcoded default rule for `.git/`.
*   `node_modules/`: **Ignored**. Matched by the hardcoded default rule.
*   `logs/debug.log`: **Ignored**. Matched by the `*.log` rule in the project file.
*   `logs/audit.log`: **Included**. It is first matched by `*.log` (marking it for ignore), but the *last* rule that matches it is `!audit.log`, which negates the ignore and re-includes it.
*   `src/main.c`: **Included**. No ignore rule matches it.
*   `secret.key`: **Ignored**. Matched by the `secret.key` rule in the project file.
