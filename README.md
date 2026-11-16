# nipkow

> **AI-assisted project.** This codebase was created with [Claude](https://claude.com/claude-code)
> (Anthropic), directed and reviewed by a human author. **It has never been loaded into
> Resolume.** What's established is established by an offline harness, `nktest`, that drives
> the real plugin class through the same `ProcessOpenGL` a host calls, in a headless GL
> context on a synthetic clock, on this Mac's GPU at two rasters and on Apple's software
> renderer. Read off the picture, each scanned line sits at the spiral's radius to within
> **0.008 px**, a dot drawn by the mirror drum moves **r_k / r_c** as far along the arc as on
> the picture (to **0.009%**), every line's brightness falls as **1/r** to **2e-7**, and a
> free-running receiver's picture drifts **N × speed error × f** lines a second to **1e-6** of
> a line. The phonic wheel hunts at the linearised swing equation's frequency to **1.2e-4**
> and locks where the equation says to **4e-4**. **24** deliberately wrong models and **51**
> one-character mutants of the shipped code are all caught. The amplifier's single pole, the
> lamp's source resistance, the wheel's 1.5 Hz and 1 s, and the glow's place on the plate are
> **chosen**, not measured: see [Status](#status).

Baird's 30-line mechanical television looking at your clip, for
[Resolume](https://resolume.com) Arena and Avenue: an FFGL effect, **SW Nipkow**.

**The picture is drawn by a spinning disc, one hole at a time, in front of a neon lamp whose
brightness follows the signal.** A mirror drum at the transmitter explores the clip in 30
straight vertical strips, 12½ pictures a second, as the BBC's did in 1932–35; the signal goes
through a narrow amplifier into a flat-plate neon lamp; a disc with 30 holes on a spiral
turns in front of the lamp, each hole tracing one arc of the picture; a toothed wheel on the
motor shaft, its coils in series with the lamp, holds the disc in step with the transmitter,
and hunts. Nothing in the picture is drawn. The lines are arcs because the holes travel in
circles; the picture is taller and dimmer on its outer side, because a straight line from the
drum becomes an arc as long as its radius and an outer hole dwells less on each point; edges
smear upwards along the lines, the same angle on every line; a lamp starved of current glows
over only part of its plate; a dark picture holds the wheel weakly, so it swings and slips;
and every point flickers at 12½ Hz because it is lit once a picture.

## Controls

| Group | |
| --- | --- |
| **Transmitter** | Lines (8–120; 30 the standard), Picture Rate (2–30 pictures a second; 12.5 the standard), Scanner (Drum, the BBC's mirror drum: straight lines that the disc draws as arcs; Disc: the same arcs at both ends, so the geometry cancels). |
| **Amplifier** | Bandwidth (0.5–40 kHz; 9 kHz, the BBC's channel). |
| **Receiver** | Lamp (the neon's standing current, 0–30 mA; the signal swings it ±10 mA), Sync (Locked: an ideal synchroniser; Hunting: the phonic wheel; Free: its coils disconnected), Speed (the motor's own speed error, ±5%), Framing (where the wheel locks, ±½ line). |
| **Look** | Persistence (how long the screen holds its light, 20 ms–1 s), Frame (Baird: the 7:3 field, the whole sector fitted to the height; Fill: stretched to the frame), Mix. |

The defaults are the 30-line standard (30 lines, 12½ pictures, the drum, 9 kHz), Sync
Hunting with the motor 0.5% fast, Persistence 0.1 s and the Baird frame. **The Lamp's 10.1 mA
was set from Resolume's own demo clips**: the lowest standing current at which the median
clip (of all 33) keeps the lamp lit over 99% of its picture. Below 10 mA black puts the lamp
out (the drive reaches zero), and it strikes again a formative lag later; the wheel's
reference pull was set from the same clips.

Over a transparent part of a clip the transmitter sees black; the receiver paints the whole
frame, so the output is opaque at Mix 1.

## Status

**v0.1.0, local only, 16 November 2026. Never loaded into Resolume.**

### Measured offline, on macOS

`tools/verify.sh` passes on this machine (Apple Silicon, macOS 26) against a fresh universal
Release build (16 November 2026). Every picture check runs at **two rasters**, 1280×720 and
320×180, and again on **Apple's software renderer** at 320×180. The numbers below are from
`nktest --all` on the same code (an arm64 build); where two are given, the first is 1280×720.

| check | result |
| --- | --- |
| `--spiral` | alternate lines white, one frame: each lit line's light-weighted radius **0.0005 px** (0.008) from r_k + 1/(12 r_k) (tolerance 0.5 px); the scanning hole's light **0.22 px** (0.13) along the arc from the model's place |
| `--arc` | a dot drawn 18 holes below and 18 above the field's middle: with the drum it moves **37.5586** holes on the outermost line against 36 × r_k/r_c = **37.5587**, and **34.4353** against **34.4353** on the innermost (worst **0.009%**, tolerance 0.36–1.3%); with the disc **36.02** and **36.03** holes against the disc's own 36.01 and 36.02 (worst **0.13%** at 320×180) |
| `--dwell` | one picture under a steady lamp: each line's mean brightness against L w/(r_k ω) to **1.6e-7** (tolerance 1e-5), the outer line **0.91684** as bright as the inner = r_29/r_0; each line's length from the pixels the field's edge cuts, r_k Φ to **1.8e-7**; the field's light N line periods of the lamp's to **1.8e-7** |
| `--slip` | Free, Speed ±0.2% and ±0.4%: the sync's band drifts **0.36000** lines against N × Speed × f × t = **0.36000**, worst **7e-8** (9e-7) lines (tolerance half a pixel at each end, 0.0015 and 0.006 lines) |
| `--bandwidth` | an edge between 2 kHz and 20 kHz moves **0.005625 rad** on every line, worst **4.3e-5** (7.2e-5) off (tolerance 2e-4): **1.80** holes along the inner line, **1.96** along the outer |
| `--lamp` | light ∝ current: 20 mA over 10 mA **2.000001** (tolerance 2e-5); after a black-to-grey step the light missing **102.85 µs** against the formative lag through the amplifier's **102.78 µs** at 30 V over, **38.49** against **38.46 µs** at 60 V (worst 0.07 µs, 0.29 µs at 320×180; tolerance two samples, 1.19 µs); a glow at 4 mA and 2.5 mA covers **0.8944** and **0.7071** of the field each way against √(I/5 mA) (worst 5.3e-5) |
| `--hunting` | after a framing step, at twice the reference pull: swings at **13.3177** rad/s against **13.3193** (−1.2e-4, tolerance 1.5e-3), decays at **0.50004**/s against 1/(2τ_m) = 0.5; locked **0.08333** lines off at half the pull-out speed against asin(…)/2π = **0.08333**, **0.17829** against **0.17822** at 90% of it at half the pull; at 110% it slips (**39.8** lines in 6 s) |
| `--flicker` | a pixel's exposure frame after frame against the closed form of its scans: worst **1.1e-6** at 60, 24 and 8 fps (tolerance 1e-5) |
| `--rate` | 24, 30, 60 and 120 fps against 240 fps at 0.5–2 s: samples, cell, amplifier, lamp and strike progress **bit-exact**; ψ to **1.1e-14** rev; the exposure to **9.3e-7** of its peak (tolerance 1.3e-4); the samples run exactly t N M f |
| `--colour` | the output in the neon's proportions, G/R to **5.2e-9**; a lamp held at 20 mA averages **0.4994** linear at the field's middle (want 0.5, tolerance 9.8e-3 from the phases' discrepancy) |
| `--alpha` | alpha = mix(α, 1, Mix) to **3e-8**; white under α = 0 changes **0** exposure values; a grey under α sends what its light × α sends opaque, to **3.5e-7**; Mix 0 the clip bit for bit. On two of Arena's own alpha clips, white painted under every transparent pixel changes **0** output bytes |
| `--resize` | 1280×720 → 320×180 mid-run: the screen's light **30.9659** against a run at 320×180 throughout **30.9659** (−1.4e-6); 30 frames on, the wheel **4.4e-8** rev from a run never resized (tolerance 2.6e-7 from the runs' own pulls and swing), the lamp and the samples exact |
| `--clock` | a host clock at 4.99e8 ms against one from 0: **0** of 14,745,600 values differ; seconds and milliseconds clocks settle on the right unit |
| `--glstate` | the host's GL state comes back, **0** things changed |
| `--geometry` | the conversions, r_c = 7N²/6π (334.225 holes at 30 lines), the lag through GE's points, the colour, the pull-out (3.77%); the amplifier's step withholds τ to **5.9e-8**; at a constant current the lamp lights the lag after the start, within its sample, to **1.4e-10 s** |
| `--pipe` | 2.5 frames in, exactly 2 out; bad cues and settings refused (exit 2); a closed stdout (`\| head -c 1`) exits 1, not 141 |
| `--negative` | **24** deliberately wrong models, **24** caught |
| mutation | **51** one-character mutants of the shipped GLSL and C++ (one on the software renderer), **51** caught |
| `tools/sweep.py` | all **11** controls measurably change the picture, with and without their context |
| shaders | all 5 compile through `glslc`, not merely through Apple's driver |
| the bundle | universal (`x86_64 arm64`), exports `plugMain`, ad-hoc signs; `oxbow` reads `SW Nipkow` / `NK01` / `effect` and renders through it |

Render cost, from verify.sh's final run on 16 November 2026 with other builds sharing the
machine, at the defaults over the harness's card: GPU time **0.72 ms** a frame at 720p (worst
0.95) and **0.85 ms** at 1080p (worst 1.08); the CPU's time in `ProcessOpenGL` (the signal
chain, and the table's read-back, which waits for the GPU) **0.69 ms** and **0.76 ms**. One
4K run: 2.88 ms GPU, 2.44 ms CPU. macOS only.

### Not established

- **No host has loaded it.** Not Arena, not Avenue, on any platform. `oxbow` (the fleet's test
  host) reads the bundle and renders through it, which is all.
- **Windows** has never been built, and there is no CI.
- **Chosen, not measured:** the amplifier as one pole (no period response curve was found);
  the output stage's 10 kΩ (so the lamp strikes above 3 mA); the wheel's hunting frequency
  (1.5 Hz at the reference pull) and the motor's 1 s time constant; where on the plate a
  starved glow sits (centred); the neon's colour (a spectral lamp's chromaticity, brought
  into sRGB at constant luminance); four samples a picture element; Persistence's 0.1 s.
- **The formative lag** is an indicator lamp's (GE, Signalite), not a television lamp's.
- **Left out:** the eye, the amplifier's low-frequency cut and peaking, the statistical
  striking lag, the wheel's impulse shape and the motor's torque ripple, the graduated end
  holes, the radio path.
- **Bright, varied pictures can keep the wheel hunting.** The pull grows with the square of
  the lamp's current; on the harness's card at Lamp 9 mA and Speed +1% the wheel's own
  frequency comes near half the picture rate, the picture's pull pattern excites it, and it
  never settles. At the defaults on the same card it locks. This follows from the model;
  whether a Televisor did it is not known.
- **Resolume's alpha.** The plugin treats a clip's colour as straight alpha, which is what
  Arena's DXV demo clips decode to; whether Resolume hands an effect premultiplied colour has
  not been checked.
- The defaults were measured on the demo clips through `--pipe`, not looked at in a show.
- No user guide, no presets, no OpenFX port and no browser demo.

## Build

macOS (universal: Apple Silicon and Intel):

```bash
git clone --recursive <this repo>
```

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build
```

```bash
cmake --install build
```

The last one copies `Nipkow.bundle` into `~/Documents/Resolume Arena/Extra Effects`.
Restart Arena and it's under Effects as **SW Nipkow**.

## Building and testing

```bash
OXBOW=/path/to/oxbow tools/verify.sh
```

That's the whole lot: shader hygiene and `glslc`, a fresh universal build with no warnings,
the bundle's architecture, plist and ad-hoc signature, a host's view of it through `oxbow`
(required), every check at two rasters and again on the software renderer, the alpha check on
Arena's demo clips (required), the pipe, the negative controls, the mutants, the sweep for
dead controls and the render cost. There is no CI: this script is the verification.
`CLAUDE.md` lists the checks one by one; `./build/nktest --all` runs them all.

Render a frame of your own:

```bash
./build/nktest --out /tmp/nipkow.png --set "Scanner=Disc" --set "Sync=Free"
```

Run a clip through it:

```bash
ffmpeg -nostdin -i in.mov -f rawvideo -pix_fmt rgba - | ./build/nktest --pipe --fps 30 --size 1280x720 | ffmpeg -f rawvideo -pix_fmt rgba -s 1280x720 -r 30 -i - out.mov
```

## License

MIT. See [LICENSE](LICENSE) and [ATTRIBUTIONS.md](ATTRIBUTIONS.md).

<!-- attributions:start -->
This project is built on other people's work — see [ATTRIBUTIONS.md](ATTRIBUTIONS.md).
<!-- attributions:end -->
