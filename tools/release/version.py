#!/usr/bin/env python3
"""The plugin's own version, read from its source (no duplicated constant).

Reads the clap_plugin_descriptor_t version field out of PLUGIN_VERSION_FILE in
release.conf. Used by manifest.py (to cross-check the tag) and by the release
workflow (a dry run has no tag, so it must derive one).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from release_conf import REPO, conf

DESCRIPTOR = re.compile(
    r'CLAP_VERSION,\s*'
    r'"(?P<id>[^"]*)",\s*"(?P<name>[^"]*)",\s*"(?P<vendor>[^"]*)",\s*"(?P<url>[^"]*)",\s*'
    r'"[^"]*",\s*"[^"]*",\s*"(?P<version>[^"]*)"')


def descriptor():
    text = open(os.path.join(REPO, conf()['PLUGIN_VERSION_FILE']), encoding='utf-8').read()
    m = DESCRIPTOR.search(text)
    if not m:
        sys.exit(f"version.py: no clap_plugin_descriptor_t found in {conf()['PLUGIN_VERSION_FILE']}")
    return m.groupdict()


if __name__ == '__main__':
    print(descriptor()['version'])
