# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))
from sphinx.builders.html import StandaloneHTMLBuilder
import subprocess, os

# Doxygen
subprocess.call('doxygen Doxyfile.in', shell=True)

# -- Project information -----------------------------------------------------

project   = 'Telescope Looking Glass Proxy'
copyright = '2022-2023 Tim Dettmar and contributors'
author    = 'Tim Dettmar and contributors'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.intersphinx',
    'sphinx.ext.autosectionlabel',
    'sphinx.ext.todo',
    'sphinx.ext.coverage',
    'sphinx.ext.mathjax',
    'sphinx.ext.ifconfig',
    'sphinx.ext.viewcode',
    'sphinx_sitemap',
    'sphinx.ext.inheritance_diagram',
    'sphinx_toolbox.collapse',
    'breathe'
]

highlight_language = 'none'

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'sphinx_rtd_theme'
html_theme_options = {
    'canonical_url': '',
    'analytics_id': '',
    'display_version': False,
    'prev_next_buttons_location': 'bottom',
    'style_external_links': False,
    
    'logo_only': True,

    # Toc options
    'collapse_navigation': True,
    'sticky_navigation': True,
    'navigation_depth': 4,
    'includehidden': True,
    'titles_only': False
}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']
html_logo = "telescope-logo.png"

breathe_projects = {
	"Telescope Looking Glass Proxy": "_BUILD/xml/"
}
breathe_default_project = "Telescope Looking Glass Proxy"
breathe_default_members = ('members', 'undoc-members')

# Get the current git version

ver = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"])
origin = subprocess.check_output(["git", "remote", "get-url", "origin"])

with open("subs.rst", "w") as f:

    if ver is not None:
        ver = ver.decode("utf-8").replace("\n", "").strip()
        f.write(f".. |GITVER| replace:: {ver}\n")
    else:
        f.write(".. |GITVER| unicode:: U+0020\n")
    
    if origin is not None:
        origin = origin.decode("utf-8").replace("\n", "").strip()
        f.write(f".. |GITURL| replace:: {origin}\n")
    else:
        f.write(".. |GITURL| replace:: https://github.com/telescope-proj/lgproxy\n")


# Add the imprint based on the input environment variable. This is possibly
# required for upstream LGProxy due to § 5 TMG, but forks should add their own
# if they are required to by applicable laws.

no_imp = """.. note::
    | No imprint is configured.
    | Telescope Project contributors are not liable for the content of any forks.
    | Please set the environment variable ``IMPRESSUM_RST``
    | Alternatively, modify ``docs/legal/texts/impressum.rst``
"""

imp = os.getenv("IMPRESSUM_RST")
if imp is not None:
    imp = imp.replace("\\n", "\n")
    with open("./legal/texts/impressum.rst", "w") as f:
        f.write(imp)
else:
    if not os.path.exists("./legal/texts/impressum.rst"):
        with open("./legal/texts/impressum.rst", "w") as f:
            f.write(no_imp)