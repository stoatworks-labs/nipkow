#pragma once

#include <cstdint>
#include <vector>

/**
    The apparatus on the CPU: the disc's geometry, the amplifier, the neon
    lamp and the phonic wheel, run in double a transmitter sample at a time.
    No GL here, so the harness's --geometry check runs it without a context.

    Units: one hole (the line pitch) is the length unit, N is Lines, f the
    Picture Rate (pictures a second = the disc's revolutions a second). Phases
    are in lines (u) or revolutions (psi). Currents in mA, times in seconds.
    Every constant carries its source; see SPEC-nipkow.md for the quotations.
*/
namespace nipkow::apparatus
{

constexpr double kPi = 3.14159265358979323846;

//---------------------------------------------------------------------------
// The lamp.
//---------------------------------------------------------------------------
/// The signal's swing either side of the standing current, mA ("unlikely to
/// be more than 5 or 10 milliamps", Television, Feb 1930 p. 598).
constexpr double kSignalSwing = 10.0;
/// Below this the glow covers only part of the plate, mA (Raytheon 1928:
/// "approximately five milliamperes").
constexpr double kCoverCurrent = 5.0;
/// Breakdown and maintaining voltages, V (M&BC p. 73, Woodford 1930: 160-180;
/// Collins pp. 148-149: about 140 after a short time in operation).
constexpr double kBreakdownVolts = 170.0;
constexpr double kMaintainVolts  = 140.0;
/// The output stage's source resistance, ohms: chosen (no figure found).
constexpr double kSourceOhms = 10e3;
/// The drive current above which an out lamp strikes, mA: (V_b - V_m) / R.
constexpr double kStrikeCurrent = ( kBreakdownVolts - kMaintainVolts ) / kSourceOhms * 1e3;
/// The formative lag, log-linear in the overvoltage through GE's two points
/// (Glow Lamp Manual p. 4: about 300 us just above breakdown, under 25 us at
/// 60-70 V over), floored at Signalite's 10 us.
constexpr double kLagAtBreakdown = 300e-6;
constexpr double kLagAtOvervolt  = 25e-6;
constexpr double kLagOvervolt    = 65.0;
constexpr double kLagFloor       = 10e-6;
/// The current at which the display's scale is set: a lamp held here
/// averages 0.5 (linear) at the field's centre radius.
constexpr double kReferenceCurrent = 20.0;

//---------------------------------------------------------------------------
// The phonic wheel.
//---------------------------------------------------------------------------
/// The motor's mechanical time constant, s: chosen.
constexpr double kMotorTau = 1.0;
/// The hunting frequency at the reference pull, Hz: chosen.
constexpr double kHuntHz = 1.5;
/// The reference pull, mA^2: the median demo clip's mean C at the default
/// Lamp (tools/clips.py defaults).
extern const double kReferencePull;

//---------------------------------------------------------------------------
// The colour: CIE 1931 x 0.665, y 0.334 (Lamptech, the Osram neon spectral
// lamp), brought into sRGB by mixing with D65 white at constant luminance.
//---------------------------------------------------------------------------
constexpr double kNeonX = 0.665, kNeonY = 0.334;

/// Samples a picture element: chosen.
constexpr int kSamplesPerElement = 4;

//---------------------------------------------------------------------------
// Geometry.
//---------------------------------------------------------------------------
/// The angle between holes, rad.
double HoleAngle( int lines );
/// The field's centre radius, holes: (7N/3) / (2 pi / N) = 7 N^2 / (6 pi).
double CentreRadius( int lines );
/// Hole k's radius (k = 0 the outermost, the picture's right-hand line).
double HoleRadius( int lines, int k );
/// Picture elements a line, ceil( 7N / 3 ): the field is 7/3 as high as wide.
int ElementsPerLine( int lines );
/// Samples a line, M.
int SamplesPerLine( int lines );
/// The last this many samples of every line are the sync pulse (one element).
constexpr int kSyncSamples = kSamplesPerElement;

/// The formative lag at an overvoltage, s.
double FormativeLag( double overvolts );
/// The overvoltage an out lamp sees at a drive current, V.
double Overvoltage( double currentMa );

/// The neon in linear sRGB per unit luminance (mapped into the gamut), and
/// the same normalised to red = 1.
void NeonLinear( double rgb[ 3 ] );
void NeonShown( double rgb[ 3 ] );

/// The harness's negative controls that reach the signal chain. All off in
/// the plugin.
struct ChainHooks
{
	double tauScale        = 1.0;  ///< 2 pi: tau = 1 / B, not 1 / ( 2 pi B )
	bool instantStrike     = false;///< no formative lag
	bool lightSquare       = false;///< light proportional to current squared
	bool pullUnscaled      = false;///< the wheel's pull not scaled by C
	double dampingSign     = 1.0;  ///< -1: the motor's damping of the wrong sign
	bool phaseNotTimesN    = false;///< the receiver's phase advanced by psi, not N psi
	bool fixedFrameStep    = false;///< the disc advanced by 1/60 s a frame, whatever the frame
	bool strikeResetEachFrame = false;///< the strike's progress forgotten at every frame
	bool noSync            = false;///< not a fault: the harness's reference picture, the sync's samples carrying the picture
};

/// What the controls ask of the chain.
struct ChainSettings
{
	int lines            = 30;
	double rate          = 12.5;
	double bandwidth     = 9000.0;
	double lampMa        = 12.0;
	int sync             = 1;  ///< Sync's index: 0 Locked, 1 Hunting, 2 Free
	double speed         = 0.0;///< the motor's speed error, a fraction
	double framing       = 0.0;///< lines
	double persistence   = 0.1;///< s
};

/// One processed sample, as the display pass reads it.
struct Sample
{
	float u;     ///< the receiver's scan phase at the sample's start, lines (reduced to one picture, 0..N)
	float light; ///< the lamp's light per unit area x its lit fraction x the persistence weight, mA
	float glow;  ///< the fraction of the plate glowing (1 at or above kCoverCurrent)
	float uRest; ///< u less the float u, so u = u + uRest to double precision
};

/// A frame's worth of the chain's output.
struct FrameOut
{
	std::vector< Sample > samples;///< count + 1 entries: the last is the end of the last sample, dark
	int count            = 0;     ///< samples processed this frame
	double sampleSeconds = 0.0;   ///< one sample's duration
	double decay         = 1.0;   ///< e^( -count x sampleSeconds / persistence )
	double covered       = 0.0;   ///< count x sampleSeconds
};

/// The chain's state, carried from frame to frame.
struct ChainState
{
	bool started     = false;
	int lines        = 0;
	int samplesLine  = 0;
	int64_t cell     = 0;   ///< the transmitter's sample within the picture, 0 .. N M - 1
	double carry     = 0.0; ///< s of host time not yet processed (under one sample)
	double amp       = 0.0; ///< the amplifier's output, 0..1
	bool lit         = false;
	double strike    = 0.0; ///< the formative lag's progress, 0..1
	double psi       = 0.0; ///< the receiver's disc angle less the transmitter's, revolutions
	double psiDot    = 0.0; ///< revolutions a second
	double pull      = 0.0; ///< C from the last whole line, mA^2
	double lineSq    = 0.0;
	double syncSq    = 0.0;
	int lineN        = 0;
	int syncN        = 0;
	int64_t strikes  = 0;   ///< strikes since the start (for the harness)
	int64_t micros   = 0;   ///< whole microseconds of host time run, since the start
	int64_t samples  = 0;   ///< samples processed since the start
	int64_t baseMicros  = 0;///< where the samples-a-second last changed
	int64_t baseSamples = 0;
	double perSecond = 0.0; ///< samples a second at the last frame
	double lastCurrent = 0.0;///< the drive current of the last sample, mA
};

/**
    Runs the signal chain over a host frame of `seconds`. `table` holds the
    transmitter's picture, N rows of M samples, the clip's luminance x alpha
    averaged over the aperture (the GPU's table pass); the sync is applied
    here. The samples are on the transmitter's clock: a frame processes the
    whole samples that fit in its time and carries the rest. `seconds` is
    whole microseconds (the plugin rounds the host's interval), so the samples
    processed by any time depend on that time alone, not on how the host cut
    it into frames.
*/
void RunFrame( ChainState& state, const ChainSettings& settings, const float* table, double seconds, const ChainHooks& hooks, FrameOut& out );

/// The pull's natural angular frequency at C, rad/s (the linearised wheel).
double PullOmega( double pull );
/// The largest motor speed error the lock holds against at C, as a fraction
/// of the picture rate.
double PullOut( int lines, double rate, double pull );

} // namespace nipkow::apparatus
