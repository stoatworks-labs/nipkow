#pragma once

#include "StoatworksAboutLinks.h"

/**
    The host's parameters, and what they mean in physical units.

    Every ranged STANDARD parameter the host sees is 0..1, because
    `SetParamInfo` clamps an `FF_TYPE_STANDARD` default into 0..1 before
    `SetParamRange` could widen it. The conversions live in Controls.cpp, one
    function per control, each with its inverse where a default or the harness
    needs it. Lines is an `FF_TYPE_INTEGER` and holds the line count itself
    (the clamp is guarded by the type, and SetParamRange then widens it).
    Option parameters hold the element index (an option reads back 0..1
    whatever its count: map by index).
*/
namespace nipkow
{

/// Every control, by what it is. The host sees them in this order, with the
/// About block last.
enum ParamId : unsigned int
{
	// -- Transmitter -------------------------------------------------------------
	PT_LINES = 0,
	PT_RATE,
	PT_SCANNER,

	// -- Amplifier ---------------------------------------------------------------
	PT_BANDWIDTH,

	// -- Receiver ----------------------------------------------------------------
	PT_LAMP,
	PT_SYNC,
	PT_SPEED,
	PT_FRAMING,

	// -- Look --------------------------------------------------------------------
	PT_PERSISTENCE,
	PT_FRAME,
	PT_MIX,

	// -- The Stoatworks About block: a text line, then one button per link.
	PT_ABOUT_TEXT,
	PT_COUNT = PT_ABOUT_TEXT + 1 + stoatworks::about::kButtonCount
};

enum class Kind
{
	Standard,
	Integer,
	Option
};

/// One control as the host is told about it.
struct ControlInfo
{
	unsigned int id;
	const char* name;///< at most 16 characters: FFGL's name field is not terminated
	const char* group;
	Kind kind;
	float defaultValue;///< the host's 0..1, the integer itself, or the element index
	const char* const* options;
	int optionCount;
	float low, high;///< an Integer's range
};

/// The table, indexed by ParamId, for every id below PT_ABOUT_TEXT.
const ControlInfo& InfoOf( unsigned int id );

/// An option's index from the value the host stored.
int OptionIndex( float value, int count );

/// The transmitter's scanner, by the Scanner option's index.
enum Scanner
{
	kDrum = 0,///< a mirror drum: straight vertical lines (the BBC, 1932-35)
	kDisc = 1 ///< a disc like the receiver's: the same arcs
};

/// The synchroniser, by the Sync option's index.
enum Sync
{
	kLocked  = 0,///< an ideal synchroniser
	kHunting = 1,///< the phonic wheel
	kFree    = 2 ///< the wheel's coils disconnected
};

/// How the field sits in the host's frame, by the Frame option's index.
enum FrameMode
{
	kBaird = 0,///< the 7:3 field, whole, to the host's height
	kFill  = 1 ///< the field's nominal rectangle stretched to the frame
};

//---------------------------------------------------------------------------
// Conversions, the host's value to physical and back.
//---------------------------------------------------------------------------
int LinesFromParam( float v );           ///< 8..120, the integer
double RateFromParam( float v );         ///< pictures a second, 2..30, geometric
float ParamFromRate( double perSecond );
double BandwidthFromParam( float v );    ///< Hz, 500..40000, geometric
float ParamFromBandwidth( double hertz );
double LampFromParam( float v );         ///< mA standing current, 0..30, linear
float ParamFromLamp( double mA );
double SpeedFromParam( float v );        ///< the motor's speed error, -0.05..+0.05, linear
float ParamFromSpeed( double fraction );
double FramingFromParam( float v );      ///< lines, -0.5..+0.5, linear
float ParamFromFraming( double lines );
double PersistenceFromParam( float v );  ///< s, 0.02..1, geometric
float ParamFromPersistence( double seconds );
double MixFromParam( float v );          ///< 0..1

/// The ranges, for the harness and the docs.
constexpr int kLinesLow = 8, kLinesHigh = 120;
constexpr double kRateLow = 2.0, kRateHigh = 30.0;
constexpr double kBandwidthLow = 500.0, kBandwidthHigh = 40000.0;
constexpr double kLampLow = 0.0, kLampHigh = 30.0;
constexpr double kSpeedLow = -0.05, kSpeedHigh = 0.05;
constexpr double kFramingLow = -0.5, kFramingHigh = 0.5;
constexpr double kPersistenceLow = 0.02, kPersistenceHigh = 1.0;

/// The defaults in physical units (Controls.cpp says where each comes from).
constexpr int kDefaultLines           = 30;
constexpr double kDefaultRate         = 12.5;
constexpr double kDefaultBandwidth    = 9000.0;
extern const double kDefaultLamp;
constexpr double kDefaultSpeed        = 0.005;
constexpr double kDefaultPersistence  = 0.1;

} // namespace nipkow
