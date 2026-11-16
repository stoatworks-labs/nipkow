#include "Controls.h"

#include <algorithm>
#include <cmath>

namespace nipkow
{
namespace
{
double clamp01( float v )
{
	return std::clamp( static_cast< double >( v ), 0.0, 1.0 );
}
double geometric( float v, double low, double high )
{
	return low * std::pow( high / low, clamp01( v ) );
}
float inverseGeometric( double value, double low, double high )
{
	return static_cast< float >( std::log( std::clamp( value, low, high ) / low ) / std::log( high / low ) );
}
double linear( float v, double low, double high )
{
	return low + ( high - low ) * clamp01( v );
}
float inverseLinear( double value, double low, double high )
{
	return static_cast< float >( ( std::clamp( value, low, high ) - low ) / ( high - low ) );
}

const char* const kScannerNames[] = { "Drum", "Disc" };
const char* const kSyncNames[]    = { "Locked", "Hunting", "Free" };
const char* const kFrameNames[]   = { "Baird", "Fill" };

ControlInfo standard( unsigned int id, const char* name, const char* group, float value )
{
	return { id, name, group, Kind::Standard, value, nullptr, 0, 0.0f, 1.0f };
}
ControlInfo integer( unsigned int id, const char* name, const char* group, int value, int low, int high )
{
	return { id, name, group, Kind::Integer, static_cast< float >( value ), nullptr, 0, static_cast< float >( low ), static_cast< float >( high ) };
}
ControlInfo option( unsigned int id, const char* name, const char* group, int value, const char* const* names, int count )
{
	return { id, name, group, Kind::Option, static_cast< float >( value ), names, count, 0.0f, static_cast< float >( count - 1 ) };
}
} // namespace

//---------------------------------------------------------------------------
// The defaults. The BBC's 30-line standard (Scroggie p. 18: 30 lines, 12 1/2
// pictures a second), its mirror-drum transmitter (Macnamara and Birkinshaw
// p. 731) and its 9 kc/s channel (the Televisor's Instructions; Moseley and
// Barton Chapple p. 95). The Lamp's standing current is set from Resolume's
// 33 demo clips through --pipe (tools/clips.py defaults): the lowest current,
// in whole milliamps, at which the median clip keeps the lamp lit over 99% of
// its picture. Speed +0.5% is chosen: a motor set close by hand, not exactly.
// Persistence 0.1 s is chosen for the look: about one picture.
//---------------------------------------------------------------------------
const double kDefaultLamp = 12.0;

namespace
{
const ControlInfo kTable[] = {
	integer( PT_LINES, "Lines", "Transmitter", kDefaultLines, kLinesLow, kLinesHigh ),
	standard( PT_RATE, "Picture Rate", "Transmitter", ParamFromRate( kDefaultRate ) ),
	option( PT_SCANNER, "Scanner", "Transmitter", kDrum, kScannerNames, 2 ),
	standard( PT_BANDWIDTH, "Bandwidth", "Amplifier", ParamFromBandwidth( kDefaultBandwidth ) ),
	standard( PT_LAMP, "Lamp", "Receiver", ParamFromLamp( kDefaultLamp ) ),
	option( PT_SYNC, "Sync", "Receiver", kHunting, kSyncNames, 3 ),
	standard( PT_SPEED, "Speed", "Receiver", ParamFromSpeed( kDefaultSpeed ) ),
	standard( PT_FRAMING, "Framing", "Receiver", ParamFromFraming( 0.0 ) ),
	standard( PT_PERSISTENCE, "Persistence", "Look", ParamFromPersistence( kDefaultPersistence ) ),
	option( PT_FRAME, "Frame", "Look", kBaird, kFrameNames, 2 ),
	standard( PT_MIX, "Mix", "Look", 1.0f ),
};
static_assert( sizeof( kTable ) / sizeof( kTable[ 0 ] ) == PT_ABOUT_TEXT, "every control needs a row in the table" );
} // namespace

const ControlInfo& InfoOf( unsigned int id )
{
	return kTable[ std::min< unsigned int >( id, PT_ABOUT_TEXT - 1 ) ];
}

int OptionIndex( float value, int count )
{
	const double v = std::isfinite( value ) ? static_cast< double >( value ) : 0.0;
	const long i   = std::lround( std::clamp( v, 0.0, static_cast< double >( count - 1 ) ) );
	return static_cast< int >( i );
}

int LinesFromParam( float v )
{
	const double d = std::isfinite( v ) ? static_cast< double >( v ) : static_cast< double >( kDefaultLines );
	return static_cast< int >( std::lround( std::clamp( d, static_cast< double >( kLinesLow ), static_cast< double >( kLinesHigh ) ) ) );
}
double RateFromParam( float v )
{
	return geometric( v, kRateLow, kRateHigh );
}
float ParamFromRate( double perSecond )
{
	return inverseGeometric( perSecond, kRateLow, kRateHigh );
}
double BandwidthFromParam( float v )
{
	return geometric( v, kBandwidthLow, kBandwidthHigh );
}
float ParamFromBandwidth( double hertz )
{
	return inverseGeometric( hertz, kBandwidthLow, kBandwidthHigh );
}
double LampFromParam( float v )
{
	return linear( v, kLampLow, kLampHigh );
}
float ParamFromLamp( double mA )
{
	return inverseLinear( mA, kLampLow, kLampHigh );
}
double SpeedFromParam( float v )
{
	return linear( v, kSpeedLow, kSpeedHigh );
}
float ParamFromSpeed( double fraction )
{
	return inverseLinear( fraction, kSpeedLow, kSpeedHigh );
}
double FramingFromParam( float v )
{
	return linear( v, kFramingLow, kFramingHigh );
}
float ParamFromFraming( double lines )
{
	return inverseLinear( lines, kFramingLow, kFramingHigh );
}
double PersistenceFromParam( float v )
{
	return geometric( v, kPersistenceLow, kPersistenceHigh );
}
float ParamFromPersistence( double seconds )
{
	return inverseGeometric( seconds, kPersistenceLow, kPersistenceHigh );
}
double MixFromParam( float v )
{
	return clamp01( v );
}

} // namespace nipkow
