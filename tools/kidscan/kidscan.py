#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
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
import stat
import subprocess
import sys
import tempfile
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

# A Steam app id, as provisioning's copy step and picture step also read it
# (ADR-0033): those programs use these rules from here, so the three cannot
# drift apart.
APPID = re.compile(r"[1-9][0-9]{0,9}")

# StateFlags is a bit field, and 4 is FullyInstalled. A game with an update
# waiting has other bits too, still runs, and keeps its tile.
STATE_FLAGS = re.compile(r"[0-9]{1,10}")
FULLY_INSTALLED = 4

# Steam's manifests are a few kilobytes. Root reads the shared library's,
# and every Guardian can write there.
MANIFEST_LIMIT = 1 << 20


# --------------------------------------------------------------------------
# Valve KeyValues (VDF) parsing
# --------------------------------------------------------------------------

# A string runs to its closing quote, and a backslash keeps the character
# after it, whatever it is, in the string. The closing quote is optional
# here so that a string that never closes is matched once, to the end, and
# refused. Were it required, the search would start again from every quote
# inside such a string, and a 1 MiB file would take about an hour.
_TOKEN_RE = re.compile(r'"((?:[^"\\]|\\.)*)(")?|([{}])|//[^\n]*|\s+', re.DOTALL)


def parse_vdf(text):
    """Parse Valve KeyValues text into nested dicts.

    Handles the subset used by appmanifest_*.acf and libraryfolders.vdf:
    quoted key/value pairs, nested braces, // comments. Duplicate keys keep
    the last value, matching Valve's own behaviour. A string that never
    closes is a ValueError, like unbalanced braces.
    """
    tokens = []
    for match in _TOKEN_RE.finditer(text):
        if match.group(1) is not None:
            if match.group(2) is None:
                raise ValueError("unterminated string")
            tokens.append(("str", match.group(1)))
        elif match.group(3) is not None:
            tokens.append(("brace", match.group(3)))

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

def find_steam_roots(extra=None, defaults=True):
    """The Steam roots that exist: `extra`, then the usual places in this
    HOME unless `defaults` is False."""
    roots = []
    candidates = list(extra or []) + (STEAM_ROOTS if defaults else [])
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


def plain_folder_name(name):
    """One folder name inside steamapps/common, never a path out of it."""
    return (
        isinstance(name, str)
        and name not in ("", ".", "..")
        and "/" not in name
        and "\0" not in name
        and not name.startswith(".cairn-")
    )


# A child's Steam account may not have the branch, and its Steam would then
# download the game again.
def on_beta_branch(state):
    for block in ("UserConfig", "MountedConfig"):
        config = ci_get(state, block, {})
        branch = ci_get(config, "BetaKey", "") if isinstance(config, dict) else None
        if branch not in ("", "public"):
            return True
    return False


def fully_installed(state):
    flags = ci_get(state, "StateFlags")
    if not isinstance(flags, str) or not STATE_FLAGS.fullmatch(flags):
        return False
    return (int(flags) & FULLY_INSTALLED) != 0


def is_steam_tool(appid, name):
    """Whether an app is one of Steam's own tools, a runtime, Proton or a
    redistributable, rather than a game. Three programs take the rule from
    here: kidscan leaves these apps out of the tiles, and on the image the
    tools step and the copy step take only these apps (ADR-0046)."""
    return appid in APPID_DENYLIST or bool(PROTON_NAME_RE.match(name))


def app_from_manifest(file_name, data, steamapps):
    """The game one appmanifest describes, or None when it gets no tile. A
    manifest is Steam's file, and anyone who can write the library can
    change it: a value of the wrong kind, such as a block where a string
    belongs, leaves the game out and never stops the scan."""
    state = ci_get(data, "AppState")
    if not isinstance(state, dict):
        return None
    appid = ci_get(state, "appid")
    name = ci_get(state, "name", "")
    installdir = ci_get(state, "installdir")
    if not isinstance(appid, str) or not APPID.fullmatch(appid):
        return None
    if file_name != f"appmanifest_{appid}.acf":
        return None
    if not isinstance(name, str) or not plain_folder_name(installdir):
        return None
    if not fully_installed(state) or on_beta_branch(state):
        return None
    if is_steam_tool(appid, name):
        return None

    path = steamapps / "common" / installdir
    if not path.is_dir():
        return None
    return {"appid": appid, "name": name or installdir, "path": path}


def read_plain_file(path):
    """A file's bytes, exactly as they are, or None when it is not a plain
    file of at most MANIFEST_LIMIT bytes. Steam never links or pipes one of
    its files; one that is a link or a FIFO could make root read another
    file, or make any reader wait for ever."""
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    except OSError:
        return None
    try:
        details = os.fstat(descriptor)
        if not stat.S_ISREG(details.st_mode) or details.st_size > MANIFEST_LIMIT:
            return None
        data = b""
        while len(data) <= MANIFEST_LIMIT:
            chunk = os.read(descriptor, MANIFEST_LIMIT + 1 - len(data))
            if not chunk:
                break
            data += chunk
    except OSError:
        return None
    finally:
        os.close(descriptor)
    if len(data) > MANIFEST_LIMIT:
        return None
    return data


def read_manifest(path):
    """A manifest's text, or None when it is not a plain file of at most
    MANIFEST_LIMIT bytes (read_plain_file)."""
    data = read_plain_file(path)
    return None if data is None else data.decode("utf-8", errors="replace")


def installed_apps(steamapps):
    """Yield installed apps from appmanifest_*.acf files."""
    for manifest in sorted(steamapps.glob("appmanifest_*.acf")):
        text = read_manifest(manifest)
        if text is None:
            print(f"warning: skipping {manifest.name}: not a plain file of at most 1 MiB",
                  file=sys.stderr)
            continue
        try:
            data = parse_vdf(text)
        except ValueError as exc:
            print(f"warning: skipping {manifest.name}: {exc}", file=sys.stderr)
            continue
        app = app_from_manifest(manifest.name, data, steamapps)
        if app is not None:
            yield app


# --------------------------------------------------------------------------
# ScummVM detection
# --------------------------------------------------------------------------

_DETECT_LINE_RE = re.compile(r"^([a-z0-9_.:-]+)\s{2,}(\S.*?)\s*$", re.I)


def scummvm_detect(path, scummvm_bin):
    """Return [(target_id, description)] for SCUMM games under path."""
    # ScummVM opens an SDL window before it looks at any file, even for
    # --detect, and exits 1 with "No available video device" when there is
    # no display. This runs headless during image build and over ssh, so
    # give SDL its dummy drivers. A real display is never needed to detect.
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    try:
        result = subprocess.run(
            [scummvm_bin, "--detect", f"--path={path}"],
            capture_output=True, text=True, timeout=60, env=env,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        print(f"warning: scummvm --detect failed for {path}: {exc}", file=sys.stderr)
        return []
    if result.returncode != 0:
        # Do not fall through to Steam silently: a title routed through the
        # Steam client for no reason is exactly what this tool exists to avoid.
        reason = result.stderr.strip().splitlines()[-1:] or ["no output"]
        print(f"warning: scummvm --detect exited {result.returncode} for {path}: "
              f"{reason[0]}; routing through Steam instead", file=sys.stderr)
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


class LinkedLibrary(Exception):
    """A library named with --library whose steamapps is a link."""


def build_entries(roots, scummvm_bin, category_map, follow_library_folders=True):
    entries = []
    seen_appids = set()

    for root in roots:
        libraries = library_paths(root) if follow_library_folders else [root / "steamapps"]
        for steamapps in libraries:
            # Provisioning names the shared folder, which every Guardian can
            # write, and runs this as root: a link swapped in there would
            # list another account's games. Guardians are administrators,
            # so this catches a mistake; it is not a wall.
            if not follow_library_folders and steamapps.is_symlink():
                raise LinkedLibrary(steamapps)
            for app in installed_apps(steamapps):
                if app["appid"] in seen_appids:
                    continue
                seen_appids.add(app["appid"])

                detected = scummvm_detect(app["path"], scummvm_bin) if scummvm_bin else []
                # A tile kind the launcher knows (ADR-0024); a map can say otherwise.
                category = category_map.get(app["appid"], "games")

                if detected:
                    # One tile per detected game: a Steam "pack" can hold several.
                    for target, description in detected:
                        title = description if len(detected) > 1 else app["name"]
                        # From the source, not the title, so ids are
                        # unique and stay put when a name changes (#24).
                        entries.append({
                            "id": f"steam-{app['appid']}-{slugify(target)}",
                            "title": title,
                            "category": category,
                            "engine": "scummvm",
                            # Only options ScummVM takes on Linux: it rejects
                            # the Windows-only --no-console and exits (#97).
                            "exec": [
                                scummvm_bin, "--fullscreen",
                                f"--path={app['path']}", target,
                            ],
                            "source": {"store": "steam", "appid": app["appid"]},
                            "needs_steam_running": False,
                        })
                else:
                    entries.append({
                        "id": f"steam-{app['appid']}",
                        "title": app["name"],
                        "category": category,
                        "engine": "steam",
                        "exec": ["steam", "-applaunch", app["appid"]],
                        "source": {"store": "steam", "appid": app["appid"]},
                        "needs_steam_running": True,
                    })

    entries.sort(key=lambda item: item["title"].lower())
    return entries


def add_pictures(entries, folder):
    """Names DIR/<appid>.jpg as a game's picture when it is a plain file,
    and never opens it. Provisioning copies the pictures there; the
    launcher draws them."""
    folder = os.path.abspath(Path(folder).expanduser())
    for entry in entries:
        source = entry.get("source", {})
        if source.get("store") != "steam":
            continue
        path = os.path.join(folder, f"{source['appid']}.jpg")
        try:
            plain = stat.S_ISREG(os.lstat(path).st_mode)
        except OSError:
            plain = False
        if plain:
            entry["picture"] = path


def write_atomically(text, out_path):
    """A new file beside the old one, on the disk before it is renamed over
    it, so a launcher starting meanwhile reads the old file or the new one,
    never half of one."""
    out_path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(
        dir=out_path.parent, prefix=f".{out_path.name}.", suffix=".new")
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as file:
            os.fchmod(file.fileno(), 0o644)
            file.write(text)
            file.flush()
            os.fsync(file.fileno())
        os.replace(temporary, out_path)
    except BaseException:
        Path(temporary).unlink(missing_ok=True)
        raise
    folder = os.open(out_path.parent, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(folder)
    finally:
        os.close(folder)


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
    where = parser.add_mutually_exclusive_group()
    where.add_argument("--steam-root", action="append", default=[],
                       help="extra Steam root to scan (repeatable)")
    where.add_argument("--library", action="append", default=[], metavar="DIR",
                       help="scan only this Steam library folder, the one holding "
                            "steamapps/ (repeatable); the usual Steam roots and "
                            "libraryfolders.vdf are not read, and a steamapps that "
                            "is a link stops the scan")
    parser.add_argument("--pictures", default=None, metavar="DIR",
                        help="name DIR/<appid>.jpg as a game's picture when it is a plain file")
    parser.add_argument("--scummvm", default=None,
                        help="path to scummvm binary (default: search PATH)")
    parser.add_argument("--no-scummvm", action="store_true",
                        help="skip ScummVM detection; route everything through Steam")
    parser.add_argument("--desktop-dir", default=None,
                        help="also write .desktop files into this directory")
    parser.add_argument("--categories", default=None,
                        help='JSON file mapping appid to category, e.g. {"294660": "games"}')
    args = parser.parse_args(argv)

    if args.library:
        roots = find_steam_roots(args.library, defaults=False)
    else:
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

    try:
        entries = build_entries(roots, scummvm_bin, load_category_map(args.categories),
                                follow_library_folders=not args.library)
    except LinkedLibrary as linked:
        print(f"warning: {linked} is a link, so it is not read; no games are listed",
              file=sys.stderr)
        return 2
    if args.pictures:
        add_pictures(entries, args.pictures)

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
        write_atomically(text, Path(args.output).expanduser())

    if args.desktop_dir:
        written = write_desktop_files(entries, Path(args.desktop_dir).expanduser())
        print(f"wrote {len(written)} desktop files", file=sys.stderr)

    native = sum(1 for e in entries if e["engine"] == "scummvm")
    print(f"{len(entries)} tiles ({native} native ScummVM, "
          f"{len(entries) - native} via Steam)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
