"""Diff golden expected outputs vs ModelSim DUT outputs.

Both files are text, line-oriented; '#' starts a comment. Lines that are
shorter than the expected get extra fields treated as missing. Exit code 0
on full match, 1 on any mismatch.
"""

from __future__ import annotations

import argparse
import sys


def normalize(line: str):
    line = line.split("#", 1)[0].strip()
    return line.split() if line else None


def main():
    p = argparse.ArgumentParser()
    p.add_argument("expected")
    p.add_argument("dut")
    p.add_argument("--field-start", type=int, default=0,
                   help="ignore the first N fields (e.g., input fields)")
    args = p.parse_args()

    with open(args.expected) as f:
        exp_lines = [normalize(l) for l in f]
    with open(args.dut) as f:
        dut_lines = [normalize(l) for l in f]

    exp_lines = [l for l in exp_lines if l]
    dut_lines = [l for l in dut_lines if l]

    if len(exp_lines) != len(dut_lines):
        print(f"FAIL: line count exp={len(exp_lines)} dut={len(dut_lines)}",
              file=sys.stderr)
        return 1

    mismatches = 0
    for i, (e, d) in enumerate(zip(exp_lines, dut_lines)):
        e_out = e[args.field_start:]
        d_out = d[args.field_start:]
        if e_out != d_out:
            mismatches += 1
            print(f"line {i}: expected {' '.join(e_out)}", file=sys.stderr)
            print(f"         got      {' '.join(d_out)}", file=sys.stderr)
            if mismatches >= 10:
                print("(stopping after 10 mismatches)", file=sys.stderr)
                break

    if mismatches:
        print(f"FAIL: {mismatches} mismatches in {len(exp_lines)} lines",
              file=sys.stderr)
        return 1
    print(f"PASS: {len(exp_lines)} vectors")
    return 0


if __name__ == "__main__":
    sys.exit(main())
