#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# check-network-pins.sh [--root <dir>] -- refuse a network clone this
# repository has not pinned.
#
# The failure it was written for: two JPEG2000 sources that end up inside the
# signed AppImage were cloned from a default branch and from whatever branch
# qmake reported, so two builds of one tag produced different bytes and the
# signature could not tell. They are now fetched by commit, from
# ci/pins/*.txt -- in two workflows, and a pin applied to one and not the other
# builds a different AppImage in CI than the one the release signs. Green CI
# over an artefact nobody ships.
#
# What it reads: every `run:` block of every workflow, parsed as YAML, and in
# it every `git clone` and `git fetch`. A block that acquires something over
# the network passes only if it is pinned, which means both:
#   * the commit comes from a pin file -- the block reads ci/pins/<file>.txt,
#     or names a forty-hex commit that a pin file carries; and
#   * the fetched HEAD is asserted (`rev-parse HEAD`) rather than trusted.
# A forty-hex commit in such a block that no pin file carries is a failure on
# its own. A pin file row whose commit is not forty hex digits is a failure:
# a tag in that column is the thing a pin exists to prevent.
#
# Exit codes -- a consumer writes the condition as `rc = 0`, never "not 1":
#   0  every clone and fetch is pinned and asserted
#   1  one is not (named by file and line), or a pin row is malformed
#   2  cannot judge: no workflows, no clone or fetch in any of them (a scan
#      that measured nothing is unmeasured, not clean), a clone with no
#      ci/pins directory to judge it against, unreadable YAML, or no python3
#      with PyYAML on this host
set -uo pipefail

root="."
while [ $# -gt 0 ]; do
    case "$1" in
        --root) root="${2:-}"; shift 2 || { echo "FATAL: --root needs a directory" >&2; exit 2; } ;;
        *) echo "FATAL: usage: $(basename -- "$0") [--root <dir>]" >&2; exit 2 ;;
    esac
done
[ -d "$root" ] || { echo "FATAL: $root is not a directory -- cannot judge" >&2; exit 2; }

command -v python3 >/dev/null 2>&1 \
    || { echo "FATAL: python3 is not on PATH -- cannot judge" >&2; exit 2; }

exec python3 - "$root" <<'PYEOF'
import glob, os, re, sys

# A crash in here is "could not judge", never a verdict: Python exits 1 on an
# uncaught exception, and 1 is what a real finding looks like.
def _unjudged(kind, exc, tb):
    import traceback
    traceback.print_exception(kind, exc, tb)
    print("FATAL: check-network-pins crashed -- cannot judge", file=sys.stderr)
    sys.exit(2)
sys.excepthook = _unjudged
try:
    import yaml
except ImportError:
    print("FATAL: PyYAML is not installed -- cannot judge", file=sys.stderr)
    sys.exit(2)

root = sys.argv[1]
HEX = re.compile(r"\b[0-9a-f]{40}\b")
ACQ = re.compile(r"\bgit\b(?:\s+-C\s+\S+)*\s+(clone|fetch)\b")
PINREF = re.compile(r"ci/pins/[A-Za-z0-9._-]+\.txt")

wfs = sorted(glob.glob(os.path.join(root, ".github/workflows/*.yml"))
             + glob.glob(os.path.join(root, ".github/workflows/*.yaml")))
if not wfs:
    print(f"FATAL: no workflows under {root}/.github/workflows -- cannot judge",
          file=sys.stderr)
    sys.exit(2)

def run_blocks(path):
    """(first file line of the script body, script text) per `run:` scalar."""
    with open(path, encoding="utf-8") as fh:
        node = yaml.compose(fh)
    out = []
    def walk(n):
        if isinstance(n, yaml.MappingNode):
            for k, v in n.value:
                if (isinstance(k, yaml.ScalarNode) and k.value == "run"
                        and isinstance(v, yaml.ScalarNode)):
                    # A block scalar's body starts on the line after the key.
                    first = v.start_mark.line + (2 if v.style in ("|", ">") else 1)
                    out.append((first, v.value))
                else:
                    walk(v)
        elif isinstance(n, yaml.SequenceNode):
            for v in n.value:
                walk(v)
    walk(node)
    return out

acquisitions = []   # (wf, line, statement, block)
for wf in wfs:
    try:
        blocks = run_blocks(wf)
    except yaml.YAMLError as exc:
        print(f"FATAL: {wf} is not readable YAML ({exc}) -- cannot judge",
              file=sys.stderr)
        sys.exit(2)
    for first, text in blocks:
        lines = text.split("\n")
        for i, ln in enumerate(lines):
            if ln.lstrip().startswith("#") or not ACQ.search(ln):
                continue
            stmt = ln
            j = i
            while stmt.rstrip().endswith("\\") and j + 1 < len(lines):
                j += 1
                stmt = stmt.rstrip()[:-1] + " " + lines[j].strip()
            acquisitions.append((os.path.basename(wf), first + i, stmt, text))

if not acquisitions:
    print("FATAL: no git clone or git fetch in any workflow -- a scan that "
          "measured nothing is unmeasured, not clean", file=sys.stderr)
    sys.exit(2)

pin_files = sorted(glob.glob(os.path.join(root, "ci/pins/*.txt")))
if not pin_files:
    print(f"FATAL: {len(acquisitions)} network clone/fetch statement(s) and no "
          f"ci/pins/*.txt to judge them against -- cannot judge", file=sys.stderr)
    sys.exit(2)

rc = 0
pins = {}       # commit -> (name, url)
urls = {}       # url -> name
for pf in pin_files:
    with open(pf, encoding="utf-8") as fh:
        for n, raw in enumerate(fh, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            f = line.split()
            if len(f) != 4 or not re.fullmatch(r"[0-9a-f]{40}", f[2]):
                print(f"FAIL: {os.path.relpath(pf, root)}:{n}: expected "
                      f"'<name> <url> <40-hex commit> <version>', got: {line}")
                rc = 1
                continue
            pins[f[2]] = (f[0], f[1])
            urls[f[1]] = f[0]

def pinned(block):
    commits = set(HEX.findall(block))
    unknown = sorted(c for c in commits if c not in pins)
    sourced = bool(PINREF.search(block)) or any(c in pins for c in commits)
    asserted = "rev-parse HEAD" in block
    return sourced, asserted, unknown

# Where each pinned URL IS applied, so a copy that is not can name the other.
applied = {}
for wf, line, stmt, block in acquisitions:
    sourced, asserted, unknown = pinned(block)
    if sourced and asserted and not unknown:
        for url, name in urls.items():
            if url in block or re.search(r"\b%s\b" % re.escape(name), block):
                applied.setdefault(url, set()).add(wf)

for wf, line, stmt, block in acquisitions:
    sourced, asserted, unknown = pinned(block)
    where = f"{wf}:{line}"
    if unknown:
        print(f"FAIL: {where}: the block checks out {', '.join(unknown)}, "
              f"which no ci/pins/*.txt file names")
        rc = 1
        continue
    if sourced and asserted:
        continue
    if sourced:
        print(f"FAIL: {where}: fetched from a pin but the block never asserts "
              f"`rev-parse HEAD` against it -- a pin that is not checked is "
              f"not applied: {stmt.strip()}")
        rc = 1
        continue
    others = sorted({w for u, ws in applied.items() if u in stmt for w in ws} - {wf})
    if others:
        print(f"FAIL: {where}: fetched unpinned here while "
              f"{', '.join(others)} pins the same source -- the two workflows "
              f"build different bytes: {stmt.strip()}")
    else:
        print(f"FAIL: {where}: network {ACQ.search(stmt).group(1)} with no "
              f"pinned commit from ci/pins/: {stmt.strip()}")
    rc = 1

if rc == 0:
    print(f"check-network-pins: {len(acquisitions)} clone/fetch statement(s) "
          f"in {len(wfs)} workflow(s), all pinned and asserted")
sys.exit(rc)
PYEOF
