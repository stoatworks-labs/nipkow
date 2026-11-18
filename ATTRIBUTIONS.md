# Attributions

Nipkow is built on other people's work. This file lists what that work is, who did
it, and what it is doing here.

It is generated — the master lists live in the `stoatworks-backend` repo and are
pushed out by `scripts/sync-attributions.py`. Edit it there, not here.

## Code we derived from other people's work

Someone else solved this first, and this project would not exist in its current form without their work.

### The harness and the tooling — Stoatworks intensifier, filings, fountain, and ferrofluid before them

<https://github.com/stoatworks-labs/intensifier>  
Licence: MIT  
Copyright: Stoatworks Labs

The harness's rig, PNG writer, cue sheets, --set by display name, --pipe/--film (SIGPIPE ignored, a closed stdout an exit of 1), --negative with its Perturb, and --offline (tools/nktest is intensifier's ittest, which is filings' fgtest, fountain's fttest and ferrofluid's fetest, under new checks); tools/verify.sh, tools/mutate.sh (with fountain's perl alarm), tools/glslc.sh, tools/sweep.py and tools/clips.py; the clock's vote on seconds or milliseconds (ferrofluid's, by way of rosette and millpond) with fountain's offline fallback (source/Nipkow.cpp). All adapted.

### The pass buffers, the GL state and the log — Stoatworks millpond and radar

<https://github.com/stoatworks-labs/millpond>  
Licence: MIT  
Copyright: Stoatworks Labs

source/PassBuffer.{h,cpp} and source/Diag.{h,cpp} are millpond's, by way of filings and intensifier. source/GLState.h is radar's, which is vectrix's, from resolume-scopes; nipkow adds a scope of its own in source/Nipkow.cpp for the pixel-transfer state its table read-back touches, and lets the harness's negative control skip the restore.

### The release workflow and the DLL check — Stoatworks xenon

<https://github.com/stoatworks-labs/xenon>  
Licence: MIT  
Copyright: Stoatworks Labs

.github/workflows/ci.yml and release.yml are xenon's (slitscan's before it): a universal bundle and a Windows DLL, ATTRIBUTIONS.md in each zip, the Windows installer per user into Resolume's Extra Effects. tools/wincheck.ps1 (slitscan's and isohypse's before it) loads the DLL as a host does and reads back its name, id, type and parameters. scripts/release-lib.sh is the fleet's. Adapted.

### The look card — Stoatworks schlieren

<https://github.com/stoatworks-labs/schlieren>  
Licence: MIT  
Copyright: Stoatworks Labs

The harness's test card (a sunset sky, a sun, a skyline, colour bars), the default clip in nktest.

### The browser demo's build and kit — Stoatworks fountain, schlieren and polyhedral

<https://github.com/stoatworks-labs/fountain>  
Licence: MIT  
Copyright: Stoatworks Labs

demo/tools/build-wasm.sh, which compiles the plugin's own C++ to WebAssembly unmodified, is fountain's script (polyhedral's pattern); demo/wasm/gl_shim.cpp is schlieren's shim (zoetrope's and polyhedral's before it); the page around them is the fleet's shared demo kit (demo/vendor/). Adapted.

## Third-party code this project uses

Libraries, SDKs and frameworks the project is built on or bundles.

### Resolume FFGL SDK

<https://github.com/resolume/ffgl>  
Licence: BSD-3-Clause  
Copyright: FreeFrame

Vendored as a git submodule at external/ffgl (third_party/ffgl in oxbow).

The plugin ABI itself. An FFGL effect or source is defined by this SDK's headers — there is no other way to be loadable by Resolume Arena and Avenue.

### GLEW — the OpenGL Extension Wrangler Library

<https://github.com/nigels-com/glew>  
Licence: BSD-3-Clause (with Mesa 3-D and Khronos components)  
Copyright: Milan Ikits, Marcelo E. Magallon and Lev Povalahev

Arrives inside the FFGL submodule at external/ffgl/deps/glew-2.1.0. Not fetched separately.

Resolves OpenGL entry points on Windows, where the system headers stop at OpenGL 1.1.

### libpng

<http://www.libpng.org/pub/png/libpng.html>  
Licence: PNG Reference Library License (libpng)  
Copyright: the PNG Reference Library authors

Arrives inside the FFGL submodule, under the SDK's CustomThumbnail sample.

Part of the upstream SDK tree rather than something these plugins call directly — listed because it is present in the checkout.

## Work we checked ourselves against

No code was taken from these — but they were how we knew we had it right, and that is worth saying out loud.

### Television — M. G. Scroggie (Blackie, 1935)

The 30-line standard: 30 lines, 12½ pictures a second, 375 lines a second, 750 rpm, the 7:3 picture, the bandwidth the picture needs (pp. 8, 18, 22, 51); the Lines and Picture Rate defaults. Read at archive.org (dli.ernet.8035) on 2026-11-16.

### The London Television Service — T. C. Macnamara and D. C. Birkinshaw, J. IEE 83 (1938), pp. 730–731

The BBC's mirror drum, its square aperture and the black pulse at the end of each line: the Scanner's default, the table pass and the sync. Read on 2026-11-16.

### The Baird Televisor (disc model) — Baird Television Ltd, Instructions for Connecting and Operating the Baird Televisor; H. J. Barton Chapple, Servicing the Baird Disc Model Televisor

The 20-inch disc at 750 rpm, the 9-kilocycle channel (the Bandwidth default), the 30-tooth synchroniser and its coils, the speed and framing adjustments, the picture's drift: the phonic wheel, Speed and Framing. Read at earlytelevision.org on 2026-11-16.

### The period books — F. J. Camm (1934); S. A. Moseley and H. J. Barton Chapple (1930); G. V. Dowding (ed.); R. W. Hutchinson (1937); K. A. Hathaway (1933); A. F. Collins (1932)

Newnes Television and Short-Wave Handbook, Television To-day and To-morrow, Book of Practical Television, Television Up-to-Date, Television, Experimental Television: the disc's spiral and its holes, the scan's direction, the curved lines and the sector-shaped field, the lamp's voltages, hunting. Read at archive.org on 2026-11-16.

### The neon lamp — Gray, Horton and Mathes (BSTJ 6, 1927); Raytheon (1928); General Electric (1966); Signalite; the Television Society; Meek and Craggs (1953)

Light proportional to current (BSTJ 6, pp. 568, 594); the Kino-Lamp's 5 mA covering current; the formative lag through 300 µs and 25 µs (GE Glow Lamp Manual, 2nd ed., pp. 2–5, 10) and the 10 µs floor (Signalite); the lamp's currents, striking voltage and colour (Television, 1928–1930); the statistical lag, left out (Electrical Breakdown of Gases, pp. 111–116). The colour from the Osram neon spectral lamp's chromaticity (Lamptech); the NIST Atomic Spectra Database's Ne I lines read, not used.

### The phonic wheel — P. la Cour, US Patent 203,423 (1878); Lord Rayleigh, Nature 18 (1878) p. 111, Phil. Trans. 174 (1883) and The Theory of Sound §63

The wheel's lock, its loss to a disturbance of half a tooth, its persistent oscillations and their damping: the swing equation's form.

## Standards and published specifications

What the implementation is measured against.

- **IEC 61966-2-1 (sRGB)** — The clip's decoding and the output's encoding, and the XYZ-to-linear-sRGB matrix the neon's colour goes through (source/Shaders.cpp, source/Apparatus.cpp).
- **ITU-R BT.709** — The luminance weights 0.2126, 0.7152, 0.0722 the transmitter's photocell sees the clip through.
- **CIE 1931** — The neon's chromaticity, as xy, and D65's.

## Getting this wrong

If your work is here and the description is inaccurate, the licence is wrong, or you would rather not be listed — open an issue and it will be fixed.
