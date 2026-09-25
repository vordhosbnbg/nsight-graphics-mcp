#!/usr/bin/env python3
"""Prepare a source-complete, versioned local makepkg recipe without downloads."""

import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile


def git(root, *arguments):
    return subprocess.check_output(["git", "-C", str(root), *arguments])


def prepare(source, output, version, makepkg):
    declared = re.search(r"project\(NsightGraphicsMCP VERSION (\d+\.\d+\.\d+)",
                         (source / "CMakeLists.txt").read_text()).group(1)
    if version != declared:
        raise ValueError("CMake project version changed; reconfigure before packaging")
    revision = git(source, "rev-parse", "HEAD").decode().strip()
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH") or
                git(source, "show", "-s", "--format=%ct", "HEAD").decode().strip())
    roots = {"cmake", "docs", "include", "packaging", "shaders", "src", "tests"}
    top_files = {".clang-format", ".gitignore", ".gitmodules", "AGENTS.md",
                 "CMakeLists.txt", "CMakePresets.json", "LICENSE", "README.md"}
    files = set()
    # Include current first-party edits and new, non-ignored source files. Never
    # sweep the working directory: captures, builds and local configuration stay out.
    for entry in git(source, "ls-files", "--cached", "--others", "--exclude-standard", "-z").split(b"\0"):
        if not entry:
            continue
        path = Path(os.fsdecode(entry))
        if str(path) in top_files or path.parts[0] in roots:
            files.add(path)
    dependencies = {}
    for entry in git(source, "ls-files", "--stage", "-z", "--", "external").split(b"\0"):
        if not entry:
            continue
        metadata, name = os.fsdecode(entry).split("\t", 1)
        mode, commit, stage = metadata.split()
        if mode != "160000" or stage != "0":
            raise ValueError(f"Expected an exact submodule gitlink: {name}")
        path = Path(name)
        actual = git(source / path, "rev-parse", "HEAD").decode().strip()
        dirty = git(source / path, "status", "--porcelain", "--untracked-files=all")
        if actual != commit or dirty:
            raise ValueError(f"Dependency {name} must be initialized, clean and at {commit}")
        dependencies[name] = commit
        for child in git(source / path, "ls-files", "-z").split(b"\0"):
            if child:
                files.add(path / os.fsdecode(child))
    if not dependencies:
        raise ValueError("No dependency gitlinks found; use an initialized source checkout")
    provenance = json.dumps({
        "project_version": version, "source_revision": revision,
        "source_dirty": bool(git(source, "status", "--porcelain", "--untracked-files=all")),
        "source_date_epoch": epoch, "dependencies": dependencies,
        "scope": "Current first-party files and clean dependency revisions; no Git metadata",
    }, indent=2, sort_keys=True).encode() + b"\n"
    output.mkdir(parents=True, exist_ok=True)
    package = f"nsight-graphics-mcp-{version}"
    archive = output / f"{package}.tar.gz"
    with tempfile.NamedTemporaryFile(dir=output) as temporary:
        with gzip.GzipFile(fileobj=temporary, filename="", mode="wb", mtime=epoch) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as bundle:
                for path in sorted(files):
                    original = source / path
                    if original.is_symlink() or not original.is_file():
                        raise ValueError(f"Expected a regular source file: {path}")
                    info = bundle.gettarinfo(str(original), arcname=f"{package}/{path.as_posix()}")
                    info.uid = info.gid = 0
                    info.uname = info.gname = "root"
                    info.mtime = epoch
                    info.mode = 0o755 if original.stat().st_mode & 0o111 else 0o644
                    with original.open("rb") as contents:
                        bundle.addfile(info, contents)
                info = tarfile.TarInfo(f"{package}/SOURCE_PROVENANCE.json")
                info.size, info.mtime, info.mode = len(provenance), epoch, 0o644
                info.uname = info.gname = "root"
                bundle.addfile(info, io.BytesIO(provenance))
        temporary.flush()
        with open(temporary.name, "rb") as contents:
            digest = hashlib.file_digest(contents, "sha256").hexdigest()
        # Copy the completed archive atomically; the temporary context owns cleanup.
        destination = output / f".{package}.tar.gz.new"
        with open(temporary.name, "rb") as contents, destination.open("wb") as target:
            while chunk := contents.read(1024 * 1024):
                target.write(chunk)
        destination.replace(archive)
    template = (source / "packaging/arch/PKGBUILD.in").read_text()
    recipe = template.replace("@VERSION@", version).replace("@SOURCE_SHA256@", digest)
    (output / "PKGBUILD").write_text(recipe)
    environment = dict(os.environ)
    for name in ("PKGDEST", "SRCDEST", "SRCPKGDEST", "BUILDDIR", "LOGDEST"):
        environment[name] = str(output)
    metadata = subprocess.check_output([makepkg, "--printsrcinfo"], cwd=output, env=environment)
    (output / ".SRCINFO").write_bytes(metadata)
    (output / "SHA256SUMS").write_text(f"{digest}  {archive.name}\n")
    print(f"Prepared {archive}, PKGBUILD and .SRCINFO", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--makepkg", default="makepkg")
    args = parser.parse_args()
    prepare(args.source.resolve(), args.output.resolve(), args.version, args.makepkg)
