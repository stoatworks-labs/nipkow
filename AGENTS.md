# AGENTS.md: Nipkow

Read this before changing the exposure pass (`kExposureFragment`), the table pass, the signal
chain (`Apparatus.cpp`), the clock, the resize or the composite. `CLAUDE.md` has the commands;
the spec is `~/Projects/resolume/specs/SPEC-nipkow.md`, written before any code and kept in
step with this repo (no As-built section: where the build departed from the first draft, the
spec was changed, and the changes are listed below).

## The idea

**The picture is drawn by a spinning disc, one hole at a time, in front of a neon lamp whose
brightness follows the signal.** The arcs, the sector-shaped field, the outer side taller and
dimmer, the upward smear, the starved glow, the 12½ Hz flicker and the wheel's hunting and
slipping all come out of the scan, the lamp and the swing equation; none of them is drawn.
Neighbours not duplicated: `old-cathode` (an electronic signal path and a CRT), `slowscan`,
`teletext`. None is mechanical.

## Decisions taken without asking (the brief says decide and write it down)

- **Identity.** `SW Nipkow`, `NK01` (checked free against every `"XX01"` in
  `~/Projects/*/*/source` and `~/dev/*/source`), an effect, `com.stoatworks.ffgl.nipkow`,
  harness `nktest`. One class, `NipkowPlugin`; host indices are the ParamIds, the About block
  last. Lines is an FFGL integer (8..120) holding the integer itself.
- **Three stages, split by what each needs.** The transmitter is a GPU pass (the table: N × M
  samples of the clip's linear luminance × α, each the average over a square aperture one hole
  across, centred on the drum's straight line or the disc's arc), read back. The signal chain
  runs on the CPU in double, a step per transmitter sample: it is a sequential recurrence (the
  amplifier, the lamp's state, the wheel) and needs double for the wheel. The screen is a GPU
  pass that integrates every scan in closed form.
- **Units: one hole.** r_c = 7N²/(6π), r_k = r_c + (N−1)/2 − k, Φ = 2π/N, a hole a wedge 1 deep
  and 1/r_k wide. The Baird frame fits the whole sector to the host's height:
  S = H / (2 (r_c + N/2) sin(Φ/2)) pixels a hole; Fill stretches N × 7N/3 to the frame.
- **M = 4 ⌈7N/3⌉ samples a line** (four a picture element, chosen); the sync is the last
  element, 4 samples, black.
- **The amplifier is exact for the held input** (y ← x + (y − x) e^{−Δ/τ}) and the lamp sees
  each sample's exact average response.
- **The lamp**: I = I₀ + 10 mA (2y − 1); light ∝ I while lit; out when I ≤ 0; an out lamp
  accumulates Δ/τ_f(ΔV) while I > 3 mA and strikes when the sum reaches 1, **lit for the exact
  share of the striking sample** (1 − need/got: a closed-form crossing, not the sample
  boundary); below 5 mA the glow covers I/5 of the plate, a centred rectangle of the field's
  proportions, at the light per unit area it has at 5 mA. **It starts lit if I₀ alone is over
  3 mA** (below).
- **The wheel**: the swing equation, semi-implicit Euler per sample; C per line is the mean of
  lit × I² over the picture less that over the sync (the coils carry nothing while the lamp
  is out).
- **How a frame integrates the scan.** The screen is the exponentially weighted exposure,
  A ← A e^{−Δt/τ} + the frame's light. The frame's light at a pixel is summed over every band
  its footprint overlaps radially, every picture m whose scan of that band falls in the frame
  (up to nine), and every sample of that scan: the overlap of the hole's angular sweep over
  the sample with the pixel's angular window is the closed-form integral of a trapezoid
  (`swept`), times the radial overlap, times the sample's light. Each sample's light carries
  its persistence weight at its own time, computed on the CPU, so a frame longer than a
  picture holds every scan, each decayed as it should be. Nothing is point-sampled in time.
- **The display**: A × π r_c / (τ L_ref), L_ref = 20 mA, so a lamp held at 20 mA averages 0.5
  linear at the centre radius. `--colour` measures it.
- **Output alpha = mix(α, 1, Mix)**: the receiver paints the whole frame. The clip's light is
  weighted by α (straight alpha: Arena's DXV clips decode straight, as intensifier measured).
- **The clock, and why it cannot overflow.** The host's time is read in double (ferrofluid's
  seconds/milliseconds vote, fountain's offline fallback); Δt is the difference of two whole
  microsecond counts, clamped to [0, 0.1 s]; a paused host runs the last non-zero interval.
  The transmitter's samples are counted in int64 since the start, as floor(µs × N M f / 1e6)
  of the total, so how the host cuts time into frames cannot change the count, and an int64 of
  microseconds lasts 292,000 years. The receiver's phase at the frame's first sample is
  reduced to one picture in double; each sample's phase goes to the GPU as a float plus its
  float remainder, so the shader works in effective double near the hole it is integrating.
  ψ is reduced to one revolution each frame. No absolute time reaches a shader, so a clock at
  4.99e8 ms (where a float resolves 0.03 s) gives the same pictures as one from 0, bit for bit.
- **Nothing is random**: no noise, no PCG in the plugin.
- **Defaults from Resolume's demo clips** (`tools/clips.py defaults`, all 33 DXV clips in
  `media/AV` and `media/Shop74`, through `--pipe` at 640×360, frames 5..29, Sync Locked):
  Lamp **10.1 mA**, the median clip's lowest standing current lighting 99% of its picture's
  samples (9.92 to 10.001 mA over the 33, the median 10.001: every clip's strip has at least
  1% black, and black is lit only above 10 mA), up to the next 0.1 mA; the reference pull
  C_ref **0.9 mA²**, the median clip's mean C at that Lamp (0.00 to 29.3 mA²; the Baird frame's
  strip is black in eight clips; Ethnik2 the median at 0.91). The rest of the defaults are the
  standard (30 lines, 12½ pictures, the drum, 9 kHz) or chosen (Speed +0.5%, Persistence 0.1 s,
  which flickers past WCAG's red-flash line: below).
- **No OpenFX port and no presets.** CI and release workflows (xenon's) since the repo went
  public for v0.1.0.

## Flashing: what crosses WCAG's lines, and why the default stays

Measured 2026-11-18 with `nktest --pipe` (640×360, 60 fps, 6 s) per pixel: each pixel's own
red-flash transitions (WCAG 2.x: 320 × max(0, R − G − B) in linear light moving by more than
20, a saturated red at the brighter end) and general ones (relative luminance 0.1, the darker
end below 0.8), then the share of the worst ten-degree window (a third of the frame each way,
stepped by a sixth) flashing more than three times in the same one-second span. At the default
0.1 s every lit pixel of the field crosses the red threshold 12½ times a second: **69.8%** of
the worst window on white, **63.9%** on NoHopeJustFear_44 (the brightest demo clip), 62.1% on
Metalive 01, 49.8% on the card, 38.6% on IntoTheGlow_02, 32.8% on a flat 25% grey. The fleet's
window-mean measure (flashcheck.py's, the one a video is cut to) averages the rolling scan out
and reads 3.0 at most at the defaults; Frame Fill (13.0 on white), Lamp 15–30 mA (6.5–13.0),
Picture Rate 5 and 8 (5.5, 8.5), Persistence 0.02–0.05 s (13.0) and Sync Free (3.5) cross it
too. The swing is about R × (1 − e^(−T/τ)): white needs τ ≳ 0.55 s at 12½ pictures to stay
under (0.5 fails, 0.6 passes), the brightest clip τ ≳ 0.2 s (0.15 fails); at τ = 1 s nothing
crosses at any Lamp (30 mA on white 14.7%) except Picture Rate 4–7 on white (47–70%).

**The release keeps the 0.1 s default and warns.** The release agent first moved the default to
1 s (066bd34) and reverted it the same morning on Allan's instruction: "flashes are fine, but
let's put a warning in the videos and descriptions". So the README opens with a
photosensitivity warning and a Flashing table, and so must the guide, the demo, the video, the
reel and their copy. Don't change a model or a default for flash safety.

## Changes from the spec's first draft

Each was made in the spec as well.

- **The lamp starts lit** when the standing current alone is over the strike current. With
  it starting out, a dark clip at a low Lamp never struck, and the defaults' bisection measured
  nothing.
- **Lamp 10.1 mA and C_ref 0.9 mA²** were "from the demo clips"; now measured (above).
- **`--spiral`** lights alternate lines inset to whole pixels of their bands, not a white
  field (trap 1), and takes the scanning hole's angle from its light-weighted centroid against
  the harness's model of the partly lit line.
- **`--slip`** reads the sync's band as the light it removes against a reference run whose
  sync samples carry the picture (`ChainHooks::noSync`), in frames one picture long, with the
  framing set so the band travels about the lines' middle: the first version lost the band
  where it wrapped at a line's end.
- **`--arc`** measures the displacement of a dot's centroid between two places 36 holes apart,
  not the arc's second moment less the closed-form moments of the aperture, hole, amplifier
  and pixel: those are the same at both places and cancel, which no deconvolution can promise.
- **`--dwell`** takes each line's length from the share of light in the pixels the field's
  edge cuts, and gained the field's total light.
- **`--bandwidth`** puts the amplifier's level and the persistence weight of the withheld
  light into the expectation (both were first left out and showed as 1e-4 errors).
- **`--lamp`**'s strike runs at 120 lines (a sample 0.6 µs, the lag 50 to 160 samples) in
  frames one picture long: at 30 lines and 30 fps the strike fell across frame boundaries and
  the window was too short.
- **`--rate`** gained an absolute anchor (the samples run against t N M f; mutant 30) and runs
  on a grey 0.5 mA over the strike at 9 mA, so the sync puts the lamp out every line and about
  one frame boundary in ten falls inside a formative lag (the strike-reset negative passed at
  720p on the first grey).
- **`--colour`** gained the display's scale (no check covered it; it has its own negative).
- **`--alpha`** gained the weighting: a grey under α against its light × α sent opaque
  (mutant 2).
- **`--resize`** runs on a flat grey, its wheel tolerance derived from the runs' own pulls and
  swing (trap 5), and gained two negatives (the wheel restarted, the lamp put out).
- **`--geometry`** gained the chain on a synthetic table: the amplifier's step, the lamp out
  on black, and the strike within its sample to 1 ns (mutant 37).
- **`nktest --set`** (and cues) refuse a number the host could not send, as well as what does
  not parse.

## The traps actually hit

1. **A partly covered pixel spreads its light over the whole pixel.** A white stripe meant for
   one line, cut at the band's edge, put light into pixels the next line's centroid counts:
   the radius came out 0.3 px off. Stripes are inset to whole pixels (`drumPixels`).
2. **Float in the exposure pass.** At r ≈ 348 holes a float resolves 3e-5, so r + hr less
   r − hr rounds the same way at every pixel and the radial overlap took a pattern: radii are
   taken relative to the hole's (`rel = r − rk`). The angular window is taken relative to the
   pixel's angle for the same reason (φ ≈ 0.1 rounds to 7e-9, 5e-5 of the window). And u ≈ 30
   lines in float drops 2e-6 of a line, 5e-4 of a sample: each sample's u goes up as a float
   and its float remainder (`uRest`).
3. **Apple's compiler reassociated the remainder away**: (u − start) + rest became
   (u + rest) − start, and the remainder vanished in the first addition. `precise` on those
   lines stops it (GLSL 4.00+ requires `precise` to be honoured; verify's reserved-word grep
   allows it).
4. **Apple's software renderer's atan is off by up to 6e-6 rad**, 3e-5 of the field's
   half-angle (measured by a probe against double). The exposure pass takes the angle from its
   own half-angle series (`angleOf`, error 1e-9 over the widest field); the GPU's atan was fine.
   **Its sin is worse: off by up to 1.1e-3 for |x| ≤ 0.4** (the GPU's 6e-8; probed the same
   way). It was found because the software renderer's disc arcs at 320×180 passed at +1.105%
   and +0.941% against tolerances of 1.227% and 1.219%, where the GPU read +0.001% and −0.134%:
   a pass that nearly fills its tolerance on one renderer is a finding, not a pass. The table
   pass now takes its sines from a series (`sine`, to x⁷, 6e-10 over the widest field), and the
   software renderer reads the GPU's arcs to the last digit printed, at both rasters. exp, log
   and pow probed fine on both (2.8e-7 at worst).
5. **The wheel can be parametrically excited.** With C_ref measured at 0.9 mA², the test
   card's lines pull from 4 to 37 mA², repeating at the picture rate; at Lamp 9 mA the wheel's
   frequency, 1.5 Hz × √(C/C_ref) ≈ 6.5 Hz, sits near half the picture rate, and it never
   settles. Two runs whose tables differ only in float rounding (one at 1280×720, one at
   320×180) parted from 1e-9 to 3e-3 rev in 75 frames, and `--resize`'s fixed 1e-7 rev
   tolerance failed. It now runs on a flat grey and derives its bound from the measured pulls.
6. **The plugin's first frame runs the default 1/60 s** (there is no previous host time), so
   after F frames the plugin has run 1/60 + (F − 1)/fps: `--rate`'s shared times are at frame
   lround(t fps) + 1.
7. **A negative control undetected at one raster**: the strike forgotten at every frame passed
   `--rate` at 720p on a grey far over the strike (a strike reset only near the boundary
   barely moved the light). Fixed by the grey near the strike (above).
8. **The first mutation run left four survivors, each a gap**: the clip's light divided by α
   (`--alpha` compared only the two sides of α = 0; partial α was never measured); the samples
   due ten times too many (`--rate`'s rates all share the error, so its comparisons passed by
   construction); the striking sample's lit share (a sub-sample effect under `--lamp`'s
   two-sample tolerance); and the plate a fifth too wide, aimed at `--slip`, which does not
   see it (`--dwell`'s field total does).
9. **std::max swallows NaN.** Every worst-case accumulation goes through `worse()`.
10. **This sandbox**: heredocs hang unless `TMPDIR` points somewhere writable (the scripts take
    none: mutate.sh hands Python its edit on the command line); `head`, `cc` and `grep` are
    shadowed in the interactive zsh (scripts run under bash, where they are not); zsh has no
    PIPESTATUS and macOS has no `timeout` (the pipe checks run under bash, the mutants under
    perl's alarm); ffmpeg reads the terminal unless `-nostdin`; a C++ probe linked with
    `/usr/bin/cc` needs `-lc++`.
11. **The FFGL SDK warns** (FFGLUtilities.cpp); verify.sh ignores warnings under external/ffgl
    only.

## Verified, and assumed

Verified, by `OXBOW=... tools/verify.sh` on 16 November 2026 (numbers in README's Status):
every check at 1280×720 and 320×180 on this Mac's GPU and again on Apple's software renderer
at 320×180 (and `--arc` there at 1280×720, in mutate.sh's run of the unmutated copy); the
alpha decision on two of Arena's clips; the pipe; the 24 negative controls and 51 mutants; the sweep, with and without context; the bundle universal, signed ad hoc, read and
rendered by oxbow.

The claims and their tolerances (each from first principles, never fitted to this Mac):

- **`--spiral`**: half a pixel. A pixel holds its footprint's average, so its light sits within
  half a pixel of its centre, and so does a light-weighted centroid over pixels that tile the
  region with the neighbours dark.
- **`--arc`**: half a pixel along the arc plus an eighth of a table cell (the table
  point-samples a trapezoid; a kink inside a cell moves the midpoint sum's centroid by at most
  cell/16), at each end, over the 36 holes moved.
- **`--dwell`**: the mean 1e-5 (a pixel sums ~8 sample terms of ~20 float operations, 6e-8
  each); the length a thousandth of a pixel at each end; the field's light 1e-5.
- **`--slip`**: half a pixel's angle at the innermost line, at each end, over the drift.
- **`--bandwidth`**: 2e-4: a pixel's 1e-5 over a window holding 17 times the edge's movement.
- **`--lamp`**: the ratio 2e-5 (two sums at 1e-5); the strike two samples (the lag runs on
  each sample's average current; the measurement's lattice); the glow's extents the lattice's
  pixel²/12 at each edge, as a share of the glow's variance, plus 1e-4; its total 1e-4.
- **`--hunting`**: frequency 1.5e-3 and decay 1e-2 (the linearisation's amplitude error and
  the fit over ~34 crossings); the lock's offset 2e-2.
- **`--flicker`**: 1e-5 (float, as `--dwell`).
- **`--rate`**: the chain's integers, amplifier and lamp bit-exact (the same arithmetic in the
  same order); ψ 1e-9 rev (2e5 additions at ulp(2), ×25); the exposure 1.3e-4 (480 frames'
  decay and sum at two float ulps each, plus a scan split between frames at 1e-5); the
  samples run within the one under way.
- **`--colour`**: G/R 1e-4 (the float sRGB curve out and back, 1e-6, with margin); the
  display's average twice the Koksma bound (the swing seen, doubled, times the phases'
  discrepancy on the circle) plus 1e-4.
- **`--alpha`**: 1e-6 and bit-exact; the weighting 1e-5 of the brightest pixel (the sRGB curve
  in float holds the light to 1e-6; the current carries it with gain under 2).
- **`--resize`**: the light 1e-4 (area resampling conserves it; each raster's light to 1e-5);
  the wheel 2 A δ (ωt/2 + 1), A the swing and δ the runs' pull difference, both read off the
  runs; the lamp and samples exact.
- **`--clock`, `--glstate`**: exact.
- **`--geometry`**: conversions 1e-6 (float), the disc 1e-12, the amplifier's step 1e-5, the
  lamp on black one sample, the strike within its sample 1e-9 s (the light stored in float).

Assumed or chosen, not measured: the single-pole amplifier; R = 10 kΩ (I_strike 3 mA); the
formative lag from indicator lamps; τ_m = 1 s and f_h = 1.5 Hz; the glow's centred rectangle;
the neon's chromaticity (a spectral lamp's) and its mapping into sRGB; M = 4 samples an
element; Persistence 0.1 s (which flickers: above); Speed +0.5%.

Not done: loading it into Resolume on any platform; a Windows build; a user guide, presets,
an OpenFX port; looking at the demo clips in a show rather than as numbers.

## Would this hold on another rasteriser, at another raster?

One line per check. Apple's software renderer at 320×180 passes every one of them.

- `--geometry`: no GL; double, and the light's float storage.
- `--spiral`: the tolerance is the raster's own half pixel and the stripes its whole pixels;
  holds wherever the exposure pass's `precise` is honoured (it is in GLSL 4.00).
- `--arc`: in pixels and table cells, scaled to the raster (0.36% at 720p, 1.2% at 180p). It
  held on the software renderer only once the table pass stopped calling its sin (trap 4);
  a GPU whose sin is as loose would fail it at 720p (mutant 51 is that case).
- `--dwell`: 1e-5 is float arithmetic per pixel, raster-free; the length's thousandth of a
  pixel is the raster's.
- `--slip`: half a pixel at the raster checked.
- `--bandwidth`: per-pixel float error over a window, raster-free.
- `--lamp`: two samples, independent of the raster; at 320×180 the measurement uses 0.29 µs of
  1.19.
- `--hunting`: a flat grey at 64×36; the chain is CPU double and the table of a flat field is
  the same at any raster.
- `--flicker`: per-pixel float.
- `--rate`: the chain is CPU double with no fast-math; the exposure's float budget holds on any
  IEEE float GPU.
- `--colour`: float curve; the average's bound is computed from the phases the run had.
- `--alpha`: compares the plugin with itself on one driver.
- `--resize`: only 1280×720 → 320×180; its wheel bound is read off the run.
- `--clock`: the plugin against itself under two clocks in one context.
- `--glstate`: whatever the driver reports.

## The negative controls and mutants

`nktest --negative`, each wrong model required to fail (checks failed of those run), all 24
caught:

| check | wrong model | failed |
| --- | --- | --- |
| geometry | τ = 1/B, not 1/(2πB) | 2 |
| geometry | an instant strike | 2 |
| spiral | holes on a spiral 2% too steep | 2 |
| spiral | every hole on one circle | 2 |
| slip | the receiver's phase advanced by ψ, not Nψ | 2 |
| arc | the drum's lines explored along the disc's arcs | 4 |
| dwell | every hole the same angular width (no 1/r) | 2 |
| bandwidth | τ = 1/B | 2 |
| lamp | an instant strike | 2 |
| lamp | light proportional to the current squared | 4 |
| hunting | the pull not scaled by C | 4 |
| hunting | the motor's damping of the wrong sign | 4 |
| flicker | at most one scan of a band a frame | 2 |
| rate | the disc advanced by 1/60 s a frame, whatever the frame | 2 |
| rate | the lamp's strike forgotten at every frame | 2 |
| colour | the lamp drawn in white | 2 |
| colour | the display scaled without its π | 2 |
| alpha | the clip's alpha passed through | 2 |
| alpha | the clip's light not weighted by its alpha | 2 |
| resize | a new raster clears the exposure | 1 |
| resize | a new raster restarts the wheel at the framing | 1 |
| resize | a new raster puts the lamp out | 1 |
| clock | the host's clock read in single precision | 6 |
| glstate | the host's GL state not put back | 2 |

`tools/mutate.sh`, 51 one-character mutants, each built arm64 and run at 320×180 under a 600 s
alarm, all caught. One runs on Apple's software renderer at 1280×720 (`sw:--arc@1280x720`):
the disc's height through the built-in sin again (`sine(` → `sin (`) reads the arcs +1.108% and
+1.106% long against a 0.37% tolerance. On the GPU, whose sin is good, the same mutant would
rightly pass. Tried and not shipped: the series' cube twice too strong (`q / 6.0` → `q / 3.0`)
passes `--spiral` at 320×180, the check it was aimed at. It moves a sample at most 0.07 holes at
the edge of the 30-line field (0.16 px at 320×180; 0.28 holes at 8 lines), and was not tried
against the other checks: the series' coefficients rest on the probe, not on the harness. GLSL: the luminance weights summing to 1.1, the clip decoded at gamma 2.5,
the glow's extent from the light (`--lamp`); the light divided by α, the screen transparent
(`--alpha`); line k exploring line k + 1, the aperture a hole wide on one side, the pixel's
angle ×1.5, every hole half a hole in, hole k a line late (`--spiral`); the drum's line 8N/3
long, the disc's arc bowed three times (`--arc`); the entry ramp three times steep, the ramp's
integral past the hole, the window's near edge a hole off, the footprint's width and its
half-angle, the plate a fifth wide, the hole two lines deep and three times wide, the radial
overlap summed, the pixel's area 5 hr hφ (`--dwell`); the screen forgetting each frame
(`--flicker`); the resample's overlap a pixel wide (`--resize`); the sRGB encode's offset
(`--colour`). C++: the centre radius 8N²/6π, the Baird frame two thirds high, the field off
centre (`--spiral`); the lag's exponent, the overvoltage ×10, the signal's swing ×1.5, the glow's
covering current inverted (`--lamp`); the striking sample's lit share (`--geometry`); the
XYZ-to-sRGB green, the display divided by r_c (`--colour`); the samples due ×10, the host's
time a tenth (`--rate`); the screen's decay growing, the persistence weights' step
(`--flicker`); the amplifier's τ 1/3πB, the sample's average response, a geometric control's
range high × low (`--bandwidth`); the wheel's stiffness, the pull from the lit share alone,
the sync's current added to the pull, the damping driving the swing, the teeth 1.5 a picture
(`--hunting`); the receiver's phase a line a sample (`--dwell`); a linear control's range
high + low (`--slip`); the clock-unit fallback upside down (`--clock`).

## The sweep's honest list

`python3 tools/sweep.py`: all 11 controls change the picture (120 frames at 320×180 on the
card), each in a context where it acts; only Speed has one (Sync Free). `--bare` (the context
table emptied) also finds nothing dead: under the default Hunting, Speed's two ends (±3%) move
the wheel's lock, or slip it, enough to change the picture (mean 8-bit delta 2.62, the same as
with Sync Free). **Speed is dead under Sync Locked, by design** (the ideal synchroniser ignores
the motor), and Framing does nothing under Sync Free beyond where the drift starts. Scanner is
the weakest at 0.51 (the drum and the disc differ by the bow of the lines, under 2 line
pitches), Persistence next at 0.78.

## The browser demo (2026-11-18)

https://nipkow-demo.stoatworks-labs.com/, from `demo/`, deployed by `wrangler.toml` (a Worker
route: the zone is at Cloudflare's 100 custom domains, so `nipkow-demo` is a proxied AAAA
`100::` record made through the API) and `.github/workflows/deploy.yml` on every push to main.

- **It runs the plugin, it doesn't port it.** `demo/nipkow-core.wasm` is `Nipkow.cpp` and
  everything it calls (Controls, Apparatus, Shaders, PassBuffer, Diag) plus the FFGL SDK's
  plugin classes, FFGLShader and FFGLFBO, compiled unmodified by emscripten
  (`demo/tools/build-wasm.sh`, fountain's pattern). Left out: `EffectPlugin.cpp`.
- **What is the page's**: `demo/wasm/glue.cpp` plays the host (constructs the plugin, reads its
  declarations back through the SDK, forwards SetFloatParameter, SetTime and ProcessOpenGL as
  nktest's Rig does, and reads the state `--stats` prints for the line under the picture);
  `demo/wasm/gl_shim.cpp` takes six GL calls: glShaderSource (below); glEnable, glDisable and
  glIsEnabled for GL_PROGRAM_POINT_SIZE, which WebGL2 lacks; glTexImage2D, because the SDK
  allocates R32F with GL_RGBA and no data and WebGL2 takes only GL_RED for R32F; glReadPixels,
  because the table's GL_RED/GL_FLOAT read-back may not be the browser's read format (it then
  reads RGBA/FLOAT and copies red, the same values). Chrome on this Mac, GPU and SwiftShader,
  reads RED/FLOAT; the RGBA path was run by forcing it. All of it needs EXT_color_buffer_float;
  without it the page stops and says why (no 8-bit path).
- **The ES port is a checked, mechanical transform, at load time.** glShaderSource refuses any
  text that isn't byte for byte the page's assembly of `demo/shaders.js`, then applies the
  demo kit's `port()` and checks its output: `#version 410 core` (and the blank line after it,
  which port()'s pattern takes) becomes `#version 300 es`, the ES highp precision defaults go
  in front, and `precise ` comes off exactly the declarations `PRECISE` lists (rel, rlo, rhi,
  da, db, ua, ub). Anything else refuses the stage. `demo/tools/check_shaders.py` (in
  `tools/verify.sh`) checks the same on every stage at verify time, against a Python mirror of
  port() pinned by the hash of port()'s text in the vendored `gl.js`.
- **What dropping `precise` may cost** (traps 2 and 3): a compiler that reassociates
  `rlo = max( rel - hr, -0.5 )` back into terms of r rounds the radial overlap at r ≈ 350
  holes, 3e-5 of a hole, against an overlap of 2hr (about 0.1 hole at 720p, 0.07 at 1080p):
  under 1e-3 of a pixel's light; and one that turns (u − start) + rest into (u + rest) − start
  loses the remainder, 2e-6 of a line, 5e-4 of a sample. Both under one 8-bit step by that
  arithmetic; neither measured. A new `precise` in Shaders.cpp fails check_shaders.py until it
  is added to PRECISE, here and to the page's list.
- **Flash safety, the page's side.** The warning is the first thing on the page (index.html),
  in the README's words. The page starts at the defaults: a URL that carries any control away
  from its default (Copy link writes one) mounts nothing until the visitor chooses "Open at the
  defaults" (the settings are stripped from the URL) or "Open with the link's settings". Embeds
  are gated the same way, deliberately: a link that flickers on opening is worse than a click
  in an OBS source. No control is clamped. A caution line under the picture names a setting in
  or beside the README's flickering range (Persistence under 0.6 s, Picture Rate 3 to 8, Lamp
  over 20 mA, Frame Fill with any of them); it reads the table, it measures nothing. Clips: the
  dusk scene first, and not the 100% bars or the alpha clip.
- **Checked on the page** (headless Chrome, GPU and SwiftShader, 960x540): all eight stages
  accepted, `precise` off the seven, the read-outs in the plugin's units, Lines 30 → 60
  changes the picture, the gate (shown, nothing drawn, both answers), the caution, the RGBA
  read-back path. Chrome logs "GPU stall due to ReadPixels" warnings: that is the table's
  per-frame read-back, which is the plugin's design. Not compared with nktest frame for frame.
- **Rebuild after any change under `source/`**, the generated `StoatworksAbout*.h` included:
  `demo/tools/build-wasm.sh`, then commit both outputs and `demo/wasm/inputs.sha256`;
  verify fails until you do.

## Open questions

- **The wheel's pull grows as the lamp's current squared, without limit.** C_ref, measured on
  the mostly dark demo clips, is 0.9 mA²; a bright, varied picture pulls 20 to 40 times that,
  and its hunting frequency then sits near half the picture rate, where the picture's own pull
  pattern excites it (trap 5). At the defaults on the harness's card it locks, but a brighter
  Lamp with some detuning keeps it swinging. Saturate the pull (the pole pieces would), take
  C_ref from brighter content, or keep it as the model says?
- **The default Lamp sits on a knife edge**: at 10.1 mA black is 0.1 mA, just lit. Turn it
  down a little and every black part of the picture puts the lamp out and strikes it again a
  lag later (the most period-looking part of the model). Measured, not looked at in a show:
  is 10.1 the right default, or should it sit just under 10?
- **The IDEAS bullet** says the picture is "slightly wider at one end" and slips "sideways".
  The sources say it is taller and dimmer on the outer side (with a drum transmitter) and
  drifts vertically, stepping one line sideways a slipped tooth. IDEAS.md was read-only to
  this build; correcting it is yours.
- **Straight or premultiplied alpha in Resolume's pipeline?** As intensifier's question.
- **The glow's place on the plate** (centred) has no source; nor the amplifier's single pole,
  R = 10 kΩ, the wheel's 1.5 Hz and 1 s.
- **Fill** stretches the field to the frame (the sector's outer corners overhang by up to 4%);
  is it wanted at all, or should Baird be the only frame?
