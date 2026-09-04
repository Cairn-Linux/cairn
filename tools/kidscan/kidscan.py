#!/usr/bin/env python3
"""kidscan - discover installed games and emit a launcher manifest.

Scans Steam libraries, works out which titles can run under native ScummVM
(no Proton, no Steam client UI) and which need Steam itself, then writes a
launcher-agnostic JSON manifest. Optionally emits .desktop files.

Standard library only, on purpose: this runs during image build and on
parent machines where nothing is guaranteed to be installed.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

MANIFEST_VERSION = 1

STEAM_ROOTS = [
    "~/.steam/steam",
    "~/.steam/root",
    "~/.local/share/Steam",
    "~/.var/app/com.valvesoftware.Steam/data/Steam",
]

# Steam ships these as tools/runtimes, not games. Never surface them.
APPID_DENYLIST = {
    "228980",   # Steamworks Common Redistributables
    "1070560",  # Steam Linux Runtime 1.0
    "1391110",  # Steam Linux Runtime 2.0 (soldier)
    "1628350",  # Steam Linux Runtime 3.0 (sniper)
    "1493710",  # Proton Experimental
}
PROTON_NAME_RE = re.compile(r"^(proton|steam linux runtime|steamworks)", re.I)


# --------------------------------------------------------------------------
# Valve KeyValues (VDF) parsing
# --------------------------------------------------------------------------

_TOKEN_RE = re.compile(r'"((?:[^"\\]|\\.)*)"|([{}])|//[^\n]*|\s+')


def parse_vdf(text):
    """Parse Valve KeyValues text into nested dicts.

    Handles the subset used by appmanifest_*.acf and libraryfolders.vdf:
    quoted key/value pairs, nested braces, // comments. Duplicate keys keep
    the last value, matching Valve's own behaviour.
    """
    tokens = []
    for match in _TOKEN_RE.finditer(text):
        if match.group(1) is not None:
            tokens.append(("str", match.group(1)))
        elif match.group(2) is not None:
            tokens.append(("brace", match.group(2)))

    root = {}
    stack = [root]
    pending_key = None

    for kind, value in tokens:
        if kind == "brace":
            if value == "{":
                if pending_key is None:
                    raise ValueError("unexpected '{' with no key")
                child = {}
                stack[-1][pending_key] = child
                stack.append(child)
                pending_key = None
            else:
                if len(stack) == 1:
                    raise ValueError("unbalanced '}'")
                stack.pop()
        else:
            if pending_key is None:
                pending_key = value
            else:
                stack[-1][pending_key] = value
                pending_key = None

    if len(stack) != 1:
        raise ValueError("unbalanced braces")
    return root


def ci_get(mapping, key, default=None):
    """Case-insensitive lookup. Valve is inconsistent about capitalisation."""
    lowered = key.lower()
    for existing, value in mapping.items():
        if existing.lower() == lowered:
            return value
    return default


# --------------------------------------------------------------------------
# Steam discovery
# --------------------------------------------------------------------------

def find_steam_roots(extra=None):
    roots = []
    candidates = list(extra or []) + STEAM_ROOTS
    for candidate in candidates:
        path = Path(candidate).expanduser()
        try:
            resolved = path.resolve()
        except OSError:
            continue
        if (resolved / "steamapps").is_dir() and resolved not in roots:
            roots.append(resolved)
    return roots


def library_paths(steam_root):
    """All library folders for a Steam install, including the root itself."""
    paths = [steam_root / "steamapps"]
    vdf_path = steam_root / "steamapps" / "libraryfolders.vdf"
    if not vdf_path.is_file():
        return paths

    try:
        data = parse_vdf(vdf_path.read_text(encoding="utf-8", errors="replace"))
    except (ValueError, OSError) as exc:
        print(f"warning: could not parse {vdf_path}: {exc}", file=sys.stderr)
        return paths

    folders = ci_get(data, "libraryfolders", {})
    for entry in folders.values():
        # Modern format nests a dict with "path"; ancient format is a bare string.
        raw = ci_get(entry, "path") if isinstance(entry, dict) else entry
        if not raw:
            continue
        steamapps = Path(raw).expanduser() / "steamapps"
        if steamapps.is_dir() and steamapps not in paths:
            paths.append(steamapps)
    return paths


def installed_apps(steamapps):
    """Yield installed apps from appmanifest_*.acf files."""
    for manifest in sorted(steamapps.glob("appmanifest_*.acf")):
        try:
            data = parse_vdf(manifest.read_text(encoding="utf-8", errors="replace"))
        except (ValueError, OSError) as exc:
            print(f"warning: skipping {manifest.name}: {exc}", file=sys.stderr)
            continue

        state = ci_get(data, "AppState", {})
        appid = ci_get(state, "appid")
        name = ci_get(state, "name", "")
        installdir = ci_get(state, "installdir", "")
        if not appid or not installdir:
            continue
        if appid in APPID_DENYLIST or PROTON_NAME_RE.match(name):
            continue

        path = steamapps / "common" / installdir
        if not path.is_dir():
            continue
        yield {"appid": appid, "name": name or installdir, "path": path}


# --------------------------------------------------------------------------
# ScummVM detection
# --------------------------------------------------------------------------

_DETECT_LINE_RE = re.compile(r"^([a-z0-9_.:-]+)\s{2,}(\S.*?)\s*$", re.I)


def scummvm_detect(path, scummvm_bin):
    """Return [(target_id, description)] for SCUMM games under path."""
    try:
        result = subprocess.run(
            [scummvm_bin, "--detect", f"--path={path}"],
            capture_output=True, text=True, timeout=60,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        print(f"warning: scummvm --detect failed for {path}: {exc}", file=sys.stderr)
        return []

    found = []
    for line in result.stdout.splitlines():
        stripped = line.strip()
        if not stripped or set(stripped) <= set("- "):
            continue
        if stripped.lower().startswith(("gameid", "id ", "id\t")):
            continue
        match = _DETECT_LINE_RE.match(line.rstrip())
        if match:
            # Keep only the description, not the trailing "Full Path" column.
            # Columns are separated by runs of spaces, but a long description
            # runs into the path with a single space, so also cut at the path
            # we asked about (the Full Path column always starts with it).
            description = re.split(r"\s{2,}", match.group(2))[0]
            cut = description.find(f" {path}")
            if cut > 0:
                description = description[:cut]
            found.append((match.group(1), description.strip()))
    return found


# --------------------------------------------------------------------------
# Manifest
# --------------------------------------------------------------------------

def slugify(text):
    slug = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return slug or "game"


def build_entries(roots, scummvm_bin, category_map):
    entries = []
    seen_appids = set()

    for root in roots:
        for steamapps in library_paths(root):
            for app in installed_apps(steamapps):
                if app["appid"] in seen_appids:
                    continue
                seen_appids.add(app["appid"])

                detected = scummvm_detect(app["path"], scummvm_bin) if scummvm_bin else []
                category = category_map.get(app["appid"], "play")

                if detected:
                    # One tile per detected game: a Steam "pack" can hold several.
                    for target, description in detected:
                        title = description if len(detected) > 1 else app["name"]
                        entries.append({
                            "id": slugify(f"{app['name']}-{target}"),
                            "title": title,
                            "category": category,
                            "engine": "scummvm",
                            "exec": [
                                scummvm_bin, "--fullscreen", "--no-console",
                                f"--path={app['path']}", target,
                            ],
                            "source": {"store": "steam", "appid": app["appid"]},
                            "needs_steam_running": False,
                        })
                else:
                    entries.append({
                        "id": slugify(app["name"]),
                        "title": app["name"],
                        "category": category,
                        "engine": "steam",
                        "exec": ["steam", "-applaunch", app["appid"]],
                        "source": {"store": "steam", "appid": app["appid"]},
                        "needs_steam_running": True,
                    })

    entries.sort(key=lambda item: item["title"].lower())
    return entries


def write_desktop_files(entries, outdir):
    outdir.mkdir(parents=True, exist_ok=True)
    written = []
    for entry in entries:
        # Quote nothing fancy; Exec fields with spaces in paths need quoting.
        exec_line = " ".join(
            f'"{part}"' if " " in part else part for part in entry["exec"]
        )
        target = outdir / f"kid-{entry['id']}.desktop"
        target.write_text(
            "[Desktop Entry]\n"
            "Type=Application\n"
            f"Name={entry['title']}\n"
            f"Exec={exec_line}\n"
            f"Categories=Game;\n"
            f"X-Kid-Category={entry['category']}\n"
            f"X-Kid-Engine={entry['engine']}\n"
            "Terminal=false\n",
            encoding="utf-8",
        )
        target.chmod(0o644)
        written.append(target)
    return written


def load_category_map(path):
    if not path:
        return {}
    try:
        data = json.loads(Path(path).expanduser().read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        print(f"warning: could not read category map: {exc}", file=sys.stderr)
        return {}
    return {str(k): str(v) for k, v in data.items()}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("-o", "--output", default="-",
                        help="manifest path, or - for stdout (default: -)")
    parser.add_argument("--steam-root", action="append", default=[],
                        help="extra Steam root to scan (repeatable)")
    parser.add_argument("--scummvm", default=None,
                        help="path to scummvm binary (default: search PATH)")
    parser.add_argument("--no-scummvm", action="store_true",
                        help="skip ScummVM detection; route everything through Steam")
    parser.add_argument("--desktop-dir", default=None,
                        help="also write .desktop files into this directory")
    parser.add_argument("--categories", default=None,
                        help='JSON file mapping appid to category, e.g. {"294660": "play"}')
    args = parser.parse_args(argv)

    roots = find_steam_roots(args.steam_root)
    if not roots:
        print("no Steam installation found", file=sys.stderr)
        return 2

    scummvm_bin = None
    if not args.no_scummvm:
        scummvm_bin = args.scummvm or shutil.which("scummvm")
        if not scummvm_bin:
            print("warning: scummvm not found; all titles will route through Steam",
                  file=sys.stderr)

    entries = build_entries(roots, scummvm_bin, load_category_map(args.categories))

    manifest = {
        "version": MANIFEST_VERSION,
        "steam_roots": [str(root) for root in roots],
        "scummvm": scummvm_bin,
        "entries": entries,
    }
    text = json.dumps(manifest, indent=2) + "\n"

    if args.output == "-":
        sys.stdout.write(text)
    else:
        out_path = Path(args.output).expanduser()
        if out_path.parent and not out_path.parent.exists():
            out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(text, encoding="utf-8")

    if args.desktop_dir:
        written = write_desktop_files(entries, Path(args.desktop_dir).expanduser())
        print(f"wrote {len(written)} desktop files", file=sys.stderr)

    native = sum(1 for e in entries if e["engine"] == "scummvm")
    print(f"{len(entries)} tiles ({native} native ScummVM, "
          f"{len(entries) - native} via Steam)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
