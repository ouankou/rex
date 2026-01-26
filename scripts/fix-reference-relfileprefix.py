#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reference-dir", default=None)
    args = ap.parse_args()

    root = (
        Path(args.reference_dir).resolve()
        if args.reference_dir
        else Path(__file__).resolve().parent.parent / "docs" / "reference"
    )
    if not root.is_dir():
        print(f"reference dir not found: {root}", file=sys.stderr)
        return 2

    renamed = 0
    removed = 0
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if path.suffix:
            continue
        try:
            head = path.read_bytes()[:8192]
        except Exception:
            continue
        probe = head.replace(b"\x00", b"")
        if b":mrdocs:" not in probe:
            continue
        new_path = path.with_name(path.name + ".adoc")
        if new_path.exists():
            path.unlink()
            removed += 1
            continue
        path.rename(new_path)
        renamed += 1

    changed = 0
    for path in sorted(root.rglob("*.adoc")):
        rel = path.relative_to(root)
        if rel.as_posix() == "index.adoc":
            continue
        if rel.parts and rel.parts[0] == "sections":
            continue

        lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
        out = []
        updated = False

        for line in lines:
            if "\ufffd" in line:
                line = line.replace("\ufffd", "")
            if "\x00" in line:
                line = line.replace("\x00", "")
            if line.startswith(":relfileprefix:"):
                value = line[len(":relfileprefix:") :].strip()
                if value and not value.endswith("/"):
                    value += "/"
                value = (value + "../") if value else "../"
                out.append(f":relfileprefix: {value}\n")
                updated = True
                continue
            out.append(line)

        if updated:
            path.write_text("".join(out), encoding="utf-8")
            changed += 1

    print(
        f"renamed {renamed} files, removed {removed} duplicates, "
        f"and fixed relfileprefix in {changed} files"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
