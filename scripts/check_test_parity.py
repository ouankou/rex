#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import re
import json
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    yaml = None


TEST_FILE_EXTS = {
    ".c",
    ".C",
    ".cc",
    ".cpp",
    ".cxx",
    ".cu",
    ".f",
    ".F",
    ".f90",
    ".F90",
    ".f95",
    ".F95",
    ".f03",
    ".F03",
    ".f08",
    ".F08",
}

LABELS_TO_COMPARE = {
    "disabled",
    "xfail",
    "known_fail",
    "known-fail",
    "expected_fail",
    "expected-fail",
}

REQUIRED_ROSE_BRANCH = "develop"

def _load_json(path: Path) -> list[dict]:
    return json.loads(path.read_text())


def _load_whitelist(path: Path) -> dict:
    if not path.exists():
        return {}
    text = path.read_text()
    if yaml:
        return yaml.safe_load(text) or {}
    return json.loads(text)


def _assert_rose_branch(manifest_list: list[dict]) -> None:
    rose_origins = []
    for entry in manifest_list:
        for origin in entry.get("origin", []) or []:
            repo = origin.get("repo")
            if repo in {"rose", "rose-archive"}:
                rose_origins.append(origin)
    if not rose_origins:
        raise RuntimeError(
            "manifest missing rose-archive origins; regenerate using rose-archive "
            f"branch '{REQUIRED_ROSE_BRANCH}'"
        )
    missing = [
        origin
        for origin in rose_origins
        if f"branch={REQUIRED_ROSE_BRANCH}" not in (origin.get("notes") or "")
    ]
    if missing:
        raise RuntimeError(
            "manifest rose-archive origins missing branch annotation; "
            f"regenerate using rose-archive branch '{REQUIRED_ROSE_BRANCH}'"
        )


def _normalize_command(command: list[str], repo_root: Path, build_dir: Path) -> list[str]:
    normalized = []
    repo_root = repo_root.resolve()
    build_dir = build_dir.resolve()
    repo_root_str = str(repo_root)
    build_dir_str = str(build_dir)
    build_dir_pattern = re.compile(re.escape(build_dir_str) + r"/[^\s\"']+")
    idx = 0
    while idx < len(command):
        token = command[idx]
        if token.startswith("-DTEST_STRING_MACRO"):
            idx += 1
            while idx < len(command):
                peek = command[idx]
                if (
                    peek.startswith("-")
                    or "/" in peek
                    or Path(peek).suffix in TEST_FILE_EXTS
                ):
                    break
                idx += 1
            continue
        token = token.strip().replace("\"", "")
        token = token.replace("\\$", "$").replace("\\\"", "\"")
        token = re.sub(r"\\+\s", " ", token)
        if "$<TARGET_FILE:" in token:
            token = re.sub(r"\$<TARGET_FILE:([^>]+)>", r"\1", token)
        if build_dir_pattern.search(token):
            token = build_dir_pattern.sub(lambda m: Path(m.group(0)).name, token)
        if repo_root_str in token:
            token = token.replace(repo_root_str + "/", "")
            token = token.replace(repo_root_str, "")
        if " " not in token and "\t" not in token and "/" in token and not token.startswith("-"):
            token = os.path.normpath(token)
        path = Path(token)
        if idx == 0:
            normalized_token = None
            if path.is_absolute():
                try:
                    if path.is_relative_to(build_dir):
                        normalized_token = path.name
                except AttributeError:
                    try:
                        path.relative_to(build_dir)
                        normalized_token = path.name
                    except ValueError:
                        pass
                if normalized_token is None:
                    try:
                        if path.is_relative_to(repo_root):
                            rel = path.relative_to(repo_root)
                            if rel.parts and rel.parts[0] == build_dir.name:
                                normalized_token = path.name
                            else:
                                normalized_token = str(rel)
                    except AttributeError:
                        try:
                            rel = path.relative_to(repo_root)
                            if rel.parts and rel.parts[0] == build_dir.name:
                                normalized_token = path.name
                            else:
                                normalized_token = str(rel)
                        except ValueError:
                            pass
                if normalized_token is None:
                    normalized_token = path.name
            else:
                if "/" in token and not token.startswith("-"):
                    resolved = (repo_root / path).resolve()
                    try:
                        resolved.relative_to(build_dir)
                        normalized_token = path.name
                    except ValueError:
                        try:
                            rel = resolved.relative_to(repo_root)
                            normalized_token = str(rel)
                        except ValueError:
                            normalized_token = token
                else:
                    normalized_token = token
            normalized.append(normalized_token)
            idx += 1
            continue
        if " " not in token and "\t" not in token and not token.startswith("-") and "/" in token:
            rel_path = Path(token)
            if not rel_path.is_absolute():
                resolved = (repo_root / rel_path).resolve()
                try:
                    token = str(resolved.relative_to(repo_root))
                except ValueError:
                    token = str(rel_path)
                path = Path(token)
        if path.is_absolute():
            try:
                if path.is_relative_to(repo_root):
                    rel = path.relative_to(repo_root)
                    normalized.append(str(rel))
                    continue
            except AttributeError:
                try:
                    rel = path.relative_to(repo_root)
                    normalized.append(str(rel))
                    continue
                except ValueError:
                    pass
            normalized.append(str(path))
        elif path.suffix in TEST_FILE_EXTS:
            normalized.append(str(path))
        else:
            normalized.append(token)
        idx += 1
    return normalized


def _apply_renames(manifest: dict[str, dict], renames: dict[str, str]) -> dict[str, dict]:
    updated = {}
    for name, entry in manifest.items():
        new_name = renames.get(name, name)
        if new_name in updated:
            raise RuntimeError(f"rename collision for {new_name}")
        entry = dict(entry)
        entry["name"] = new_name
        if entry.get("depends"):
            entry["depends"] = [renames.get(dep, dep) for dep in entry["depends"]]
        updated[new_name] = entry
    return updated


def main() -> int:
    parser = argparse.ArgumentParser(description="Check manifest vs CTest inventory.")
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--whitelist", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()

    manifest_list = _load_json(args.manifest)
    inventory_list = _load_json(args.inventory)
    whitelist = _load_whitelist(args.whitelist)
    _assert_rose_branch(manifest_list)

    renames = whitelist.get("renames", {}) or {}
    exclusions = {item["name"] if isinstance(item, dict) else item for item in whitelist.get("exclusions", [])}
    allow_cmd = set(whitelist.get("allow_command_mismatch", []) or [])
    allow_labels = set(whitelist.get("allow_label_mismatch", []) or [])
    allow_status = set(whitelist.get("allow_status_mismatch", []) or [])
    allow_depends = set(whitelist.get("allow_depends_mismatch", []) or [])

    cmd_cache: dict[tuple[str, ...], list[str]] = {}

    def _normalized(cmd: list[str]) -> list[str]:
        key = tuple(cmd)
        cached = cmd_cache.get(key)
        if cached is not None:
            return cached
        normalized = _normalize_command(cmd, args.repo_root, args.build_dir)
        cmd_cache[key] = normalized
        return normalized

    manifest = {item["name"]: item for item in manifest_list}
    manifest = _apply_renames(manifest, renames)
    inventory = {item["name"]: item for item in inventory_list}

    missing = sorted(name for name in manifest if name not in inventory and name not in exclusions)
    extra = sorted(name for name in inventory if name not in manifest and name not in exclusions)

    mismatched_cmd = []
    mismatched_labels = []
    mismatched_status = []
    mismatched_depends = []

    for name, man in manifest.items():
        if name in exclusions or name not in inventory:
            continue
        inv = inventory[name]
        if name not in allow_cmd:
            man_cmd_raw = man.get("command", [])
            inv_cmd_raw = inv.get("command", [])
            if man_cmd_raw != inv_cmd_raw:
                man_cmd = _normalized(man_cmd_raw)
                inv_cmd = _normalized(inv_cmd_raw)
                if man_cmd != inv_cmd:
                    mismatched_cmd.append((name, man_cmd, inv_cmd))
        man_labels = sorted(set(man.get("labels", [])))
        inv_labels = sorted(set(inv.get("labels", [])))
        man_compare = sorted(set(man_labels) & LABELS_TO_COMPARE)
        inv_compare = sorted(set(inv_labels) & LABELS_TO_COMPARE)
        manual_followup = "needs_manual_followup" in man_labels or "needs_manual_followup" in inv_labels
        if name not in allow_labels and not manual_followup:
            if man_compare != inv_compare:
                mismatched_labels.append((name, man_compare, inv_compare))
        if name not in allow_status:
            if man.get("status") != inv.get("status"):
                mismatched_status.append((name, man.get("status"), inv.get("status")))
        if name not in allow_depends and "depends" in man:
            man_depends = sorted(set(man.get("depends", [])))
            inv_depends = sorted(set(inv.get("depends", [])))
            if man_depends != inv_depends:
                mismatched_depends.append((name, man_depends, inv_depends))

    issues = []
    if missing:
        issues.append(f"Missing tests: {len(missing)}")
    if extra:
        issues.append(f"Inventory-only tests: {len(extra)}")
    if mismatched_cmd:
        issues.append(f"Command mismatches: {len(mismatched_cmd)}")
    if mismatched_labels:
        issues.append(f"Label mismatches: {len(mismatched_labels)}")
    if mismatched_status:
        issues.append(f"Status mismatches: {len(mismatched_status)}")
    if mismatched_depends:
        issues.append(f"Depends mismatches: {len(mismatched_depends)}")

    if issues:
        print("Parity check failed:")
        for item in issues:
            print(f"  {item}")
        if missing:
            for name in missing:
                print(f"  missing: {name}")
        if extra:
            for name in extra:
                print(f"  inventory-only: {name}")
        for name, man_cmd, inv_cmd in mismatched_cmd:
            print(f"  command mismatch: {name}")
            print(f"    manifest: {man_cmd}")
            print(f"    inventory: {inv_cmd}")
        for name, man_labels, inv_labels in mismatched_labels:
            print(f"  labels mismatch: {name}")
            print(f"    manifest: {man_labels}")
            print(f"    inventory: {inv_labels}")
        for name, man_status, inv_status in mismatched_status:
            print(f"  status mismatch: {name} manifest={man_status} inventory={inv_status}")
        for name, man_depends, inv_depends in mismatched_depends:
            print(f"  depends mismatch: {name}")
            print(f"    manifest: {man_depends}")
            print(f"    inventory: {inv_depends}")
        return 1

    print("Parity check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
