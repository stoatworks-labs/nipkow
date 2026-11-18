# nipkow

> **AI-assisted project.** This codebase was created with [Claude](https://claude.com/claude-code)
> (Anthropic), directed and reviewed by a human author. It has run in Resolume Arena on a Mac
> and on Windows, over a still test card ([In Resolume](#in-resolume)), but never through a
> show. What's measured is measured by an offline harness, `nktest`, that drives
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

> **Photosensitivity warning.** SW Nipkow flickers, as a Baird televisor did. **At its
> defaults** the picture flickers in saturated red 12½ times a second, over most of the field
> on bright clips, past WCAG's red-flash threshold; some settings make it flash harder or over
> the whole frame. Flicker like this can trigger seizures in people with photosensitive
> epilepsy. Persistence turned up to 1 s calms it. See [Flashing](#flashing).

<!-- downloads:start -->

## Download

**[v0.1.0](https://github.com/stoatworks-labs/nipkow/releases/tag/v0.1.0)** — prebuilt for macOS and Windows. Pick your platform:

<details>
<summary><b>macOS</b> — Universal (Apple Silicon + Intel)</summary>

| Build | Download | Size |
| --- | --- | --- |
| Universal (Apple Silicon + Intel) · .dmg disk image | [`nipkow-0.1.0-macos-universal.dmg`](https://github.com/stoatworks-labs/nipkow/releases/download/v0.1.0/nipkow-0.1.0-macos-universal.dmg) | 225 KB |
| Universal (Apple Silicon + Intel) · .zip archive | [`nipkow-macos-universal.zip`](https://github.com/stoatworks-labs/nipkow/releases/latest/download/nipkow-macos-universal.zip) | 183 KB |

</details>

<details>
<summary><b>Windows</b> — x64</summary>

| Build | Download | Size |
| --- | --- | --- |
| x64 · .exe installer | [`nipkow-0.1.0-windows-x86_64-setup.exe`](https://github.com/stoatworks-labs/nipkow/releases/download/v0.1.0/nipkow-0.1.0-windows-x86_64-setup.exe) | 249 KB |
| x64 · .zip archive | [`nipkow-windows-x86_64.zip`](https://github.com/stoatworks-labs/nipkow/releases/latest/download/nipkow-windows-x86_64.zip) | 117 KB |

</details>

All builds, checksums and release notes: [github.com/stoatworks-labs/nipkow/releases](https://github.com/stoatworks-labs/nipkow/releases).

The Windows builds are unsigned, so SmartScreen warns once.

<!-- downloads:end -->

## Controls

| Group | |
| --- | --- |
| **Transmitter** | Lines (8–120; 30 the standard), Picture Rate (2–30 pictures a second; 12.5 the standard), Scanner (Drum, the BBC's mirror drum: straight lines that the disc draws as arcs; Disc: the same arcs at both ends, so the geometry cancels). |
| **Amplifier** | Bandwidth (0.5–40 kHz; 9 kHz, the BBC's channel). |
| **Receiver** | Lamp (the neon's standing current, 0–30 mA; the signal swings it ±10 mA), Sync (Locked: an ideal synchroniser; Hunting: the phonic wheel; Free: its coils disconnected), Speed (the motor's own speed error, ±5%), Framing (where the wheel locks, ±½ line). |
| **Look** | Persistence (how long the screen holds its light, 20 ms–1 s), Frame (Baird: the 7:3 field, the whole sector fitted to the height; Fill: stretched to the frame), Mix. |

The defaults are the 30-line standard (30 lines, 12½ pictures, the drum, 9 kHz), Sync
Hunting with the motor 0.5% fast, Persistence 0.1 s (it flickers: see
[Flashing](#flashing)) and the Baird frame. **The Lamp's 10.1 mA
was set from Resolume's own demo clips**: the lowest standing current at which the median
clip (of all 33) keeps the lamp lit over 99% of its picture. Below 10 mA black puts the lamp
out (the drive reaches zero), and it strikes again a formative lag later; the wheel's
reference pull was set from the same clips.

Over a transparent part of a clip the transmitter sees black; the receiver paints the whole
frame, so the output is opaque at Mix 1.

## Flashing

A televisor lights its picture one point at a time, each point once a picture, and Baird's
flickered for exactly that reason. Here the screen holds its light for **Persistence**, so
every lit point swings by about (1 − e^(−T/Persistence)) of its light once a picture of T
seconds: about half of it at the defaults (0.1 s, 12½ pictures a second). In the neon's
saturated red, WCAG 2.x counts a swing of more than 20 on its red scale (320 × (R − G − B),
in linear light) as half a flash, so **at the defaults every lit point of the field flashes
red 12½ times a second**. WCAG fails a flash that covers more than a quarter of a ten-degree
window (a third of the frame each way). The plugin keeps the flicker, because it is what a
Baird televisor did; this section says where it is.

| Setting (everything else at the defaults) | Per pixel: share of the worst window over 3 red flashes a second | Window mean: red flashes a second |
| --- | --- | --- |
| **The defaults**, on white | **70%** | 3.0 |
| **The defaults**, on Resolume's brightest demo clips (NoHopeJustFear_44, Metalive 01) | **64%, 62%** | 1.0 |
| **The defaults**, on the harness's card; on IntoTheGlow_02 | **50%; 39%** | 0.5; 2.0 |
| **The defaults**, on a flat 25% grey | **33%** | 0 |
| The defaults, on dark clips (Galactucity_21, Cyberspace_09) | 10%, 5% | 0 |
| **Frame Fill**, on white; on the brightest clip | **100%** | **13.0; 5.0** |
| **Lamp 15, 20, 30 mA**, on white | **70%** | **13.0, 12.5, 6.5** |
| **Picture Rate 5, 8**, on white (luminance flashes too) | **70%** | **5.5, 8.5** |
| **Persistence 0.05 s or less**, on white (luminance flashes too) | **70%** | **13.0** |
| **Sync Free**, on white | **70%** | **3.5** |
| Persistence 0.2 s, on the brightest clip | 5% | 0.5 |
| Persistence 0.6 s, on white | 0% | 0.5 |
| Persistence 1 s, on every source above, at any Lamp | 0% (Lamp 30 mA on white: 15%) | 0.5 |
| **Persistence 1 s, Picture Rate 4 to 7**, on white | **47–70%** | 0.5 |

The general (luminance) threshold is crossed only where the table says so: at the defaults the
light swings by less than 0.1 of white. **To calm it, turn Persistence up**: 0.2 s or more
keeps Resolume's brightest demo clip under WCAG's line, 0.6 s or more keeps white under, and at
1 s nothing measured crosses except a Picture Rate of 4 to 7. Turning it down, Frame Fill, a
higher Lamp, or a Picture Rate near 5 or 8 makes it worse.

Measured with `nktest --pipe`, 6 s at 640×360 and 60 fps, two ways. **Per pixel**: each pixel's
own series, counting its flashes in every one-second span, then the share of the worst window
(a third of the frame each way, stepped by a sixth) whose pixels flash more than three times
in the same span. **Window mean**: the fleet's video check, the mean colour of the frame and
of each such window. The rolling scan lights neighbouring lines at different moments, so the
window mean averages most of the flicker out: it is the measure a video is cut to, and it
understates what an eye sees across the field.

## Status

**v0.1.0, released 18 November 2026.** A [user guide](https://stoatworks-labs.com/software/nipkow/guide/)
covers every control, the flashing and the known limits.

### Measured offline, on macOS

`tools/verify.sh` passes on this machine (Apple Silicon, macOS 26) against a fresh universal
Release build (16 November 2026, and again on 18 November). Every picture check runs at **two rasters**, 1280×720 and
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

Render cost, from verify.sh's run on 18 November 2026 with other builds sharing the machine,
at the defaults over the harness's card: GPU time **0.43 ms** a frame at 720p (worst 1.00) and
**0.87 ms** at 1080p (worst 1.65); the CPU's time in `ProcessOpenGL` (the signal chain, and the
table's read-back, which waits for the GPU) **0.45 ms** and **0.76 ms** (worst 0.97 and 1.52).
At 4K: 3.17 ms GPU, 2.65 ms CPU. Other runs on the same machine read 0.72 to 0.87 ms GPU at
1080p: the medians move with what else is running. macOS only.

### In Resolume

**In Arena on macOS** (7.29.0, Apple Silicon, 18 November 2026, the release workflow's
universal build): it loads and lists SW Nipkow with Arena's Opacity and all 16 controls,
defaults and option names as declared; the About line reads "Nipkow v0.1.0 - MIT - Stoatworks
Labs, stoatworks-labs.com". Over a still of the harness's card on a layer added on top of a
show, read from Arena's composition monitor as the mean difference in 8-bit levels over the
middle of the frame:

| step | difference |
| --- | --- |
| the bare still, twice | 0.0 |
| SW Nipkow added at its defaults | 81.9 |
| the defaults, a second apart (the flicker and the hunting wheel) | 1.5–1.7 |
| Sync Locked and Persistence 1 s, a second apart | 0.09 |
| Lines 8 → 120 | 14.8 |
| Picture Rate 2 → 30 a second | 1.9 |
| Scanner Drum → Disc | 1.3 |
| Bandwidth 0.5 → 40 kHz | 4.5 |
| Lamp 6 → 24 mA | 20.9 |
| Framing −½ → +½ line | 8.2 |
| Frame Baird → Fill | 24.2 |
| Persistence 20 ms → 1 s | 4.5 |
| Mix 0 → 1 | 81.9 |
| Mix 0 against the bare still | 0.0 |
| Sync Free, Speed 0, 2 s apart | 0.10 |
| Sync Free, Speed +1%, 2 s apart (the picture drifts) | 2.25 |

Each control was read in the calm context (Sync Locked, Persistence 1 s), Persistence itself
under Sync Locked and Speed under Free. The four About buttons weren't pressed (they open a
browser).

**In Arena on Windows** (7.27.1 on win-lab, Mesa llvmpipe software rendering, 18 November
2026): the fleet's gate ran the release workflow's own Windows build of the registered tree.
The DLL loads and registers (`NK01`, an effect); all 17 controls (Nipkow's 16 and Arena's
Opacity) match their declared names, order, types, ranges and defaults; it renders (83.4
levels from its bare input at the defaults); Arena's log stays clean: 9 passed, 0 failed, 0
skipped. Nine of the eleven valued controls changed Arena's picture, read from its clip
thumbnail and held under Sync Locked with Persistence 1 s where the hunting wheel and the
flicker would move it between grabs (plugin-bench's `arena/expect/nipkow.json`); Speed and
Persistence read inconclusive there (under Free the drift moves the floor's own grabs too),
and both changed the picture on the Mac. The v0.1.0 release's own Windows zip was gated
again the same morning, since its DLL differs by hash from the pre-tag build (build
timestamps): 9 passed, 0 failed, 0 skipped again, and this time Persistence read live as well
(1.09 levels, averaged); Speed was inconclusive again. Persistence sits close to the
floor on a still, so either reading can come up.

### Not established

- **Not through a show.** Arena has run it over a still test card only; Avenue never.
  Nothing has been timed inside Resolume.
- **Windows on a GPU.** Only software rendering (win-lab's llvmpipe) has been seen.
- **Chosen, not measured:** the amplifier as one pole (no period response curve was found);
  the output stage's 10 kΩ (so the lamp strikes above 3 mA); the wheel's hunting frequency
  (1.5 Hz at the reference pull) and the motor's 1 s time constant; where on the plate a
  starved glow sits (centred); the neon's colour (a spectral lamp's chromaticity, brought
  into sRGB at constant luminance); four samples a picture element; Persistence's 0.1 s,
  which flickers (see [Flashing](#flashing)).
- **The formative lag** is an indicator lamp's (GE, Signalite), not a television lamp's.
- **Left out:** the eye, the amplifier's low-frequency cut and peaking, the statistical
  striking lag, the wheel's impulse shape and the motor's torque ripple, the graduated end
  holes, the radio path.
- **Bright, varied pictures can keep the wheel hunting.** The pull grows with the square of
  the lamp's current, with no limit (a real wheel's iron would saturate; nothing here does);
  on the harness's card at Lamp 9 mA and Speed +1% the wheel's own
  frequency comes near half the picture rate, the picture's pull pattern excites it, and it
  never settles. At the defaults on the same card it locks. This follows from the model;
  whether a Televisor did it is not known.
- **The default Lamp leaves black only just lit.** At 10.1 mA, black drives the lamp 0.1 mA
  above the point where it goes out, so black is a faint glow over part of the plate; a Lamp
  a notch lower puts black out, and the lamp then strikes again a formative lag after every
  return from black. The default was measured from the demo clips (above); whether it is the
  right one for a show is open.
- **Whether the Fill frame is wanted** is open. It stretches the 7:3 field to the whole frame,
  which no televisor did, and anything that flickers then flickers everywhere.
- **Resolume's alpha.** The plugin treats a clip's colour as straight alpha, which is what
  Arena's DXV demo clips decode to; whether Resolume hands an effect premultiplied colour has
  not been checked.
- The defaults were measured on the demo clips through `--pipe`, not looked at in a show.
- No presets and no OpenFX port.

## Browser demo

[nipkow-demo.stoatworks-labs.com](https://nipkow-demo.stoatworks-labs.com/) runs the plugin in
your browser: its own C++ (the plugin class, the lamp, the disc and the wheel in double, and
its clock), compiled unmodified to WebAssembly and drawing with its own GLSL in WebGL2, on
generated clips or a picture of your own. WebGL2 compiles GLSL ES 3.00, which has no `precise`,
so the page drops the exposure pass's seven: a browser's compiler may then reassociate the
arithmetic they protect, which works out under one 8-bit step but has not been measured. It
needs 32-bit float render targets (`EXT_color_buffer_float`) and stops without them. The
[photosensitivity warning](#flashing) applies there too: it flickers at the defaults, so the
page opens behind the warning and draws nothing until you press Start, whatever link (or
embed) brought you there. It is a demo, not the plugin, and the page lists what it leaves out.

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
dead controls and the render cost. CI (`.github/workflows/ci.yml`) builds on macOS and
Windows and runs only what needs no GPU (`nktest --offline` and `glslc`): this script is the
verification.
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
