Of course. Here is an extremely detailed developer guide that breaks down the project's execution flow, data lifecycle, and the intricate ways in which the files and modules work together.

---

# Developer's Guide: The Python-Astro Generator

## 1. Introduction for Developers

This guide provides a deep dive into the internal mechanics of the static site generator. It is intended for developers who need to modify, extend, or debug the system. We will trace the complete execution and data flow, from the initial command to the final HTML output, explaining the role and interaction of each component in detail.

The architecture is designed to be robust, maintainable, and scalable, built on a clear separation of environment, logic, and data.

---

## 2. The Grand Tour: Execution Flow & Data Lifecycle

We will trace the entire process initiated by the `just web` command.

### Stage 1: The Entry Point & Environment Setup

The process begins not in Python, but in the command line and Docker.

1.  **Command Execution (`justfile`)**:
    *   A developer runs `just web`.
    *   The `justfile` resolves this to the `_generate_project` recipe. This recipe is the master command sequence.

2.  **Docker Image Build (`Dockerfile`)**:
    *   `_generate_project` first calls `just build-docker`.
    *   Docker reads the `Dockerfile` to build (or use the cached) image named `my-astro-scaffolder`.
    *   **Inside the `Dockerfile`**:
        *   A specific Python version (`3.12-slim-bookworm`) is used as a base for consistency.
        *   System dependencies like `curl` and `git` are installed.
        *   **Node.js Environment**: Crucially, it does *not* use the OS's Node.js. It installs NVM (Node Version Manager) to lock in a specific `NODE_VERSION` (`20.10.0`). This prevents build issues from arising due to Node.js version mismatches.
        *   `pnpm` and `astro` are installed globally within this controlled Node.js environment.
        *   A non-root user (`appuser`) is created and used for security best practices.
        *   Python dependencies from `requirements.txt` are installed via `pip`.
        *   Finally, the entire project directory is copied into the `/app` directory inside the container.

3.  **Container Execution**:
    *   The `_generate_project` recipe's final step is `docker run --rm -v "$(pwd)/output:/app/output" my-astro-scaffolder`.
    *   **`docker run`**: This command starts a new container from the `my-astro-scaffolder` image.
    *   **`--rm`**: The container is automatically removed after it exits, keeping the system clean.
    *   **`-v "$(pwd)/output:/app/output"`**: This is the **most critical part of the Docker command**. It creates a **volume mount**, mapping the `output` directory in your local project folder to the `/app/output` directory inside the container. This is the bridge that allows the static files generated *inside* the isolated container to appear on your local machine.
    *   **`my-astro-scaffolder`**: The image to use.
    *   The container executes the `CMD` defined in the `Dockerfile`: `["python3", "-m", "src_py.generator.main"]`. This command is the official entry point into the Python application.

### Stage 2: Python Application Startup (`src_py/generator/main.py`)

The `main.py` script acts as the central orchestrator. Its `generate_site()` function executes the following steps in sequence:

1.  **Load Site Config**:
    *   `load_site_config(SITE_CONFIG_PATH)` is called.
    *   **File Involved**: `src_py/generator/config_loader.py`.
    *   **Data Flow**:
        *   Reads `config/site_config.yaml` as raw text.
        *   Uses `PyYAML` to parse the text into a standard Python dictionary.
        *   This dictionary is then passed to `SiteConfigModel.model_validate()`.
        *   **Pydantic (`src_py/config_models/site.py`)** takes over, validating every field, running custom validators (like ensuring all languages in `navbar_ctas` are defined), and coercing types (e.g., turning a string into an `HttpUrl` object).
        *   **Output**: A fully validated, type-safe `SiteConfigModel` object is returned. If validation fails, the program exits with a critical error.

2.  **Load All Page Configs**:
    *   `load_all_page_configs(PAGES_CONFIG_DIR)` is called.
    *   **File Involved**: `src_py/generator/config_loader.py`.
    *   **Data Flow**:
        *   It scans the `config/pages/` directory for all `.yaml` files.
        *   For each file, it performs a preliminary validation against `PageConfigFileStructure` to ensure the essential `page_id` field exists and is valid. This allows it to build a dictionary of raw page data.
        *   **Output**: A dictionary where keys are the `page_id`s and values are the *raw, unvalidated dictionaries* for each page (`Dict[str, Dict[str, Any]]`). Full content validation happens later.

3.  **Pre-calculate URL Slugs**:
    *   `get_page_slugs_map()` is called.
    *   **File Involved**: `src_py/generator/config_loader.py`.
    *   **Logic**: It iterates through the raw page data and the site languages to create a lookup table: `Dict[lang_code, Dict[page_id, slug]]`. This map is essential for resolving internal links correctly in the templates later.

4.  **Prepare Global Templating Context**:
    *   A `global_jinja_context` dictionary is created.
    *   **Data Content**: It's populated with `project_name`, the entire `site_config` object (converted back to a dictionary via `.model_dump()`), and the `page_slugs_map`. This dictionary is the foundational data available to *all* static templates (like `header.astro.j2`).

5.  **Generate Static Project Files**:
    *   `process_static_templates_recursively()` is called.
    *   **File Involved**: `src_py/generator/templating.py`.
    *   **Logic**: This function walks the entire `templates_astro/` directory. For every `.j2` file it finds (that isn't explicitly skipped, like the page templates), it renders it using the `global_jinja_context` and writes the output to the corresponding location in the `/app/astro_src` directory. Non-`.j2` files are copied directly. This step creates the foundational Astro project structure.

6.  **Generate Dynamic Pages (The Main Loop)**:
    *   The script now iterates through the `all_page_configs_raw` dictionary from step 2.
    *   For each page, it then loops through every `language` defined in the `site_config`.
    *   **Inside the loops**:
        *   It finds the correct language block in the page's YAML data (with a fallback to the default language).
        *   It validates this block against the `PageLangContentModel`. This is where the deep validation of `seo` and `sections` happens.
        *   **Crucial Step**: It calls `prepare_page_render_context()`.

### Stage 3: Deep Dive - The Data Transformation (`data_preparer.py`)

This is where raw, validated data is transformed into a format perfectly tailored for the frontend templates.

1.  **Function Call**: `prepare_page_render_context()` is called with the `page_id`, the validated `page_config_struct` and `page_lang_content` models, `site_config`, and the `global_jinja_context`.

2.  **Data Hydration and Fallbacks**:
    *   It reads the `SeoMetadataModel` from the `page_lang_content`.
    *   It applies business logic for fallbacks. For example: `og_title = seo_data.og_title or main_seo_title`. This ensures that even if optional fields are omitted in YAML, the templates will still receive a valid value.

3.  **Complex Data Generation**:
    *   **Canonical URL**: It uses the `page_slugs_map` and language information to construct the full, absolute canonical URL for the current page.
    *   **JSON-LD Schema**: It checks the page's sections. If a section of `type: "faq"` is found, it calls `_generate_faq_json_ld_main_entity` to create a valid `FAQPage` schema.org object. The list of generated schemas is then converted to a minified JSON string.

4.  **Section Props Preparation**:
    *   It iterates through the `sections` list from the `PageLangContentModel`.
    *   For each `section_model_instance`, it calls `prepare_section_props_for_astro()`.
    *   This helper function takes the Pydantic model for the section's content (e.g., `HeroSectionContentModel`) and calls `.model_dump()` on it. This converts the type-safe Python object into a clean dictionary.
    *   **Output**: It builds the `SECTIONS_DATA` list, where each item looks like `{ "type": "hero", "id": "hero-main", "props": {"headline": "...", "subheadline": "..."} }`.

5.  **Final Context Assembly**: It combines the global context with all the newly generated page-specific data into a single `page_render_ctx` dictionary. This dictionary is the complete set of data needed to render one specific `.astro` page file.

### Stage 4: Code Generation & Final Build

1.  **Determine Output Path**:
    *   `determine_page_output_path()` is called.
    *   **File Involved**: `src_py/generator/path_utils.py`.
    *   **Logic**: Based on the `page_id`, `slug`, and `lang_code`, it calculates the correct file path according to Astro's file-based routing rules (e.g., `/en/about-us/index.astro`).

2.  **Render the Page Template**:
    *   `render_and_write_template_file()` is called with the Jinja environment, the path to the generic page template (`src/pages/generic-page.astro.j2`), the calculated output path, and the comprehensive `page_render_ctx`.
    *   **File Involved**: `src_py/generator/templating.py`.
    *   **Logic**: Jinja2 renders the template, filling in all placeholders with the data from the context. The resulting `.astro` file is written to the `/app/astro_src/src/pages/` directory.

3.  **Astro Build Process**:
    *   After the loops complete, `main.py` calls the functions from `build_utils.py`.
    *   `install_dependencies()` runs `pnpm install` in `/app/astro_src`.
    *   `build_astro_project()` runs `pnpm run build`. Astro takes all the generated `.astro` files and components, compiles them, bundles assets, and outputs a production-ready static site into `/app/astro_src/dist`.

4.  **Final Output Copy**:
    *   `copy_dist_to_output()` copies the entire contents of `/app/astro_src/dist` to `/app/output`.
    *   Because of the volume mount (`-v`), these files instantly appear in the `output/` directory on your local machine.

The process is now complete. The journey from a simple YAML key to a final HTML element is a testament to the structured flow of data through validation, transformation, and templating.
