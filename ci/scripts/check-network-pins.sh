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
# it every `git clone` and `git fetch` -- each one judged on its own, never by
# what its neighbours in the same block do. A block that pins one source and
# clones a second one unpinned is a block with one unpinned clone. A statement
# passes only if all of these hold for IT:
#   * the commit it fetches or checks out is a pin -- a forty-hex literal that a
#     ci/pins/*.txt row carries, or the commit field of a `read ... < <(grep
#     '^<name> ' ci/pins/<file>.txt)` in the same scope (the function, or the
#     block outside any function);
#   * the URL it fetches from is that same row's URL -- a literal equal to it,
#     or the URL field of the same read;
#   * the scope compares the fetched HEAD against that commit and exits on a
#     mismatch (`[ "$got" = "$sha" ] || ... exit`), with `got` taken from
#     `rev-parse HEAD`. The word rev-parse on its own asserts nothing.
# A pin read by a function (`grep "^$1 "`) is resolved at every call site: each
# name the block passes must be a row of the file, or the fetch fails at build
# time and this says so on push instead. A pin row whose commit is not forty
# hex digits is a failure: a tag in that column is the thing a pin exists to
# prevent.
#
# Not measurable here: whether a forty-hex pin is a commit or an annotated tag
# object. The asserted HEAD comparison catches that at build time.
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
HEX = re.compile(r"^[0-9a-f]{40}$")
GIT = re.compile(r"\bgit\b((?:\s+-C\s+\S+)*)\s+(clone|fetch|remote\s+add|checkout)\b(.*)")
VAR = re.compile(r'^"?\$\{?(\w+)\}?"?$')
READ = re.compile(r"\bread\s+(?:-r\s+)?(\w+)\s+(\w+)\s+(\w+)\s+(\w+)\s*<\s*<\(\s*grep\s+"
                  r"(['\"])\^(\S+?)\s\5\s+(ci/pins/[\w.-]+\.txt)\s*\)")
GOT = re.compile(r'\b(\w+)="?\$\(\s*git\b[^)]*\brev-parse\s+HEAD\s*\)"?')
FUNC = re.compile(r"^(\s*)(\w+)\s*\(\)\s*\{")
ASSERT = re.compile(r'\[\[?\s*"([^"]+)"\s*==?\s*"([^"]+)"\s*\]\]?\s*\|\|(.*)')
OPT_WITH_VALUE = {"--depth", "--branch", "-b", "--origin", "-o", "--filter", "--reference"}

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


def statements(text, first):
    """Logical statements (continuations joined): (file line, text), comments dropped."""
    out, lines, i = [], text.split("\n"), 0
    while i < len(lines):
        ln, start = lines[i], i
        while ln.rstrip().endswith("\\") and i + 1 < len(lines):
            i += 1
            ln = ln.rstrip()[:-1] + " " + lines[i].strip()
        if not ln.lstrip().startswith("#"):
            out.append((first + start, ln))
        i += 1
    return out


def function_of(stmts):
    """index -> name of the function whose body holds it ('' outside any)."""
    fn_of, cur, indent = {}, "", None
    for idx, (_, ln) in enumerate(stmts):
        if not cur:
            m = FUNC.match(ln)
            if m:
                cur, indent = m.group(2), m.group(1)
            fn_of[idx] = cur
            continue
        fn_of[idx] = cur
        if re.match(r"^" + re.escape(indent) + r"\}\s*$", ln):
            cur = ""
    return fn_of


def args(rest):
    """Positional words of a git subcommand, options and their values dropped."""
    out, skip = [], False
    for x in re.split(r"\s+", rest.strip()):
        if not x:
            continue
        if x in ("||", "&&", ";", "|"):
            break
        if skip:
            skip = False
            continue
        if x in OPT_WITH_VALUE:
            skip = True
            continue
        if x.startswith("-"):
            continue
        out.append(x)
    return out


pin_files = sorted(glob.glob(os.path.join(root, "ci/pins/*.txt")))
rc = 0
rows = {}          # relpath -> {name: (url, commit)}
by_commit = {}     # commit -> (name, url)
for pf in pin_files:
    rel = os.path.relpath(pf, root)
    rows[rel] = {}
    with open(pf, encoding="utf-8") as fh:
        for n, raw in enumerate(fh, 1):
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            f = line.split()
            if len(f) != 4 or not HEX.match(f[2]):
                print(f"FAIL: {rel}:{n}: expected "
                      f"'<name> <url> <40-hex commit> <version>', got: {line}")
                rc = 1
                continue
            rows[rel][f[0]] = (f[1], f[2])
            by_commit[f[2]] = (f[0], f[1])

acquisitions = 0
failures = []     # (workflow, line, literal url or None, message)
applied = {}      # url -> workflows in which a statement for it passed

for wf in wfs:
    wname = os.path.basename(wf)
    try:
        blocks = run_blocks(wf)
    except yaml.YAMLError as exc:
        print(f"FATAL: {wf} is not readable YAML ({exc}) -- cannot judge", file=sys.stderr)
        sys.exit(2)
    for first, text in blocks:
        stmts = statements(text, first)
        fn_of = function_of(stmts)
        reads, gots, asserted, remotes, checkouts = {}, {}, {}, {}, {}
        for idx, (line, ln) in enumerate(stmts):
            sc = fn_of[idx]
            for m in READ.finditer(ln):
                _n, uvar, svar, _v, _q, name, pfile = m.groups()
                reads.setdefault(sc, []).append(
                    {"url": uvar, "sha": svar, "name": name, "file": pfile, "line": line})
            for m in GOT.finditer(ln):
                gots.setdefault(sc, set()).add(m.group(1))
            g = GIT.search(ln)
            if g and g.group(2).startswith("remote"):
                a = args(g.group(3))
                if len(a) >= 2:
                    remotes.setdefault(sc, []).append(a[1])
            if g and g.group(2) == "checkout":
                a = args(g.group(3))
                if a:
                    checkouts.setdefault(sc, []).append(a[0])
        for idx, (line, ln) in enumerate(stmts):
            sc = fn_of[idx]
            for m in ASSERT.finditer(ln):
                a, b, tail = m.groups()
                if "exit" not in tail:
                    continue
                for lhs, rhs in ((a, b), (b, a)):
                    lv = VAR.match('"' + lhs + '"')
                    if "rev-parse HEAD" in lhs or (lv and lv.group(1) in gots.get(sc, set())):
                        rv = VAR.match('"' + rhs + '"')
                        asserted.setdefault(sc, set()).add(rv.group(1) if rv else rhs)

        def resolve(sc, ref):
            """-> (url it must come from, commit key, read or None), or an error."""
            v = VAR.match(ref)
            if v:
                for r in reads.get(sc, []):
                    if r["sha"] == v.group(1):
                        return ("$" + r["url"], r["sha"], r)
                return f"fetches ${v.group(1)}, which no pin read in this scope provides"
            c = ref.strip('"')
            if HEX.match(c):
                if c not in by_commit:
                    return f"checks out {c}, which no ci/pins/*.txt file names"
                return (by_commit[c][1], c, None)
            return f"fetches '{ref}', which is not a pinned commit"

        for idx, (line, ln) in enumerate(stmts):
            g = GIT.search(ln)
            if not g or g.group(2) not in ("clone", "fetch"):
                continue
            acquisitions += 1
            sc = fn_of[idx]
            a = args(g.group(3))
            url_lit = next((x.strip('"') for x in a if re.match(r'"?https?://', x)), None)
            if g.group(2) == "clone":
                url = a[0] if a else ""
                ref = next((c for c in checkouts.get(sc, [])
                            if HEX.match(c.strip('"')) or VAR.match(c)), None)
                if ref is None:
                    failures.append((wname, line, url_lit,
                                     f"network clone with no pinned commit checked out: {ln.strip()}"))
                    continue
            else:
                if len(a) < 2:
                    failures.append((wname, line, url_lit,
                                     f"fetch names no explicit commit: {ln.strip()}"))
                    continue
                url, ref = a[0], a[-1]
                if not (url.strip('"').startswith("http") or VAR.match(url)):
                    rs = remotes.get(sc, [])
                    url = rs[-1] if rs else url
            res = resolve(sc, ref)
            if isinstance(res, str):
                failures.append((wname, line, url_lit, f"{res}: {ln.strip()}"))
                continue
            want_url, key, read = res
            u = VAR.match(url)
            got_url = ("$" + u.group(1)) if u else url.strip('"')
            if got_url != want_url:
                failures.append((wname, line, url_lit,
                                 f"fetches from {got_url}, but the pinned commit belongs to "
                                 f"{want_url}: {ln.strip()}"))
                continue
            if key not in asserted.get(sc, set()):
                failures.append((wname, line, url_lit,
                                 f"the fetched HEAD is never asserted against the pin -- no "
                                 f"`[ \"$got\" = \"${key}\" ] || ... exit` in this scope: {ln.strip()}"))
                continue
            if read is None:
                applied.setdefault(want_url, set()).add(wname)
                continue
            pfile = read["file"]
            if pfile not in rows:
                failures.append((wname, line, url_lit, f"reads {pfile}, which does not exist"))
                continue
            if read["name"] == "$1":
                fn = sc
                if not fn:
                    failures.append((wname, line, url_lit,
                                     "reads the pin name from $1 outside a function"))
                    continue
                names = []
                for j, (l2, s2) in enumerate(stmts):
                    if fn_of[j] == fn:
                        continue
                    for m in re.finditer(r"(?:^|[;&|(]\s*|\s)" + re.escape(fn) + r"\s+(\S+)", s2):
                        names.append((l2, m.group(1).strip("'\"")))
                if not names:
                    failures.append((wname, line, url_lit,
                                     f"{fn} is never called -- no pin name to resolve"))
                    continue
            else:
                names = [(read["line"], read["name"])]
            for l2, nm in names:
                if nm not in rows[pfile]:
                    failures.append((wname, l2, None,
                                     f"fetches '{nm}', which {pfile} has no row for"))
                else:
                    applied.setdefault(rows[pfile][nm][0], set()).add(wname)

if acquisitions == 0:
    print("FATAL: no git clone or git fetch in any workflow -- a scan that "
          "measured nothing is unmeasured, not clean", file=sys.stderr)
    sys.exit(2)
if not pin_files:
    print(f"FATAL: {acquisitions} network clone/fetch statement(s) and no "
          f"ci/pins/*.txt to judge them against -- cannot judge", file=sys.stderr)
    sys.exit(2)

for wname, line, url_lit, msg in failures:
    others = sorted(applied.get(url_lit, set()) - {wname}) if url_lit else []
    if others:
        print(f"FAIL: {wname}:{line}: fetched unpinned here while {', '.join(others)} "
              f"pins the same source -- the two workflows build different bytes: {msg}")
    else:
        print(f"FAIL: {wname}:{line}: {msg}")
    rc = 1

if rc == 0:
    print(f"check-network-pins: {acquisitions} clone/fetch statement(s) "
          f"in {len(wfs)} workflow(s), each pinned and asserted")
sys.exit(rc)
PYEOF
