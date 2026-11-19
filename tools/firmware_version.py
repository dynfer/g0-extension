#!/usr/bin/env python3
"""Fingerprint the G0 build inputs without changing the header on identical builds."""
import argparse
import hashlib
from pathlib import Path
import subprocess


def fingerprint(root, compiler, options):
    digest = hashlib.sha256()
    paths = sorted({*root.glob("*.cpp"), *root.glob("*.h"), root / "Makefile",
                    *root.glob("cfg/*.h"), *root.glob("board/*"),
                    root / "for_fome/g0_spi_protocol.h", root / "tools/firmware_version.py"})
    for path in paths:
        if path.is_file():
            digest.update(path.relative_to(root).as_posix().encode() + b"\0")
            digest.update(path.read_bytes() + b"\0")
    revision = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD:ChibiOS"])
    chibios = ["git", "-C", str(root / "ChibiOS")]
    if (subprocess.check_output([*chibios, "rev-parse", "HEAD"]) != revision or
            subprocess.check_output([*chibios, "status", "--porcelain", "--untracked-files=all"])):
        raise ValueError("ChibiOS must be clean and match its pinned revision; "
                         "commit driver changes and update the submodule pin before building")
    compiler_version = subprocess.check_output([compiler, "--version"])
    digest.update(revision + compiler_version + options.encode())
    return int.from_bytes(digest.digest()[:4], "little") or 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--options", default="")
    args = parser.parse_args()
    try:
        version = fingerprint(args.root.resolve(), args.compiler, args.options)
    except ValueError as error:
        parser.error(str(error))
    content = f"#pragma once\n#define G0_FIRMWARE_VERSION 0x{version:08x}U\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != content:
        temporary = args.output.with_suffix(".tmp")
        temporary.write_text(content)
        temporary.replace(args.output)


if __name__ == "__main__":
    main()
