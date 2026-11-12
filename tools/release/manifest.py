#!/usr/bin/env python3
"""Write oscairn-release.json (the release manifest) from the files in a dist directory.

    manifest.py <tag> <distdir> [outfile]        outfile: <distdir>/oscairn-release.json

<tag> is this repo's release tag, oscairn-vX.Y.Z (upstream v* tags exist, hence the
prefix; until the packages schema adopts it, source.tag stays the real tag name).
The version comes from the tag and must equal the version the plugin itself reports
in its clap_plugin_descriptor_t (version.py) and the CMake project version. The
builds rows come from every <NAME>-<platform>.clap found in <distdir>.
"""
import datetime
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from release_conf import REPO, conf, sha256
from version import descriptor

PLATFORMS = {'linux-arm64', 'linux-x86_64', 'macos-arm64', 'macos-x86_64', 'windows-x86_64'}


def git(*args):
    return subprocess.run(('git', '-C', REPO) + args, capture_output=True, text=True,
                          check=True).stdout.strip()


def clap_version():
    text = open(os.path.join(REPO, conf()['CLAP_VERSION_FILE']), encoding='utf-8').read()
    v = {k: re.search(rf'#define CLAP_VERSION_{k} (\d+)', text).group(1)
         for k in ('MAJOR', 'MINOR', 'REVISION')}
    return f"{v['MAJOR']}.{v['MINOR']}.{v['REVISION']}"


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit('usage: manifest.py <tag> <distdir> [outfile]')
    c = conf()
    tag, dist = sys.argv[1], sys.argv[2]
    out = sys.argv[3] if len(sys.argv) == 4 else os.path.join(dist, 'oscairn-release.json')

    m = re.fullmatch(re.escape(c['TAG_PREFIX']) + r'v([0-9]+\.[0-9]+\.[0-9]+)', tag)
    if not m:
        sys.exit(f"manifest.py: tag {tag!r} is not {c['TAG_PREFIX']}vX.Y.Z")
    version = m.group(1)

    plugin = descriptor()
    if plugin['version'] != version:
        sys.exit(f"manifest.py: tag {tag} says {version} but the plugin "
                 f"({c['PLUGIN_VERSION_FILE']}) reports {plugin['version']}")
    cmake = re.search(r'project\(\w+ VERSION ([0-9]+\.[0-9]+\.[0-9]+)',
                      open(os.path.join(REPO, 'CMakeLists.txt'), encoding='utf-8').read()).group(1)
    if cmake != version:
        sys.exit(f'manifest.py: tag says {version} but CMakeLists.txt says {cmake}')

    builds = []
    for name in sorted(os.listdir(dist)):
        m = re.fullmatch(re.escape(c['NAME']) + r'-([a-z0-9_]+-[a-z0-9_]+)\.clap', name)
        if not m:
            continue
        platform = m.group(1)
        if platform not in PLATFORMS:
            sys.exit(f'manifest.py: {name}: platform {platform!r} is not in the accepted set '
                     f'({", ".join(sorted(PLATFORMS))})')
        path = os.path.join(dist, name)
        builds.append({'platform': platform, 'file': name,
                       'sha256': sha256(path), 'size': os.path.getsize(path)})
    if not builds:
        sys.exit(f"manifest.py: no {c['NAME']}-<platform>.clap found in {dist}")

    license_files = []
    for pair in c['LICENSE_FILES'].split():
        path = os.path.join(dist, 'licenses', pair.split('=', 1)[1])
        if not os.path.isfile(path):
            sys.exit(f'manifest.py: {path} missing; run tools/release/licenses.sh first')
        license_files.append({'file': 'licenses/' + pair.split('=', 1)[1], 'sha256': sha256(path)})

    revision = git('rev-parse', 'HEAD')
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        sys.exit(f'manifest.py: HEAD is not a full commit hash: {revision!r}')
    oscairn_clap = json.load(open(os.path.join(REPO, c['OSCAIRN_CLAP_UPSTREAM']), encoding='utf-8'))['commit']

    manifest = {
        'format': 'oscairn.plugin-release',
        'version': 1,
        'package': {'id': c['PACKAGE_ID'], 'version': version, 'name': c['PACKAGE_NAME'],
                    'summary': c['PACKAGE_SUMMARY']},
        'plugins': [{'format': 'clap', 'id': plugin['id'], 'version': plugin['version'],
                     'name': plugin['name'], 'kind': c['PLUGIN_KIND']}],
        'builds': builds,
        'source': {'url': c['SOURCE_URL'], 'revision': revision, 'tag': tag},
        'license': c['LICENSE'],
        'licenseFiles': license_files,
        'clap': clap_version(),
        'oscairnClap': oscairn_clap,
    }
    with open(out, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)
        f.write('\n')
    print(f"manifest.py: wrote {out} ({len(builds)} build(s), package {version}, "
          f"commit {revision[:12]}, built {datetime.datetime.now(datetime.timezone.utc):%Y-%m-%d})")


if __name__ == '__main__':
    main()
