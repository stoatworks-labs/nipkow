# nipkow

Baird's 30-line mechanical television looking at the clip, for Resolume Arena/Avenue as one
FFGL effect. A mirror drum (or a disc) explores the clip in 30 strips, 12½ pictures a second;
the signal goes through a one-pole amplifier into a neon lamp that strikes after its formative
lag and glows over only part of its plate when starved; a 30-hole disc on a spiral turns in
front of it, each hole tracing one arc; a phonic wheel whose coils carry the lamp's current
holds the disc in step, and hunts. The screen is the exact exponential exposure of every scan
of every point. C++/GLSL, CMake MODULE → universal `.bundle` (macOS). MIT.

| id | display name | bundle | type |
| --- | --- | --- | --- |
| `NK01` | `SW Nipkow` | `Nipkow.bundle`, `com.stoatworks.ffgl.nipkow` | effect |

Read `AGENTS.md` before touching the exposure pass (`kExposureFragment` in `Shaders.cpp`: its
float precision is load-bearing), the table pass, the signal chain (`Apparatus.cpp`: the
amplifier, the lamp, the wheel), the clock, the resize or the composite. The spec is
`~/Projects/resolume/specs/SPEC-nipkow.md` and is kept in step with this repo.

## Commands (CMake)
- Configure: `cmake -B build -DCMAKE_BUILD_TYPE=Release`
- Fast dev build: add `-DCMAKE_OSX_ARCHITECTURES=arm64`
- Build: `cmake --build build -j8`
- Install to Resolume: `cmake --install build` (into Arena's Extra Effects).
  Never from a shared session on this Mac: nothing here has been loaded into Arena.
- Render offline: `./build/nktest --out /tmp/n.png` (the defaults over the harness's card,
  120 frames in at 60 fps)
- A clip of your own: `--clip clip.rgba` (raw RGBA at `--size`, top row first)
- List parameters: `./build/nktest --list`
- Set anything by name: `./build/nktest --set "Lamp=0.5" --set "Scanner=Disc" --set "Lines=60"`
  (a standard control takes the host's 0..1, Lines its integer, an option its name or index;
  anything unparseable, or a number the host could not send, is refused with exit 2)
- Change something later: `./build/nktest --out /tmp/n.png --at 60 "Framing=0.7"`
- Film: `./build/nktest --film 600 --size 1280x720 --script cues | ffmpeg -f rawvideo -pix_fmt rgba -s 1280x720 -r 60 -i - out.mp4`
- A clip through it: `ffmpeg -nostdin -i in.mov -f rawvideo -pix_fmt rgba - | ./build/nktest --pipe --fps 30 --size WxH | ffmpeg -f rawvideo -pix_fmt rgba -s WxH -r 30 -i - out.mp4`
- `--stats` prints a line a frame on stderr: the lamp's lit share of the picture's samples,
  its last current, the mean pull C of the lines the frame ended, ψ, ψ′ and the strikes.
- The defaults' evidence: `python3 tools/clips.py defaults` (all 33 of Arena's demo clips;
  slow). The alpha check on two of them: `python3 tools/clips.py alpha`.

## Verify
- Everything: `OXBOW=/path/to/oxbow tools/verify.sh` (GLSL reserved words and `M_PI`, `glslc`
  on all 5 shaders (required), the literals under 15 KB, the submodule pin, a fresh universal
  build with no warnings outside external/ffgl, lipo, plist, ad-hoc signature, `oxbow probe`
  and `selftest` (required: no oxbow is a failure); every check at 1280x720 and 320x180;
  every check again on Apple's software renderer at 320x180; `--offline`; the alpha check on
  Arena's clips; `--pipe`; the negative controls; the mutants; the sweep; the bench).
  `KEEP_BUILD=1` reuses `build-verify/`. It needs Resolume Arena's demo clips in
  `/Applications/Resolume Arena/media`. Run it with bash (its shebang): the pipe checks read
  PIPESTATUS.
- Every check at once: `./build/nktest --all`. One at a time (`--size WxH`: that raster alone):
  - **The disc**: `--spiral` (each line's radius and the scanning hole's angle off the
    picture), `--arc` (a dot's arc: h r_k / r_c with the drum, h with the disc), `--dwell`
    (brightness ∝ 1/r_k, each line r_k Φ long, the field's light), `--slip` (the picture
    drifts N × Speed × f lines a second, Sync Free).
  - **The signal**: `--bandwidth` (an edge moves ω(τ₁ − τ₂) along every line), `--lamp`
    (light ∝ current; the strike after the formative lag; a starved glow's area ∝ current),
    `--hunting` (the wheel's swing, its decay, the lock's offset, pull-out), `--flicker` (a
    point's exposure against the closed form at 60, 24 and 8 fps), `--colour` (the neon's
    proportions; a lamp held at 20 mA averages 0.5).
  - **The host**: `--rate` (24–240 fps share the chain's state and the picture; the clock is
    N M f samples a second), `--alpha`, `--resize` (mid-run: the wheel, the lamp, the
    exposure), `--clock` (4.99e8 ms against 0, bit for bit; the unit vote), `--glstate`.
  - **No GL**: `--geometry` (conversions, the disc, the lag, the colour, the pull-out; the
    chain on a synthetic table: the amplifier's step, the strike to 1 ns).
- The checks can fail: `./build/nktest --negative` (24 wrong models), `tools/mutate.sh`
  (51 mutants, arm64, 320x180 unless a mutant names its raster, the GPU unless it names the
  software renderer (`sw:`), perl alarm at 600 s; `ONLY=n` runs one).
- No dead controls: `python3 tools/sweep.py` (each control in a context where it acts;
  `--bare` drops the context).
- Render cost: `./build/nktest --bench` (720p, 1080p, one 4K run).
- `NKTEST_RENDERER=software` runs any of it on Apple's software renderer.
- Shared Mac: arm64 builds while iterating, no 4K loops, never kill a process you didn't start.
- In this sandbox, heredocs hang unless `TMPDIR` points somewhere writable; the scripts take
  no heredoc of their own.

## Notes
- **Four fragment passes and one shared full-raster triangle** (`Shaders.h`, five shaders for
  `glslc`): table (the transmitter: N × M samples of the clip's linear luminance × α over the
  square aperture, read back), exposure (the screen: every scan of every pixel in the frame,
  in closed form, plus the previous exposure decayed), resample (the exposure to a new raster,
  on a resize only), composite (the display scale, the neon's colour, sRGB, Mix, alpha).
  Nothing is blended.
- **The signal chain runs on the CPU in double** (`Apparatus.cpp`), a step per transmitter
  sample (~10 µs at the standard): the amplifier exact for the held input, the lamp's state
  and formative lag, the glow, the coils' pull per line, the phonic wheel (semi-implicit
  Euler). It hands the exposure pass a sample table (u, light × weight, glow, u's remainder).
- **No time reaches a shader.** The CPU keeps the host's clock in double, rounds Δt to the
  microsecond, and counts transmitter samples since the start in integers; phases go to the
  GPU relative to the frame, as two floats.
- **Nothing is random.** No noise, no PCG in the plugin (the harness's card uses one).
- All host parameters are 0..1 and mapped in `Controls.cpp`, except Lines (an FFGL integer,
  8..120); options hold the element index.
- GLSL 4.10 core. Reserved words (`common partition active … packed sample patch layout flat`,
  `noise1`–`noise4`, `any far near fixed`) must not be identifiers, and nothing writes `M_PI`;
  `verify.sh` greps for both. `precise` is used, deliberately (AGENTS.md).
- Override `SetTextParameter` to return `FF_SUCCESS` for the About block, or no host can
  instantiate the plugin.
- `nipkow_core` is an OBJECT library: the plugin registers itself from a file-scope
  constructor nothing references.
- The macOS build must be universal. Check with `lipo`, never the build log.
- **Provisional hand copies:** `source/StoatworksAbout*.h` (guide = "", no user guide) and
  `ATTRIBUTIONS.md`. Once the project is registered in stoatworks-backend, `sync-about.py`
  and `sync-attributions.py` generate them; edit them there then.
- **Workflows** (the repo is public, `stoatworks-labs/nipkow`): `ci.yml` builds on macOS
  (arm64, `nktest --list` and `--offline`, glslc) and Windows on every push and pull request;
  `release.yml` builds the universal bundle and the x64 DLL (checked by `tools/wincheck.ps1`,
  whose `-Params` is the control table plus the About block and changes with either) on a
  `v*` tag or a dispatch, and publishes only on a tag. Neither has a GPU: `tools/verify.sh`
  is still the verification.
- `~/Projects/resolume/nipkow`, moved from `~/dev/nipkow` for the v0.1.0 release
  (18 November 2026). **The default Persistence (0.1 s) flickers past WCAG's red-flash line**
  on bright clips, on purpose: README's warning and Flashing table say so, and the guide, demo
  and videos carry the warning. Never change a model or a default for flash safety (Allan,
  2026-11-18); if a change moves the flicker, re-measure it and update the table.

## Browser demo
- `demo/` is served at https://nipkow-demo.stoatworks-labs.com/ by the Worker in
  `wrangler.toml` (a route, plus a proxied AAAA `100::` record for `nipkow-demo`: delete the
  record and the page goes dark while deploys stay green). `.github/workflows/deploy.yml`
  redeploys it on every push to main; by hand: `cf-run npx wrangler deploy`.
- **`demo/nipkow-core.{js,wasm}` is the plugin's C++, compiled and committed.** After ANY
  change under `source/` (the generated `StoatworksAbout*.h` included), run
  `demo/tools/build-wasm.sh` (emscripten's `em++`) and commit both outputs and
  `demo/wasm/inputs.sha256`; `tools/verify.sh` fails until you do.
- `demo/shaders.js` is a copy: after changing a shader, `python3 demo/tools/check_shaders.py
  --write`. The same script checks the kit's ES port (only the version line, the precisions
  and `precise` off the variables its PRECISE lists) and the .wasm's input hashes.
- The plugin flickers at its defaults, so the page's photosensitivity warning, its Start gate
  (nothing drawn before it, embeds included) and its caution line stay: don't weaken them
  (AGENTS.md, "The browser demo").
- `demo/vendor/` is the shared kit (`stoatworks-backend/resolume-demo`); never edit it.

## Diagnostics

`source/Diag.{h,cpp}`: log file only, no crash handler (this runs inside Resolume).

    ~/Library/Logs/nipkow/nipkow.YYYY-MM-DD.log
    %LOCALAPPDATA%\nipkow\logs\nipkow.YYYY-MM-DD.log

`NIPKOW_LOG_DIR` overrides the folder.
