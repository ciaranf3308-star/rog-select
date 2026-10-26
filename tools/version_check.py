#!/usr/bin/env python3
"""Mirror of ParseVersion/IsNewerVersion in src/main.cpp (v1.0.7).

Run: python3 tools/version_check.py
Version compare must be numeric, not lexical: 1.0.10 > 1.0.9.
"""
import sys

def parse(v):
    s = v[1:] if v.startswith(("v", "V")) else v
    parts = s.split(".")
    parts += ["0"] * (3 - len(parts))
    return tuple(int(p) for p in parts[:3])

def newer(tag, current="1.0.7"):
    return parse(tag) > parse(current)

def main():
    cases = [
        ("v1.0.7", False), ("v1.0.8", True), ("v1.1.0", True),
        ("v2.0.0", True), ("v1.0.6", False), ("v1.0.10", True),
        ("v1.0.9", True), ("1.0.8", True),
    ]
    for tag, exp in cases:
        got = newer(tag)
        assert got == exp, f"{tag}: got {got}, expected {exp}"
        print(f"OK {tag} newer={got}")
    print("version_check PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
