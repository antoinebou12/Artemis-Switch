from datetime import datetime

project = "Artemis Switch"
copyright = f"2024-{datetime.now().year}, antoinebou12"
author = "antoinebou12"

extensions = [
    "myst_parser",
    "sphinx_immaterial",
]

myst_enable_extensions = [
    "colon_fence",
    "deflist",
]

source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

root_doc = "index"

templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

html_theme = "sphinx_immaterial"
html_title = "Artemis Switch"
html_baseurl = "https://antoinebou12.github.io/Artemis-Switch/"

html_theme_options = {
    "palette": [
        {
            "media": "(prefers-color-scheme: light)",
            "scheme": "default",
            "primary": "red",
            "accent": "red",
            "toggle": {
                "icon": "material/brightness-7",
                "name": "Switch to dark mode",
            },
        },
        {
            "media": "(prefers-color-scheme: dark)",
            "scheme": "slate",
            "primary": "red",
            "accent": "red",
            "toggle": {
                "icon": "material/brightness-4",
                "name": "Switch to light mode",
            },
        },
    ],
    "site_url": "https://antoinebou12.github.io/Artemis-Switch/",
    "repo_url": "https://github.com/antoinebou12/Artemis-Switch",
    "repo_name": "antoinebou12/Artemis-Switch",
    "icon": {"repo": "fontawesome/brands/github"},
    "features": [
        "content.tabs.link",
        "navigation.footer",
        "navigation.tabs",
        "navigation.top",
        "navigation.tracking",
        "search.highlight",
        "search.share",
        "toc.follow",
        "toc.sticky",
    ],
    "font": {
        "text": "Roboto",
        "code": "Roboto Mono",
    },
}

html_static_path = ["_static"]
html_css_files = ["css/custom.css"]
