#include "Apparatus.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace nipkow::apparatus
{

/// Set from Resolume's demo clips (tools/clips.py defaults): the median
/// clip's mean C over its picture at the default Lamp, mA^2.
const double kReferencePull = 60.0;

double HoleAngle( int lines )
{
	return 2.0 * kPi / static_cast< double >( lines );
}

double CentreRadius( int lines )
{
	const double n = static_cast< double >( lines );
	return 7.0 * n * n / ( 6.0 * kPi );
}

double HoleRadius( int lines, int k )
{
	return CentreRadius( lines ) + 0.5 * static_cast< double >( lines - 1 ) - static_cast< double >( k );
}

int ElementsPerLine( int lines )
{
	//ceil( 7N / 3 ) in integers.
	return ( 7 * lines + 2 ) / 3;
}

int SamplesPerLine( int lines )
{
	return kSamplesPerElement * ElementsPerLine( lines );
}

double FormativeLag( double overvolts )
{
	if( !( overvolts > 0.0 ) )
		return std::numeric_limits< double >::infinity();
	const double lag = kLagAtBreakdown * std::pow( kLagAtOvervolt / kLagAtBreakdown, overvolts / kLagOvervolt );
	return std::max( lag, kLagFloor );
}

double Overvoltage( double currentMa )
{
	return kSourceOhms * ( currentMa - kStrikeCurrent ) * 1e-3;
}

void NeonLinear( double rgb[ 3 ] )
{
	//CIE XYZ at Y = 1, then IEC 61966-2-1's matrix to linear sRGB.
	const double X = kNeonX / kNeonY, Y = 1.0, Z = ( 1.0 - kNeonX - kNeonY ) / kNeonY;
	double c[ 3 ] = { 3.2406 * X - 1.5372 * Y - 0.4986 * Z, -0.9689 * X + 1.8758 * Y + 0.0415 * Z, 0.0557 * X - 0.2040 * Y + 1.0570 * Z };
	//Mixed with white of the same luminance until no channel is negative:
	//the hue line through D65, the luminance kept.
	double t = 0.0;
	for( const double v : c )
		if( v < 0.0 )
			t = std::max( t, -v / ( 1.0 - v ) );
	for( int i = 0; i < 3; ++i )
		rgb[ i ] = std::max( 0.0, ( 1.0 - t ) * c[ i ] + t );
}

void NeonShown( double rgb[ 3 ] )
{
	NeonLinear( rgb );
	const double r = rgb[ 0 ];
	for( int i = 0; i < 3; ++i )
		rgb[ i ] /= r;
}

double PullOmega( double pull )
{
	return 2.0 * kPi * kHuntHz * std::sqrt( std::max( pull, 0.0 ) / kReferencePull );
}

double PullOut( int lines, double rate, double pull )
{
	const double wh = 2.0 * kPi * kHuntHz;
	return kMotorTau * wh * wh * ( pull / kReferencePull ) / ( 2.0 * kPi * static_cast< double >( lines ) * rate );
}

//---------------------------------------------------------------------------
namespace
{
/// The whole microseconds since the base, the samples a second, and the
/// samples processed since the base: floor( us x rate / 1e6 ) depends on the
/// total alone, so how the host cut the time into frames cannot change it.
int64_t samplesDue( int64_t microseconds, double perSecond )
{
	return static_cast< int64_t >( std::floor( static_cast< double >( microseconds ) * perSecond / 1e6 ) );
}
} // namespace

void RunFrame( ChainState& s, const ChainSettings& settings, const float* table, double seconds, const ChainHooks& hooks, FrameOut& out )
{
	const int N               = std::max( settings.lines, 1 );
	const int M               = SamplesPerLine( N );
	const int64_t perPicture  = static_cast< int64_t >( N ) * M;
	const double f            = settings.rate;
	const double perSecond    = static_cast< double >( perPicture ) * f;
	const double dt           = 1.0 / perSecond;
	const double nuFree       = settings.speed * f;

	if( !s.started )
	{
		s            = ChainState {};
		s.started    = true;
		s.lines      = N;
		s.samplesLine = M;
		s.amp        = table ? std::max( 0.0, static_cast< double >( table[ 0 ] ) ) : 0.0;
		s.psi        = settings.framing / N;
		s.psiDot     = settings.sync == 0 ? 0.0 : nuFree;
		s.perSecond  = perSecond;
	}
	if( s.lines != N || s.samplesLine != M )
	{
		//A new standard: keep the picture's phase, start the line's sums afresh.
		const double p = static_cast< double >( s.cell ) / ( static_cast< double >( s.lines ) * s.samplesLine );
		s.cell        = std::clamp< int64_t >( static_cast< int64_t >( std::floor( p * perPicture ) ), 0, perPicture - 1 );
		s.lines       = N;
		s.samplesLine = M;
		s.lineSq = s.syncSq = 0.0;
		s.lineN = s.syncN = 0;
	}
	if( s.perSecond != perSecond )
	{
		s.baseMicros  = s.micros;
		s.baseSamples = s.samples;
		s.perSecond   = perSecond;
	}
	if( hooks.strikeResetEachFrame )
		s.strike = 0.0;

	const double span = hooks.fixedFrameStep ? 1.0 / 60.0 : seconds;
	s.micros += static_cast< int64_t >( std::llround( std::max( span, 0.0 ) * 1e6 ) );
	const int64_t due = s.baseSamples + samplesDue( s.micros - s.baseMicros, perSecond );
	const int count   = static_cast< int >( std::clamp< int64_t >( due - s.samples, 0, 50000000 ) );
	s.samples += count;

	out.count         = count;
	out.sampleSeconds = dt;
	out.covered       = count * dt;
	out.decay         = std::exp( -out.covered / settings.persistence );
	out.samples.resize( static_cast< size_t >( count ) + 1 );

	const double tau   = hooks.tauScale / ( 2.0 * kPi * settings.bandwidth );
	const double ampE  = std::exp( -dt / tau );
	const double ampK  = tau / dt * ( 1.0 - ampE );//the average of the response over a sample
	const double wh    = 2.0 * kPi * kHuntHz;
	const double kappa = wh * wh / ( 2.0 * kPi * N );
	const double weightStep = std::exp( dt / settings.persistence );//older samples first: the weight grows

	//The receiver's phase at the frame's first sample, reduced to one picture
	//in double; every sample's is stored relative to it.
	const double timesN = hooks.phaseNotTimesN ? 1.0 : static_cast< double >( N );
	const double psi0   = s.psi;
	double base         = static_cast< double >( s.cell ) / M + timesN * psi0;
	base -= N * std::floor( base / N );

	//The persistence weight of sample i, at its middle: e^( -( count - i - 1/2 ) dt / tau ).
	double weight = std::exp( -( count - 0.5 ) * dt / settings.persistence );
	double lastU  = -std::numeric_limits< double >::infinity();

	for( int i = 0; i < count; ++i )
	{
		const int j  = static_cast< int >( s.cell % M );
		const int kt = static_cast< int >( s.cell / M );
		double x     = 0.0;
		if( j < M - kSyncSamples && table != nullptr )
			x = std::max( 0.0, static_cast< double >( table[ static_cast< size_t >( kt ) * M + j ] ) );

		//The amplifier, exact for the held input; the lamp sees the average.
		const double average = x + ( s.amp - x ) * ampK;
		s.amp                = x + ( s.amp - x ) * ampE;
		const double I       = settings.lampMa + kSignalSwing * ( 2.0 * average - 1.0 );
		s.lastCurrent        = I;

		//The lamp.
		double litFraction = 0.0;
		if( s.lit )
		{
			if( I <= 0.0 )
			{
				s.lit    = false;
				s.strike = 0.0;
			}
			else
				litFraction = 1.0;
		}
		else if( I > kStrikeCurrent )
		{
			if( hooks.instantStrike )
			{
				s.lit       = true;
				litFraction = 1.0;
				++s.strikes;
			}
			else
			{
				const double got  = dt / FormativeLag( Overvoltage( I ) );
				const double need = 1.0 - s.strike;
				if( got >= need )
				{
					litFraction = 1.0 - need / got;
					s.lit       = true;
					s.strike    = 0.0;
					++s.strikes;
				}
				else
					s.strike += got;
			}
		}
		else
			s.strike = 0.0;

		Sample& o = out.samples[ static_cast< size_t >( i ) ];
		o.glow    = 1.0f;
		o.light   = 0.0f;
		o.spare   = 0.0f;
		if( litFraction > 0.0 )
		{
			const double light = hooks.lightSquare ? I * I / kReferenceCurrent : I;
			const double glow  = std::min( 1.0, I / kCoverCurrent );
			o.glow             = static_cast< float >( glow );
			o.light            = static_cast< float >( litFraction * light / glow * weight );
		}
		weight *= weightStep;

		//The wheel's coils carry the lamp's current: nothing while it is out.
		const double square = litFraction * I * I;
		if( j < M - kSyncSamples )
		{
			s.lineSq += square;
			++s.lineN;
		}
		else
		{
			s.syncSq += square;
			++s.syncN;
		}
		if( j == M - 1 )
		{
			if( s.lineN > 0 && s.syncN > 0 )
				s.pull = std::max( 0.0, s.lineSq / s.lineN - s.syncSq / s.syncN );
			s.lineSq = s.syncSq = 0.0;
			s.lineN = s.syncN = 0;
		}

		//The receiver's phase at the sample's start; never backwards (the
		//display's search needs it monotone, and in range it always is).
		double u = base + static_cast< double >( i ) / M + timesN * ( s.psi - psi0 );
		u        = std::max( u, lastU );
		lastU    = u;
		o.u      = static_cast< float >( u );

		//The phonic wheel, a semi-implicit Euler step.
		switch( settings.sync )
		{
		case 0:
			s.psi    = settings.framing / N;
			s.psiDot = 0.0;
			break;
		case 1:
		{
			const double scale = hooks.pullUnscaled ? 1.0 : s.pull / kReferencePull;
			const double acc   = hooks.dampingSign * ( nuFree - s.psiDot ) / kMotorTau
			                   - kappa * scale * std::sin( 2.0 * kPi * ( N * s.psi - settings.framing ) );
			s.psiDot += acc * dt;
			s.psi += s.psiDot * dt;
			break;
		}
		default:
		{
			const double acc = hooks.dampingSign * ( nuFree - s.psiDot ) / kMotorTau;
			s.psiDot += acc * dt;
			s.psi += s.psiDot * dt;
			break;
		}
		}
		if( !std::isfinite( s.psi ) || !std::isfinite( s.psiDot ) )
		{
			s.psi    = settings.framing / N;
			s.psiDot = 0.0;
		}

		s.cell = s.cell + 1 == perPicture ? 0 : s.cell + 1;
	}
	{
		double u = base + static_cast< double >( count ) / M + timesN * ( s.psi - psi0 );
		u        = std::max( u, lastU );
		Sample& o = out.samples[ static_cast< size_t >( count ) ];
		o.u       = static_cast< float >( u );
		o.light   = 0.0f;
		o.glow    = 1.0f;
		o.spare   = 0.0f;
	}
	if( settings.sync == 0 )
	{
		s.psi    = settings.framing / N;
		s.psiDot = 0.0;
	}
	//psi to one revolution: N psi moves by whole pictures, which the phase's
	//reduction above takes out again.
	s.psi -= std::floor( s.psi );
}

} // namespace nipkow::apparatus
