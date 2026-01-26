#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

import yaml


def _resolve_hooks(hooks: list, root: Path) -> list:
    updated: list = []
    for hook in hooks:
        if isinstance(hook, str):
            hook_path = Path(hook)
            if not hook_path.is_absolute():
                hook_path = root / hook_path
            updated.append(str(hook_path))
        else:
            updated.append(hook)
    return updated


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--docs-dir", required=True)
    ap.add_argument("--site-dir", required=True)
    ap.add_argument("--root", required=True)
    args = ap.parse_args()

    src = Path(args.input).resolve()
    dst = Path(args.output).resolve()
    docs_dir = Path(args.docs_dir).resolve()
    site_dir = Path(args.site_dir).resolve()
    root = Path(args.root).resolve()

    data = yaml.safe_load(src.read_text(encoding="utf-8")) or {}
    if not isinstance(data, dict):
        raise SystemExit(f"mkdocs config root is not a mapping: {src}")

    data["docs_dir"] = str(docs_dir)
    data["site_dir"] = str(site_dir)

    hooks = data.get("hooks")
    if isinstance(hooks, list):
        data["hooks"] = _resolve_hooks(hooks, root)

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(yaml.safe_dump(data, sort_keys=False), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
