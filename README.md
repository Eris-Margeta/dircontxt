Of course. After a detailed review of the project files and my previous analysis, I have refined and expanded the documentation to be more comprehensive, accurate, and actionable.

My initial assessment was correct:
*   The file `src_py/config_models.py` is indeed redundant and should be removed to avoid confusion. The project correctly uses the modular models within the `src_py/config_models/` directory.
*   The workflow, technology stack, and roles of the different directories were accurately described.

I have expanded the following sections for greater clarity and practical use:
*   **Configuration Guide**: Added detailed tables explaining every key in `site_config.yaml` and the page-specific YAML files.
*   **Customization Guide**: Included a new, crucial section that provides a step-by-step guide for developers on how to add new pages and, more importantly, new reusable content sections.
*   **Usage Guide**: Clarified the parameters for the `just run` command.

Here is the final, comprehensive documentation for your project.

---

# Project Documentation: Python & Astro Static Site Generator

## 1. Project Overview

This project is a sophisticated **Static Site Generator (SSG)** framework. Its primary purpose is to take structured content defined in human-readable YAML files, process it using a powerful Python backend, and generate a complete, high-performance, and SEO-friendly website using the [Astro.js](https://astro.build/) framework.

The core philosophy is the **separation of concerns**:
*   **Content**: Managed exclusively in simple YAML files.
*   **Logic & Structure**: Defined and validated by the Python backend.
*   **Presentation**: Handled by Astro components and templates.

This architecture makes it incredibly efficient for developers to manage the site's structure and for non-technical users to update content without ever touching code.

## 2. Technologies Used

The project integrates a modern stack of tools, each chosen for a specific purpose.

| Technology | Purpose in this Project |
| :--- | :--- |
| **Python** | The **core engine** of the generator. It orchestrates the entire process: reading configuration, validating data against predefined models, preparing data for templating, and running the final build commands. |
| **YAML** | The **content format**. Used for all site configuration (`site_config.yaml`) and page content (`config/pages/*.yaml`) because it is easy for humans to read and write. |
| **Pydantic** | The **data validation layer**. It acts as a schema for all YAML files. By defining strict data models in Python, we guarantee that the configuration is always correct, preventing errors before they reach the templating stage. |
| **Jinja2** | The **templating engine**. It allows us to dynamically generate Astro source files (`.astro`, `.mjs`, etc.) by inserting the data processed by Python into predefined templates (`.j2` files). |
| **Astro** | The **frontend framework**. The final output of the Python generator is a complete Astro project. Astro is used for its excellent performance (shipping zero JavaScript by default) and its component-based architecture. |
| **Docker** | The **runtime environment**. Docker creates a consistent, isolated, and reproducible Linux container with all the necessary dependencies (a specific version of Python, Node.js, pnpm, etc.) pre-installed. This solves the "it works on my machine" problem and ensures the build process is identical everywhere. |
| **Just (`justfile`)**| The **command runner**. `just` provides a simple and clean way to run complex project commands. It acts as a user-friendly interface for building the Docker image, running the generator, and starting a local development server. |
| **pnpm** | The **Node.js package manager**. It is used within the Docker container and on the local machine to install Astro's dependencies efficiently. |

## 3. How It Works: The Generation Pipeline

The entire process is orchestrated by `src_py/generator/main.py`. Here is a step-by-step breakdown:

1.  **Configuration Loading**: The `config_loader.py` module reads the global `config/site_config.yaml` and all page configurations from `config/pages/`.
2.  **Data Validation**: As YAML is loaded, it is parsed and validated against the Pydantic models in `src_py/config_models/`. If any file is malformed, the process fails with a clear error.
3.  **Data Preparation**: `data_preparer.py` transforms the validated data into a "Jinja Context" – a dictionary accessible to the templates. This involves resolving SEO fallbacks, generating JSON-LD schemas, and structuring content for easy rendering.
4.  **Templating & Code Generation**: `templating.py` uses Jinja2 to render all `.j2` templates from `templates_astro/` into a new `astro_src/` directory, injecting the context data to create complete `.astro`, `.mjs`, and `.json` files.
5.  **Astro Build**: `build_utils.py` runs `pnpm install` and `pnpm run build` inside the generated `astro_src/` directory.
6.  **Final Output**: Astro compiles the site into static HTML/CSS/JS in a `dist/` folder. This folder's contents are then copied to the final `/output` directory.

## 4. Directory Structure and File Interconnection

#### `config/` - The Content Hub
*   `site_config.yaml`: The "brain" of the website. Defines global settings.
*   `pages/*.yaml`: The "body" of the website. Each file represents a single page.

#### `src_py/` - The Python Engine
*   **`config_models/`**: The **Schema/Blueprint**. These Pydantic models define the required structure for all YAML files.
*   **`generator/`**: The **Engine Room**.
    *   `main.py`: **Orchestrator** that runs the generation pipeline.
    *   `config_loader.py`: **Importer** for YAML files.
    *   `data_preparer.py`: **Processor** that creates the Jinja2 context.
    *   `templating.py`: **Renderer** that generates Astro code from templates.
    *   `build_utils.py`, `path_utils.py`: **Utilities** for running commands and determining paths.

#### `templates_astro/` - The Molds
This directory is a complete but *templated* Astro project. Its structure directly mirrors the final generated Astro source code. Jinja2 placeholders (`{{ ... }}`) are used to inject data.

#### `scripts/` - Developer Tools
*   `validate_configs.py`: A **Quality Assurance Tool** to quickly check all YAML files for errors without running a full build.
*   `validators/*.py`: **Specialized Inspectors** that handle specific validation tasks like checking for broken links or missing image assets.

#### Root Files
*   `Dockerfile`: The **Environment Recipe** defining the consistent Docker build environment.
*   `justfile`: The **Control Panel** for running common development and build commands.

## 5. Usage Guide

All primary interactions with the project are handled via the `justfile`.

#### 1. Validate Your Configuration
Before generating the site, always check your configuration for errors.
```bash
just validate
```
This script is your first line of defense, providing fast feedback on structural errors, broken internal links, missing image assets, and other common issues.

#### 2. Generate the Website
This is the main command to build the entire site from scratch.
```bash
just web```
This command builds the Docker image and runs the container, which executes the full Python generation pipeline. The final static website is placed in the `output/` directory.

#### 3. Run a Local Development Server
To preview your generated site with hot-reloading.
```bash
# 1. Generate the project first
just web

# 2. Run the dev server
just run web
```
*   **`project_folder`**: The argument `web` corresponds to the `project_name` defined in your `config/site_config.yaml`.
*   **What it does**: This command navigates into `output/web`, correctly reinstalls Node.js dependencies for your local operating system (e.g., macOS/Windows), and starts the Astro development server (`pnpm dev`). This step is crucial for local development after a Docker build.

#### 4. Clean the Output Directory
```bash
just clean```
Deletes the entire `output/` directory so you can start a fresh build.

## 6. Configuration Guide

### `config/site_config.yaml`
This file controls all global settings.

| Key | Type | Description |
| :--- | :--- | :--- |
| `project_name` | `string` | A short, machine-friendly name for the project. Used for the output directory name. |
| `site_domain` | `URL` | The full, canonical domain of your website (e.g., `https://www.yourdomain.com`). |
| `site_name` | `string` | The human-readable name of your website, used in SEO metadata. |
| `author` | `string` | The author's name, used in the footer and metadata. |
| `default_og_locale`| `string` | The default Open Graph locale (e.g., `en_US`). |
| `social_profiles`| `object` | Contains keys for social media handles and URLs (e.g., `twitter_username`, `linkedin_company_url`). |
| `languages` | `list` | A list of supported languages. Each item is an object with `code`, `name`, and optional `og_locale`. **The first language in the list is the default.** |
| `navbar_ctas` | `object` | Defines the global primary and secondary "Call to Action" buttons in the navigation bar. |
| `global_nav_links`| `object` | Defines the main navigation links for each language. The keys are language codes. |
| `astro_config_defaults`| `object` | Sets default values for Astro's configuration, such as `base_path` and `output_mode`. |

### Page Configuration (`config/pages/*.yaml`)
Each file defines a page and its content.

| Key | Type | Description |
| :--- | :--- | :--- |
| `page_id` | `string` | A unique, snake_case identifier for the page (e.g., `about_us`). **This is critical** as it's used to link pages together. |
| `is_conversion_page`| `boolean`| If `true`, the header and footer may render differently (e.g., a simpler footer). |
| `no_index` | `boolean`| If `true`, the page will be marked with `noindex` for search engines. |
| `tags` | `list` | A list of internal tags for categorization or special handling. |
| `en:`, `hr:`, etc. | `object` | A top-level key for each language code defined in `site_config.yaml`. |

Inside each language block:

| Key | Type | Description |
| :--- | :--- | :--- |
| `slug` | `string` | The URL slug for this page in this language (e.g., `about-us`). Use `null` for the homepage. |
| `seo` | `object` | Contains all SEO metadata for this language: `title`, `meta_description`, `keywords`, Open Graph (`og_*`), and Twitter (`twitter_*`) properties. |
| `sections`| `list` | The content of the page, defined as a list of section objects. Each object has a `type`, a unique HTML `id`, and a `content` block with fields specific to that section type. |

## 7. Customization and Extensibility

### How to Add a New Page
1.  **Create a New File**: Add a new YAML file in `config/pages/`, for example `services.yaml`.
2.  **Define Content**: Populate the file with a unique `page_id` (e.g., `services`) and add the content for each language under its respective key (`en:`, `hr:`, etc.).
3.  **Link the Page**: Add a reference to the new page in `config/site_config.yaml` under `global_nav_links` for each language to make it appear in the navigation menu.
4.  **Validate**: Run `just validate` to check for any errors.

### How to Add a New Section Type
This is an advanced task that involves touching multiple parts of the system. Let's say you want to create a new `video_embed` section.

1.  **Define the Pydantic Model**:
    *   Open `src_py/config_models/sections.py`.
    *   Create a new Pydantic class: `class VideoEmbedSectionContentModel(BaseModel): video_url: HttpUrl; caption: Optional[str] = None`.
    *   Add this new model to the `SectionContentType = Union[...]` list.
    *   Add a mapping in the `type_to_content_model_map` dictionary within the `SectionModel` validator: `"video_embed": VideoEmbedSectionContentModel`.
2.  **Update Validator Map**:
    *   Open `scripts/validators/utils.py`.
    *   Add a new entry to the `SECTION_TYPE_MAP` dictionary: `'video_embed': {'model': VideoEmbedSectionContentModel, 'astro_component': 'video-embed.astro'}`.
3.  **Create the Astro Component Template**:
    *   Create a new file: `templates_astro/src/components/sections/video-embed.astro.j2`.
    *   Write the Astro code to render the video. You can access the content via props:
        ```astro
        ---
        // templates_astro/src/components/sections/video-embed.astro.j2
        export interface Props {
          video_url: string;
          caption?: string;
        }
        const { video_url, caption } = Astro.props;
        ---
        <section>
          <iframe src={video_url} title="Embedded Video"></iframe>
          {caption && <p>{caption}</p>}
        </section>
        ```4.  **Use the New Section**:
    *   In any page YAML file (e.g., `homepage.yaml`), you can now add a new section:
        ```yaml
        - type: "video_embed"
          id: "promo-video"
          content:
            video_url: "https://www.youtube.com/embed/..."
            caption: "Our amazing new product."
        ```
5.  **Validate and Rebuild**: Run `just validate` to check your work, then `just web` to generate the site with the new component.
