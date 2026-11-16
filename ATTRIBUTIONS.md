# Attributions

Nipkow is built on other people's work. This file lists what that work is, who did it,
and what it is doing here.

**A provisional hand copy (2026-11-16).** Nipkow is not yet registered in
`stoatworks-backend`, so this was written by hand in the generated file's shape. Once it is
registered, `scripts/sync-attributions.py` generates it from the master lists; edit it there
then, not here.

## Code we derived from other people's work

Someone else solved this first, and this project would not exist in its current form without their work.

### The harness and the tooling — Stoatworks intensifier, filings, fountain, and ferrofluid before them

<https://github.com/stoatworks-labs/ferrofluid>  
Licence: MIT  
Copyright: Stoatworks Labs

The harness's rig, PNG writer, cue sheets, `--set` by display name, `--pipe`/`--film` (SIGPIPE ignored, a closed stdout an exit of 1), `--negative` with its Perturb, and `--offline` (tools/nktest is intensifier's ittest, which is filings' fgtest, fountain's fttest and ferrofluid's fetest, under new checks); tools/verify.sh, tools/mutate.sh (with fountain's perl alarm), tools/glslc.sh, tools/sweep.py and tools/clips.py; the clock's vote on seconds or milliseconds (ferrofluid's, by way of rosette and millpond) with fountain's offline fallback (source/Nipkow.cpp). All adapted. intensifier lives at ~/dev/intensifier, filings at ~/dev/filings and fountain at ~/dev/fountain; none is published yet.

### The pass buffers, the GL state and the log — Stoatworks millpond and radar

<https://github.com/stoatworks-labs/millpond>  
Licence: MIT  
Copyright: Stoatworks Labs

source/PassBuffer.{h,cpp} and source/Diag.{h,cpp} are millpond's, by way of filings and intensifier. source/GLState.h is radar's, which is vectrix's, from resolume-scopes; nipkow adds a scope of its own in source/Nipkow.cpp for the pixel-transfer state its table read-back touches, and lets the harness's negative control skip the restore.

### The About block — Stoatworks zoetrope

<https://github.com/stoatworks-labs/zoetrope>  
Licence: MIT  
Copyright: Stoatworks Labs

source/StoatworksAbout*.h, a provisional hand copy of intensifier's, itself from fountain's and zoetrope's first header (no user guide yet).

### The look card — Stoatworks schlieren

<https://github.com/stoatworks-labs/schlieren>  
Licence: MIT  
Copyright: Stoatworks Labs

The harness's test card (a sunset sky, a sun, a skyline, colour bars), the default clip in nktest.

## Third-party code this project uses

Libraries, SDKs and frameworks the project is built on or bundles.

### Resolume FFGL SDK

<https://github.com/resolume/ffgl>  
Licence: BSD-3-Clause  
Copyright: FreeFrame

Vendored as a git submodule at external/ffgl, pinned at b1afaf9.

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

Part of the upstream SDK tree rather than something this plugin calls directly — listed because it is present in the checkout.

## Work we checked ourselves against

No code was taken from these — but they were how we knew we had it right, and that is worth saying out loud. Each was read on 2026-11-16 (full text, page image or the NIST query result); the spec (`SPEC-nipkow.md`) quotes them, page by page.

### The 30-line standard — M. G. Scroggie, *Television* (Blackie, 1935)

30 lines, 12½ pictures a second, 375 lines a second, 750 rpm, the 7:3 picture, the bandwidth the picture needs (pp. 8, 18, 22, 51). Read at archive.org (`dli.ernet.8035`).

### The BBC's transmitter — T. C. Macnamara and D. C. Birkinshaw, "The London Television Service", *J. IEE* 83 (1938), pp. 730–731

The mirror drum, the square aperture, the black pulse at the end of each line (the Scanner's default, the table pass, the sync).

### The Televisor — Baird Television Ltd, *Instructions for Connecting and Operating the Baird Televisor (Disc Model)*; H. J. Barton Chapple, "Servicing the Baird Disc Model Televisor"

The 20-inch disc at 750 rpm, the 9-kilocycle channel (the Bandwidth default), the 30-tooth synchroniser and its coils, the speed and framing adjustments, the picture's drift (the phonic wheel, Speed, Framing). Read at earlytelevision.org.

### The period books

F. J. Camm, *Newnes Television and Short-Wave Handbook* (1934); S. A. Moseley and H. J. Barton Chapple, *Television To-day and To-morrow* (1930); G. V. Dowding (ed.), *Book of Practical Television*; R. W. Hutchinson, *Television Up-to-Date* (1937); K. A. Hathaway, *Television* (1933); A. F. Collins, *Experimental Television* (1932). The disc's spiral and its holes, the scan's direction, the curved lines and the sector-shaped field, the lamp's voltages, hunting. Read at archive.org.

### The neon lamp

Gray, Horton and Mathes, *Bell System Technical Journal* 6 (1927), pp. 568, 594 (light proportional to current); the Raytheon Kino-Lamp technical bulletin (1928) (the 5 mA covering current); General Electric, *Glow Lamp Manual*, 2nd ed. (1966), pp. 2–5, 10 (the formative lag through 300 µs and 25 µs); the Signalite catalogue (the 10 µs floor); *Television* (the Television Society's magazine), 1928–1930 (the lamp's currents, its striking voltage, its colour); Meek and Craggs, *Electrical Breakdown of Gases* (1953), pp. 111–116 (the statistical lag, left out). The colour: the Osram neon spectral lamp's chromaticity (Lamptech) and the NIST Atomic Spectra Database's Ne I lines (read, not used).

### The phonic wheel — P. la Cour, US Patent 203,423 (1878); Lord Rayleigh, *Nature* 18 (1878) p. 111, *Phil. Trans.* 174 (1883) and *The Theory of Sound* §63

The wheel's lock, its loss to a disturbance of half a tooth, its persistent oscillations and their damping: the swing equation's form.

## Standards and published specifications

What the implementation is measured against.

- **IEC 61966-2-1 (sRGB)** — the clip's decoding and the output's encoding, and the XYZ-to-linear-sRGB matrix the neon's colour goes through (source/Shaders.cpp, source/Apparatus.cpp).
- **ITU-R BT.709** — the luminance weights 0.2126, 0.7152, 0.0722 the transmitter's photocell sees the clip through.
- **CIE 1931** — the neon's chromaticity, as xy, and D65's.

## Getting this wrong

If your work is here and the description is inaccurate, the licence is wrong, or you would rather not be listed — open an issue and it will be fixed.
