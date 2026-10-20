"""Reads the GPU profiles a photo tour wrote to the log (-DTGGpuProfile: one ProfileGPU per view) and
prints, per view, the frame's GPU time and the passes that cost the most, nested, so two runs can be set
against each other pass by pass instead of by feel. ProfileGPU writes two frames per call; the second is
the main view (the first is the sky light's real-time capture), and that is the one reported.

    python Tools/gpu_profile.py [Saved/Logs/DustToGlory.log] [--min 0.3] [--depth 36]
"""
import re
import sys

MARK = "Photo mode: GPU profile of the "


def profiles(path):
    """Yields (view name, rows) with rows as (indent, ms, name) for the main frame of each profile."""
    view = None
    rows = []
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if MARK in line:
                if view and rows:
                    yield view, rows
                view = line.split(MARK)[1].split(" view")[0].strip()
                rows = []
                continue
            if view is None or "LogRHI: Display:" not in line:
                continue
            cells = line.rstrip("\n").split("┃")
            if len(cells) < 4:
                continue
            found = re.search(r"([0-9.]+) ms", cells[2])
            if not found:
                continue
            name = cells[3]
            rows.append((len(name) - len(name.lstrip(" ")), float(found.group(1)), name.strip()))
    if view and rows:
        yield view, rows


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    path = args[0] if args else "Saved/Logs/DustToGlory.log"
    least = 0.3
    depth = 36
    for i, a in enumerate(sys.argv):
        if a == "--min":
            least = float(sys.argv[i + 1])
        if a == "--depth":
            depth = int(sys.argv[i + 1])
    for view, rows in profiles(path):
        roots = [i for i, r in enumerate(rows) if r[2] == "<root>"]
        main_rows = rows[roots[1]:] if len(roots) > 1 else rows
        total = main_rows[0][1] if main_rows else 0.0
        print("%s view: %.2f ms GPU" % (view, total))
        for indent, ms, name in main_rows[1:]:
            if ms >= least and indent <= depth:
                print("  %6.2f ms %s%s" % (ms, " " * max(0, (indent - 4) // 3), name[:70]))
        print()


main()
