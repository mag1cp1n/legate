# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.


# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

from os import getenv

import legate

SWITCHER_PROD = "https://docs.nvidia.com/legate/switcher.json"
SWITCHER_DEV = "http://localhost:8000/legate/switcher.json"
JSON_URL = SWITCHER_DEV if getenv("SWITCHER_DEV") == "1" else SWITCHER_PROD

# -- Project information -----------------------------------------------------

project = "NVIDIA legate.core"
if "dev" in legate.__version__:
    project += f" ({legate.__version__})"

copyright = "2021-2024, NVIDIA"
author = "NVIDIA Corporation"

# -- General configuration ---------------------------------------------------

extensions = [
    "breathe",
    "sphinx.ext.autodoc",
    "sphinx.ext.autosummary",
    "sphinx.ext.intersphinx",
    "sphinx.ext.mathjax",
    "sphinx.ext.napoleon",
    "sphinx.ext.doctest",
    "sphinx_copybutton",
    "myst_parser",
    "legate._sphinxext.settings",
]

suppress_warnings = ["ref.myst"]
exclude_patterns = ["BUILD.md"]
source_suffix = {".rst": "restructuredtext", ".md": "markdown"}

# -- Options for HTML output -------------------------------------------------

html_static_path = ["_static"]

# This is pretty kludgy but the nv theme is not publicly available to
# install on CI, etc. We will use the pydata theme in those situations
if getenv("NV_THEME") == "1":
    html_theme = "nvidia_sphinx_theme"
    html_theme_options = {
        "switcher": {
            "json_url": JSON_URL,
            "navbar_start": ["navbar-logo", "version-switcher"],
            "version_match": ".".join(legate.__version__.split(".", 2)[:2]),
        }
    }
else:
    html_theme = "pydata_sphinx_theme"
    html_theme_options = {
        "footer_start": ["copyright"],
        "github_url": "https://github.com/nv-legate/legate.core",
        # https://github.com/pydata/pydata-sphinx-theme/issues/1220
        "icon_links": [],
        "logo": {
            "text": project,
            "link": "https://nv-legate.github.io/legate.core/",
        },
        "navbar_align": "left",
        "navbar_end": ["navbar-icon-links", "theme-switcher"],
        "primary_sidebar_end": ["indices.html"],
        "secondary_sidebar_items": ["page-toc"],
        "show_nav_level": 2,
        "show_toc_level": 2,
        "navigation_with_keys": False,
    }

# -- Options for extensions --------------------------------------------------

autosummary_generate = True

breathe_default_project = "legate_core"
breathe_default_members = ("members", "protected-members")

copybutton_prompt_text = ">>> "

intersphinx_mapping = {
    "python": ("https://docs.python.org/3/", None),
    "numpy": ("https://numpy.org/doc/stable/", None),
}

mathjax_path = "https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js"

pygments_style = "sphinx"

myst_heading_anchors = 3


def setup(app):
    app.add_css_file("params.css")
