#!/usr/bin/env bash
#
# Mutation testing: change ONE character of the shipped code and require that a
# check fails. A check that still passes against a mutant was not looking at
# the code it claims to cover -- and a harness that compiled its own copy of
# the shaders, or its own copy of the apparatus, would pass every mutant here.
#
# One copy of the tree in a temporary directory (the FFGL SDK is symlinked,
# not copied), built arm64-only once. Each mutant edits its file in the copy,
# deletes that file's object (make compares mtimes to the second, so an edit
# within the same second as the last build would otherwise be missed; a
# header's mutant deletes every object) and the binary (a relink skipped would
# run the last mutant's nktest), builds nktest, runs the named check at 320x180
# (or the raster it names) under a time limit (a mutant can hang: honeydew's
# lesson; a timeout counts as caught), then puts the file back and deletes the
# object again. --resize always runs 1280x720 -> 320x180. (ferrofluid's
# script by way of fountain, filings and intensifier; nipkow's mutants.)
#
#     tools/mutate.sh          every mutant
#     ONLY=3 tools/mutate.sh   the third alone
#
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# file | the exact text | its one-character mutant | the check that must fail | what it is
# A check runs at 320x180 unless it names a raster ('--spiral@1280x720'), and on the
# GPU unless it starts 'sw:' (Apple's software renderer, NKTEST_RENDERER=software).
# The fields are split on '|', so neither a target nor a mutant may contain one
# (conway's trap): no GLSL || here.
MUTANTS=(
	"source/Shaders.cpp|vec3( 0.2126, 0.7152, 0.0722 ) );|vec3( 0.3126, 0.7152, 0.0722 ) );|--lamp|GLSL: the clip's luminance weights summing to 1.1, 2 -> 3"
	"source/Shaders.cpp|lum * clamp( src.a, 0.0, 1.0 ) : lum;|lum / clamp( src.a, 0.0, 1.0 ) : lum;|--alpha|GLSL: the clip's light divided by its alpha, * -> /"
	"source/Shaders.cpp|vec3 hi = pow( ( c + 0.055 ) / 1.055, vec3( 2.4 ) );|vec3 hi = pow( ( c + 0.055 ) / 1.055, vec3( 2.5 ) );|--lamp|GLSL: the clip decoded with gamma 2.5, 4 -> 5"
	"source/Shaders.cpp|offset = vec2( 0.5 * n - ( float( k ) + 0.5 ), ( s - 0.5 ) * 7.0 * n / 3.0 );|offset = vec2( 0.5 * n - ( float( k ) + 1.5 ), ( s - 0.5 ) * 7.0 * n / 3.0 );|--spiral|GLSL: the drum's line k explores line k + 1, 0 -> 1"
	"source/Shaders.cpp|offset = vec2( 0.5 * n - ( float( k ) + 0.5 ), ( s - 0.5 ) * 7.0 * n / 3.0 );|offset = vec2( 0.5 * n - ( float( k ) + 0.5 ), ( s - 0.5 ) * 8.0 * n / 3.0 );|--arc|GLSL: the drum's line 8N/3 long, 7 -> 8"
	"source/Shaders.cpp|		float h   = sine( 0.5 * phi );|		float h   = sine( 1.5 * phi );|--arc|GLSL: the disc's arc bowed three times too far, 0 -> 1"
	"source/Shaders.cpp|	vec2 lo     = centre - 0.5 * Scale;|	vec2 lo     = centre - 1.5 * Scale;|--spiral|GLSL: the transmitter's aperture a hole too wide on one side, 0 -> 1"
	"source/Shaders.cpp|		return 0.5 * v * v;|		return 1.5 * v * v;|--dwell|GLSL: the hole's entry ramp three times too steep, 0 -> 1"
	"source/Shaders.cpp|	return 2.0 * a * ( v - a );|	return 2.0 * a * ( v + a );|--dwell|GLSL: the ramp's integral past the hole's width, - -> +"
	"source/Shaders.cpp|	return ramp( x + a + L, a ) - ramp( x + a - L, a );|	return ramp( x - a + L, a ) - ramp( x + a - L, a );|--dwell|GLSL: the window's near edge a hole off, + -> -"
	"source/Shaders.cpp|	return 2.0 * t * ( 1.0 + q|	return 3.0 * t * ( 1.0 + q|--spiral|GLSL: the pixel's angle half as large again, 2 -> 3"
	"source/Shaders.cpp|	float wt        = UnitsPerPixel.x * UnitsPerPixel.y / wr;|	float wt        = UnitsPerPixel.x * UnitsPerPixel.y * wr;|--dwell|GLSL: the footprint's width across the radius, / -> *"
	"source/Shaders.cpp|	float hphi      = 0.5 * wt / r;|	float hphi      = 0.5 * wt * r;|--dwell|GLSL: the footprint's half-angle r^2 too wide, / -> *"
	"source/Shaders.cpp|	float halfField = 0.5 * Phi;|	float halfField = 0.6 * Phi;|--dwell|GLSL: the plate a fifth wider than the field, 5 -> 6"
	"source/Shaders.cpp|		float rk  = Rc + HoleSpacing * ( 0.5 * ( n - 1.0 ) - float( k ) );|		float rk  = Rc + HoleSpacing * ( 0.5 * ( n - 2.0 ) - float( k ) );|--spiral|GLSL: every hole half a hole in, 1 -> 2"
	"source/Shaders.cpp|		precise float rlo = max( rel - hr, -0.5 );|		precise float rlo = max( rel - hr, -1.5 );|--dwell|GLSL: the hole two lines deep, 0 -> 1"
	"source/Shaders.cpp|		float a   = HoleWidthModel == 1 ? 0.5 / Rc : 0.5 / rk;|		float a   = HoleWidthModel == 1 ? 0.5 / Rc : 1.5 / rk;|--dwell|GLSL: the hole three times as wide, 0 -> 1"
	"source/Shaders.cpp|		float off = float( k ) + 0.5;|		float off = float( k ) + 1.5;|--spiral|GLSL: hole k a line late, 0 -> 1"
	"source/Shaders.cpp|						got += here.g * ( rh - rl ) * over / ( pb - pa );|						got += here.g * ( rh + rl ) * over / ( pb - pa );|--dwell|GLSL: the radial overlap summed, not differenced, - -> +"
	"source/Shaders.cpp|	float exposure = sum * SampleSeconds / ( 4.0 * hr * hphi );|	float exposure = sum * SampleSeconds / ( 5.0 * hr * hphi );|--dwell|GLSL: the pixel's area 5 hr hphi, 4 -> 5"
	"source/Shaders.cpp|						float q = sqrt( max( here.b, 0.0 ) );|						float q = sqrt( max( here.g, 0.0 ) );|--lamp|GLSL: the glow's extent from the light, b -> g"
	"source/Shaders.cpp|	float previous = texelFetch( Previous, at, 0 ).r * Decay;|	float previous = texelFetch( Previous, at, 0 ).g * Decay;|--flicker|GLSL: the screen forgets each frame, r -> g"
	"source/Shaders.cpp|			float ox = min( hi.x, float( x + 1 ) ) - max( lo.x, float( x ) );|			float ox = min( hi.x, float( x + 2 ) ) - max( lo.x, float( x ) );|--resize|GLSL: the resample's overlap a pixel too wide, 1 -> 2"
	"source/Shaders.cpp|	float outA  = AlphaModel == 1 ? src.a : 1.0;|	float outA  = AlphaModel == 1 ? src.a : 0.0;|--alpha|GLSL: the screen transparent, 1 -> 0"
	"source/Shaders.cpp|	vec3 hi = 1.055 * pow( c, vec3( 1.0 / 2.4 ) ) - 0.055;|	vec3 hi = 1.055 * pow( c, vec3( 1.0 / 2.4 ) ) - 0.065;|--colour|GLSL: the sRGB encode's offset 0.065, 5 -> 6"
	"source/Apparatus.cpp|	return 7.0 * n * n / ( 6.0 * kPi );|	return 8.0 * n * n / ( 6.0 * kPi );|--spiral|C++: the disc's centre radius 8N^2 / 6 pi, 7 -> 8"
	"source/Apparatus.cpp|	const double lag = kLagAtBreakdown * std::pow( kLagAtOvervolt / kLagAtBreakdown, overvolts / kLagOvervolt );|	const double lag = kLagAtBreakdown * std::pow( kLagAtOvervolt / kLagAtBreakdown, overvolts * kLagOvervolt );|--lamp|C++: the formative lag's exponent overvolts x 65 V, / -> *"
	"source/Apparatus.cpp|	return kSourceOhms * ( currentMa - kStrikeCurrent ) * 1e-3;|	return kSourceOhms * ( currentMa - kStrikeCurrent ) * 1e-2;|--lamp|C++: the overvoltage ten times too high, 3 -> 2"
	"source/Apparatus.cpp|-0.9689 * X + 1.8758 * Y|-0.9689 * X + 1.9758 * Y|--colour|C++: the XYZ-to-sRGB matrix's green, 8 -> 9"
	"source/Apparatus.cpp|	return static_cast< int64_t >( std::floor( static_cast< double >( microseconds ) * perSecond / 1e6 ) );|	return static_cast< int64_t >( std::floor( static_cast< double >( microseconds ) * perSecond / 1e5 ) );|--rate|C++: the samples due ten times too many, 6 -> 5"
	"source/Apparatus.cpp|	out.decay         = std::exp( -out.covered / settings.persistence );|	out.decay         = std::exp( +out.covered / settings.persistence );|--flicker|C++: the screen's decay growing, - -> +"
	"source/Apparatus.cpp|	const double tau   = hooks.tauScale / ( 2.0 * kPi * settings.bandwidth );|	const double tau   = hooks.tauScale / ( 3.0 * kPi * settings.bandwidth );|--bandwidth|C++: the amplifier's tau 1 / 3 pi B, 2 -> 3"
	"source/Apparatus.cpp|	const double ampK  = tau / dt * ( 1.0 - ampE );|	const double ampK  = tau / dt * ( 2.0 - ampE );|--bandwidth|C++: the sample's average response wrong, 1 -> 2"
	"source/Apparatus.cpp|	const double kappa = wh * wh / ( 2.0 * kPi * N );|	const double kappa = wh * wh / ( 3.0 * kPi * N );|--hunting|C++: the wheel's stiffness two thirds, 2 -> 3"
	"source/Apparatus.cpp|	const double weightStep = std::exp( dt / settings.persistence );|	const double weightStep = std::exp( dt * settings.persistence );|--flicker|C++: the samples' persistence weights' step, / -> *"
	"source/Apparatus.cpp|		const double I       = settings.lampMa + kSignalSwing * ( 2.0 * average - 1.0 );|		const double I       = settings.lampMa + kSignalSwing * ( 3.0 * average - 1.0 );|--lamp|C++: the signal's swing half as large again, 2 -> 3"
	"source/Apparatus.cpp|					litFraction = 1.0 - need / got;|					litFraction = 1.0 - need * got;|--geometry|C++: the striking sample's lit share, / -> *"
	"source/Apparatus.cpp|			const double glow  = std::min( 1.0, I / kCoverCurrent );|			const double glow  = std::min( 1.0, I * kCoverCurrent );|--lamp|C++: the glow's covering current inverted, / -> *"
	"source/Apparatus.cpp|		const double square = litFraction * I * I;|		const double square = litFraction * I / I;|--hunting|C++: the coils' pull from the lit share alone, * -> /"
	"source/Apparatus.cpp|				s.pull = std::max( 0.0, s.lineSq / s.lineN - s.syncSq / s.syncN );|				s.pull = std::max( 0.0, s.lineSq / s.lineN + s.syncSq / s.syncN );|--hunting|C++: the sync's current adds to the pull, - -> +"
	"source/Apparatus.cpp|			const double acc   = hooks.dampingSign * ( nuFree - s.psiDot ) / kMotorTau|			const double acc   = hooks.dampingSign * ( nuFree + s.psiDot ) / kMotorTau|--hunting|C++: the motor's damping driving the swing, - -> +"
	"source/Apparatus.cpp|			                   - kappa * scale * std::sin( 2.0 * kPi * ( N * s.psi - settings.framing ) );|			                   - kappa * scale * std::sin( 3.0 * kPi * ( N * s.psi - settings.framing ) );|--hunting|C++: the wheel's teeth 1.5 a picture, 2 -> 3"
	"source/Apparatus.cpp|		double u = base + static_cast< double >( i ) / M + timesN * ( s.psi - psi0 );|		double u = base + static_cast< double >( i ) * M + timesN * ( s.psi - psi0 );|--dwell|C++: the receiver's phase a line a sample, / -> *"
	"source/Controls.cpp|	return low * std::pow( high / low, clamp01( v ) );|	return low * std::pow( high * low, clamp01( v ) );|--bandwidth|C++: a geometric control's range high x low, / -> *"
	"source/Controls.cpp|	return low + ( high - low ) * clamp01( v );|	return low + ( high + low ) * clamp01( v );|--slip|C++: a linear control's range high + low, - -> +"
	"source/Nipkow.cpp|		v.scaleX = v.scaleY = height / ( 2.0 * outer * std::sin( 0.5 * v.phi ) );|		v.scaleX = v.scaleY = height / ( 3.0 * outer * std::sin( 0.5 * v.phi ) );|--spiral|C++: the Baird frame two thirds the height, 2 -> 3"
	"source/Nipkow.cpp|	v.centreX      = 0.5 * width;|	v.centreX      = 0.6 * width;|--spiral|C++: the field off the frame's centre, 5 -> 6"
	"source/Nipkow.cpp|	v.displayScale  = pi * v.rc / ( PersistenceFromParam( params[ PT_PERSISTENCE ] ) * apparatus::kReferenceCurrent );|	v.displayScale  = pi / v.rc / ( PersistenceFromParam( params[ PT_PERSISTENCE ] ) * apparatus::kReferenceCurrent );|--colour|C++: the display divided by r_c, * -> /"
	"source/Nipkow.cpp|			clockScale  = hostDelta < 1.0 ? 1.0 : 0.001;|			clockScale  = hostDelta > 1.0 ? 1.0 : 0.001;|--clock|C++: the clock-unit fallback upside down, < -> >"
	"source/Nipkow.cpp|	const int64_t micros = std::llround( now * 1e6 );|	const int64_t micros = std::llround( now * 1e5 );|--rate|C++: the host's time read a tenth as long, 6 -> 5"
	"source/Shaders.cpp|		offset    = vec2( d - 2.0 * rk * h * h, rk * sine( phi ) );|		offset    = vec2( d - 2.0 * rk * h * h, rk * sin ( phi ) );|sw:--arc@1280x720|GLSL: the disc's height through the built-in sin, on Apple's software renderer, sine( -> sin ("
)

limit() {
	# macOS has no timeout(1): perl's alarm is enough.
	perl -e 'alarm shift; exec @ARGV' "$@"
}

# run_check CHECK [nktest output]: the check at its raster, on its renderer.
run_check() {
	local check="$1" size=320x180 renderer=""
	if [[ "$check" == sw:* ]]; then renderer=software; check="${check#sw:}"; fi
	if [[ "$check" == *@* ]]; then size="${check#*@}"; check="${check%@*}"; fi
	NKTEST_RENDERER="$renderer" limit 600 "$tree/build/nktest" "$check" --size "$size"
}

tree="$WORK/tree"
mkdir -p "$tree/external"
cp -R "$REPO/source" "$REPO/tools" "$REPO/cmake" "$REPO/CMakeLists.txt" "$tree/"
ln -s "$REPO/external/ffgl" "$tree/external/ffgl"
cmake -S "$tree" -B "$tree/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DNIPKOW_BUILD_TOOLS=ON >/dev/null
cmake --build "$tree/build" --target nktest -j8 >/dev/null 2>&1 || { echo "the unmutated copy does not build"; exit 1; }
# The unmutated copy must pass every check the mutants name: a check failing
# already would catch every mutant for nothing.
for check in $( printf '%s\n' "${MUTANTS[@]}" | cut -d'|' -f4 | sort -u ); do
	run_check "$check" >/dev/null 2>&1 || { echo "the unmutated copy fails $check"; exit 1; }
done

# The edit, in Python, given its arguments on the command line (no heredoc:
# a heredoc needs a writable TMPDIR, and this script must not depend on one).
EDIT='
import sys, pathlib
path, original, mutant = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3]
text = path.read_text()
if text.count(original) != 1:
    sys.exit(f"mutation target found {text.count(original)} times in {path}: {original!r}")
if len(original) != len(mutant) or sum(a != b for a, b in zip(original, mutant)) != 1:
    sys.exit("a mutant must differ by exactly one character")
path.write_text(text.replace(original, mutant))
'

caught=0
ran=0
index=0
for entry in "${MUTANTS[@]}"; do
	index=$(( index + 1 ))
	[[ -n "${ONLY:-}" && "$ONLY" != "$index" ]] && continue
	ran=$(( ran + 1 ))
	IFS='|' read -r file original mutant check what <<<"$entry"
	cp "$REPO/$file" "$tree/$file"
	printf '\n== mutant %d: %s\n' "$index" "$what"
	if ! python3 -c "$EDIT" "$tree/$file" "$original" "$mutant"; then
		printf '   FAIL  the mutant could not be applied\n'
		continue
	fi

	if [[ "$file" == *.h ]]; then
		find "$tree/build" -name '*.o' -delete
	else
		find "$tree/build" -name "$( basename "$file" ).o" -delete
	fi
	rm -f "$tree/build/nktest"
	status=0
	if ! cmake --build "$tree/build" --target nktest -j8 >"$WORK/build.log" 2>&1; then
		# A mutant that does not compile proves nothing about the checks.
		printf '   FAIL  the mutant does not build -- choose another\n'
		tail -5 "$WORK/build.log" | sed 's/^/        /'
	elif [[ "$file" == source/Shaders.cpp ]] && ! LC_ALL=C grep -qF -- "$mutant" "$tree/build/nktest"; then
		# A GLSL mutant is a string literal: it must be in the binary, verbatim.
		printf '   FAIL  the mutant is not in the built binary -- the build did not pick it up\n'
	else
		run_check "$check" >"$WORK/log" 2>&1 || status=$?
		if [[ "$status" -eq 0 ]]; then
			printf '   FAIL  %s still PASSES -- the check does not cover this code\n' "$check"
		else
			printf '   ok    %s fails against the mutant (exit %d%s):\n' "$check" "$status" "$( [[ "$status" -eq 142 ]] && echo ', the time limit' )"
			# A mutant that timed out printed no FAIL: grep finding none must not end the run.
			{ grep -E '^  FAIL' "$WORK/log" || true; } | /usr/bin/head -2 | cut -c1-160 | sed 's/^/        /'
			caught=$(( caught + 1 ))
		fi
	fi
	cp "$REPO/$file" "$tree/$file"
	if [[ "$file" == *.h ]]; then
		find "$tree/build" -name '*.o' -delete
	else
		find "$tree/build" -name "$( basename "$file" ).o" -delete
	fi
done

printf '\nmutants: %d, caught: %d\n' "$ran" "$caught"
[[ "$ran" -gt 0 && "$caught" -eq "$ran" ]]
