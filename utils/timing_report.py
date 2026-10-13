#!/usr/bin/env python3
"""Markdown "Timing" section for a PR, from Quartus summary files.

Reads ap_core.sta.summary (worst slack per clock, all corners) and
ap_core.fit.summary (Quartus version, resources) of a build and, optionally,
of a "before" build to compare against (e.g. the upstream tree built with the
same flow).

A build is a directory that contains the summaries anywhere below it, or a
GitHub Actions run given as run:<id> (artifacts are fetched with `gh`).

  utils/timing_report.py run:38372364692
  utils/timing_report.py --before run:38282311267 run:38372364692 \\
      --repo ollisulopuisto/myc64-pocket --seed 2 \\
      --alias 'general[2]=clk_32mhz (system)' --alias 'general[0]=clk_8mhz'

Clock names are shortened (PLL output counters become "pll out N"); --alias
SUBSTRING=NAME renames any clock whose full name contains SUBSTRING.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

KINDS = ("Setup", "Hold", "Recovery", "Removal")


def find(root, name):
  for d, _, files in os.walk(root):
    if name in files:
      return os.path.join(d, name)
  return None


def fetch_run(run_id, repo, tmp):
  dest = os.path.join(tmp, run_id)
  cmd = ["gh", "run", "download", run_id, "-D", dest]
  if repo:
    cmd += ["-R", repo]
  subprocess.run(cmd, check=True, capture_output=True)
  view = ["gh", "run", "view", run_id, "--json", "headSha,headBranch"]
  if repo:
    view += ["-R", repo]
  info = json.loads(subprocess.run(view, check=True, capture_output=True, text=True).stdout)
  return dest, "%s (%s)" % (info["headSha"][:7], info["headBranch"])


def parse_sta(path):
  """{(kind, clock): (worst slack, corner, tns)} over all corner models."""
  worst = {}
  text = open(path).read()
  for m in re.finditer(r"Type\s*:\s*(.+?) Model (\w+(?: \w+)*?) '(.+?)'\s*\nSlack\s*:\s*(-?[\d.]+)"
                       r"(?:\s*\nTNS\s*:\s*(-?[\d.]+))?", text):
    corner, kind, clock, slack, tns = m.groups()
    if kind not in KINDS:
      continue
    slack = float(slack)
    key = (kind, clock)
    if key not in worst or slack < worst[key][0]:
      worst[key] = (slack, corner, float(tns or 0))
  return worst


def parse_fit(path):
  fit = {}
  for line in open(path):
    if " : " in line:
      k, v = line.split(" : ", 1)
      fit[k.strip()] = v.strip()
  return fit


def short_clock(name, aliases):
  for sub, alias in aliases:
    if sub in name:
      return alias
  m = re.search(r"general\[(\d+)\]\.gpll~PLL_OUTPUT_COUNTER\|divclk", name)
  if m:
    return "pll out %s" % m.group(1)
  return name.split("|")[-1] if len(name) > 40 else name


def load(spec, repo, tmp):
  label = spec
  if spec.startswith("run:"):
    root, label = fetch_run(spec[4:], repo, tmp)
  else:
    root = spec
  sta, fit = find(root, "ap_core.sta.summary"), find(root, "ap_core.fit.summary")
  if not sta or not fit:
    sys.exit("no ap_core.sta.summary / ap_core.fit.summary under %s" % root)
  return label, parse_sta(sta), parse_fit(fit)


def fmt(v):
  return "**%+.3f**" % v if v < 0 else "%+.3f" % v


def resource(fit, key):
  v = fit.get(key, "-")
  return re.sub(r"\s*\(\s*(\d+)\s*%\s*\)", r" (\1 %)", v)


def main():
  ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("after", help="build dir or run:<id>")
  ap.add_argument("--before", help="build dir or run:<id> to compare against")
  ap.add_argument("--repo", help="owner/name for run:<id> (default: current repo)")
  ap.add_argument("--seed", help="fitter seed of the after build (SEED in ap_core.qsf)")
  ap.add_argument("--alias", action="append", default=[], metavar="SUBSTRING=NAME")
  args = ap.parse_args()
  aliases = [tuple(a.split("=", 1)) for a in args.alias]

  with tempfile.TemporaryDirectory() as tmp:
    a_label, a_sta, a_fit = load(args.after, args.repo, tmp)
    b = load(args.before, args.repo, tmp) if args.before else None

  out = ["### Timing", ""]
  line = "Measured on %s, Quartus %s" % (a_label, a_fit.get("Quartus Prime Version", "?"))
  if args.seed:
    line += ", fitter seed %s" % args.seed
  out += [line + ". Worst slack in ns over all four corner models (slow/fast, 0/85 C).", ""]

  clocks = sorted({c for _, c in a_sta} | ({c for _, c in b[1]} if b else set()),
                  key=lambda c: short_clock(c, aliases))
  head = "| Clock | Setup | Hold |"
  rule = "|---|---:|---:|"
  if b:
    head += " Setup before | Hold before |"
    rule += "---:|---:|"
  out += [head, rule]
  for c in clocks:
    row = "| %s |" % short_clock(c, aliases)
    for sta in ([a_sta, b[1]] if b else [a_sta]):
      for kind in ("Setup", "Hold"):
        v = sta.get((kind, c))
        row += " %s |" % (fmt(v[0]) if v else "-")
    out.append(row)

  rr = [(k, c, v) for (k, c), v in a_sta.items() if k in ("Recovery", "Removal")]
  if rr:
    out += ["", "Recovery/removal: " + ", ".join(
        "%s %s %s" % (short_clock(c, aliases), k.lower(), fmt(v[0])) for k, c, v in sorted(rr))]
  neg = [(k, c, v) for (k, c), v in a_sta.items() if v[0] < 0]
  if neg:
    out += ["", "Failing: " + ", ".join(
        "%s %s %s (TNS %.3f, %s)" % (short_clock(c, aliases), k.lower(), fmt(v[0]), v[2], v[1])
        for k, c, v in sorted(neg))]

  out += ["", "| Resource | After |" + (" Before |" if b else ""), "|---|---|" + ("---|" if b else "")]
  for key, name in (("Logic utilization (in ALMs)", "ALMs"), ("Total registers", "Registers"),
                    ("Total RAM Blocks", "RAM blocks (M10K)"), ("Total DSP Blocks", "DSP blocks")):
    out.append("| %s | %s |" % (name, resource(a_fit, key)) + (" %s |" % resource(b[2], key) if b else ""))
  if b:
    out += ["", "Before: %s, Quartus %s." % (b[0], b[2].get("Quartus Prime Version", "?"))]
  print("\n".join(out))


if __name__ == "__main__":
  main()
