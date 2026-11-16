"""Resolume's own demo clips through `nktest --pipe`: the alpha check, and
the measurements the defaults were set from. (intensifier's script; the
measurements are nipkow's.)

    tools/clips.py alpha [--build DIR]      the output alpha on the DXV alpha clips
    tools/clips.py defaults [--build DIR]   the standing current the clips need,
                                            and the phonic wheel's reference pull

The clips are Resolume Arena's (`/Applications/Resolume Arena/media`), DXV
with alpha, decoded by ffmpeg (`-nostdin`: ffmpeg reads the terminal
otherwise, and a script running it in a loop loses its input). Nothing here
is a second copy of the plugin's formulas: every number is read off the
pipe's frames or its `--stats` lines.

**alpha.** The plugin's decision: output alpha = mix( a, 1, Mix ) -- the
receiver paints the whole frame, so at Mix 1 the output is opaque, and the
clip's light reaches the transmitter weighted by its alpha (a transparent
pixel is black to the photocell). Checked on BattleWeapon_Tank_09 and
Ethnik2, the two clips with real transparency: at Mix 1 every output alpha is
255; at Mix 0.5 every output alpha is within half a code of a / 2 + 127.5;
and white painted under every fully transparent pixel changes no output byte
(the clips are straight alpha over black, so without the paint a plugin that
ignored alpha would pass). The negative control paints ~200 opaque pixels
instead, which must show.

**defaults.** Lamp: for each demo clip, the lowest standing current at which
the lamp is lit for 99% of the picture's samples (the sync's left out: below
10 mA the sync puts the lamp out every line by design), found by bisection on
the pipe's own --stats over frames 5..29 at 640x360, every other control at
its default (the lamp does not depend on the wheel, so Sync is Locked). The
default is the median clip's. The reference pull C_ref: every clip's mean C
(the --stats pull, each line's mean square current over the picture less
that over the sync) at that Lamp; C_ref is the median clip's.
"""

import argparse
import pathlib
import statistics
import subprocess
import sys

import numpy as np

REPO = pathlib.Path(__file__).resolve().parent.parent
MEDIA = pathlib.Path("/Applications/Resolume Arena/media")
ALPHA_CLIPS = ["Shop74/BattleWeapon_Tank_09.mov", "Shop74/Ethnik2.mov"]
LAMP_MAX = 30.0  # mA: Lamp is 0..30 mA, linear (the spec)


def decode(path, width, height, frames, start=1.0):
    args = ["ffmpeg", "-nostdin", "-v", "error", "-ss", str(start), "-i", str(path), "-frames:v", str(frames),
            "-vf", f"scale={width}:{height}", "-f", "rawvideo", "-pix_fmt", "rgba", "-"]
    raw = subprocess.run(args, capture_output=True, check=True).stdout
    size = width * height * 4
    got = len(raw) // size
    if got == 0:
        raise RuntimeError(f"ffmpeg decoded nothing from {path}")
    return raw[:got * size], got


def pipe(nktest, raw, width, height, settings, stats=False, fps=30):
    args = [str(nktest), "--pipe", "--size", f"{width}x{height}", "--fps", str(fps)]
    for s in settings:
        args += ["--set", s]
    if stats:
        args += ["--stats"]
    result = subprocess.run(args, input=raw, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(f"nktest --pipe failed ({result.returncode}): {result.stderr.decode()[-400:]}")
    frames = np.frombuffer(result.stdout, dtype=np.uint8).reshape(-1, height, width, 4)
    rows = []
    for line in result.stderr.decode().splitlines():
        if line.startswith("frame "):
            words = line.split()
            fields = dict(zip(words[0::2], words[1::2]))
            rows.append({k: float(v) for k, v in fields.items()})
    return frames, rows


def alpha_check(nktest):
    width, height, count = 320, 180, 20
    failures, negative_caught = 0, 0
    for name in ALPHA_CLIPS:
        path = MEDIA / name
        if not path.exists():
            print(f"  FAIL  {path} is missing: the alpha check needs Resolume Arena's demo clips")
            return 1
        raw, frames = decode(path, width, height, count)
        clip = np.frombuffer(raw, dtype=np.uint8).reshape(frames, height, width, 4)
        a = clip[..., 3].astype(np.float64)
        full, _ = pipe(nktest, raw, width, height, ["Mix=1"])
        half, _ = pipe(nktest, raw, width, height, ["Mix=0.5"])
        opaque_alpha = int(full[..., 3].min())
        off_half = float(np.abs(half[..., 3].astype(np.float64) - (a / 2.0 + 127.5)).max())
        # The clips are straight alpha over black: where a = 0 the colour is
        # (almost always) black already, so a plugin that ignored alpha would
        # look the same. Paint every fully transparent pixel white: the output
        # must not change by a single byte.
        white = clip.copy()
        hidden = white[..., 3] == 0
        white[..., :3][hidden] = 255
        painted, _ = pipe(nktest, white.tobytes(), width, height, ["Mix=1"])
        changed = int(np.count_nonzero(painted != full))
        # The negative control: the same paint on a few opaque pixels must show.
        shown = clip.copy()
        opaque = np.argwhere(shown[..., 3] == 255)
        pick = opaque[:: max(1, len(opaque) // 200)]
        shown[pick[:, 0], pick[:, 1], pick[:, 2], :3] = 255
        control, _ = pipe(nktest, shown.tobytes(), width, height, ["Mix=1"])
        negative_caught += int(np.count_nonzero(control != full)) > 0
        ok = opaque_alpha == 255 and off_half <= 0.5 + 1e-9 and changed == 0
        failures += not ok
        print(f"  {'ok  ' if ok else 'FAIL'}  {path.name}: {frames} frames at {width}x{height}, {hidden.mean():.0%} fully "
              f"transparent; Mix 1: alpha >= {opaque_alpha} (want 255); Mix 0.5: alpha within {off_half:.2f} codes of "
              f"a/2 + 127.5 (tolerance 0.5, the byte's rounding); white painted under every transparent pixel changes "
              f"{changed} output bytes (want 0)")
    print(f"  {'ok  ' if negative_caught == len(ALPHA_CLIPS) else 'FAIL'}  negative control: white painted on ~200 "
          f"opaque pixels shows in the output on {negative_caught} of {len(ALPHA_CLIPS)} clips")
    failures += negative_caught != len(ALPHA_CLIPS)
    print(f"\n  {'PASS' if failures == 0 else 'FAIL'}")
    return 0 if failures == 0 else 1


def lit_share(nktest, raw, width, height, lamp_ma):
    _, rows = pipe(nktest, raw, width, height, [f"Lamp={lamp_ma / LAMP_MAX:.7f}", "Sync=Locked"], stats=True)
    rows = rows[5:]
    total = sum(r["samples"] for r in rows)
    return sum(r["lit"] * r["samples"] for r in rows) / total if total else 0.0


def threshold(nktest, raw, width, height):
    """The lowest standing current (mA) lighting 99% of the picture's samples."""
    lo, hi = 0.0, LAMP_MAX
    if lit_share(nktest, raw, width, height, hi) < 0.99:
        return None
    if lit_share(nktest, raw, width, height, lo) >= 0.99:
        return lo
    for _ in range(14):
        mid = 0.5 * (lo + hi)
        if lit_share(nktest, raw, width, height, mid) >= 0.99:
            hi = mid
        else:
            lo = mid
    return hi


def mean_pull(nktest, raw, width, height, lamp_ma):
    _, rows = pipe(nktest, raw, width, height, [f"Lamp={lamp_ma / LAMP_MAX:.7f}", "Sync=Locked"], stats=True)
    rows = rows[5:]
    return statistics.fmean(r["pull"] for r in rows)


def defaults(nktest):
    clips = sorted(p for p in (MEDIA / "AV").glob("*.mov")) + sorted(p for p in (MEDIA / "Shop74").glob("*.mov"))
    if not clips:
        print(f"no demo clips under {MEDIA}")
        return 1
    width, height = 640, 360
    decoded, found = {}, []
    for path in clips:
        raw, _ = decode(path, width, height, 30)
        decoded[path] = raw
        t = threshold(nktest, raw, width, height)
        found.append((t if t is not None else float("inf"), path))
        print(f"  {path.parent.name}/{path.name:28s} lit for 99% of the picture from {t:.3f} mA" if t is not None else
              f"  {path.parent.name}/{path.name:28s} never lit for 99% of the picture at 30 mA")
    found.sort(key=lambda tp: tp[0])
    median_ma, median_clip = found[len(found) // 2]
    finite = [t for t, _ in found if t != float("inf")]
    lamp = round(median_ma + 0.05, 1)  # up to the next 0.1 mA: lit for at least 99%
    print(f"\n{len(clips)} clips: from {finite[0]:.3f} to {finite[-1]:.3f} mA; the median clip, {median_clip.name}, "
          f"{median_ma:.3f} mA -> Lamp {lamp:.1f} mA (param {lamp / LAMP_MAX:.6f})\n")

    pulls = []
    for path in clips:
        c = mean_pull(nktest, decoded[path], width, height, lamp)
        pulls.append((c, path))
        print(f"  {path.parent.name}/{path.name:28s} mean C at {lamp:.1f} mA: {c:.2f} mA^2")
    pulls.sort(key=lambda cp: cp[0])
    median_c, median_pull_clip = pulls[len(pulls) // 2]
    print(f"\n{len(clips)} clips: mean C from {pulls[0][0]:.2f} to {pulls[-1][0]:.2f} mA^2; the median clip, "
          f"{median_pull_clip.name}, {median_c:.2f} mA^2 -> C_ref {median_c:.1f} mA^2")
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("what", choices=["alpha", "defaults"])
    parser.add_argument("--build", type=pathlib.Path, default=REPO / "build")
    args = parser.parse_args()
    build = args.build if args.build.is_absolute() else REPO / args.build
    nktest = build / "nktest"
    if not nktest.exists():
        print(f"{nktest} not found", file=sys.stderr)
        return 1
    if args.what == "alpha":
        print("\n=== alpha on a real clip: Resolume's DXV alpha clips through --pipe")
        return alpha_check(nktest)
    return defaults(nktest)


if __name__ == "__main__":
    sys.exit(main())
