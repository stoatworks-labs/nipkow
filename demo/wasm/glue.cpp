/*
    The page's way in to the plugin: what an FFGL host does to a plugin
    instance, as C functions JavaScript can call.

    Nothing here is the plugin, and nothing here does the plugin's work. The
    plugin is `source/Nipkow.cpp` and everything it calls, compiled UNMODIFIED
    into the same WebAssembly module as this file (see
    demo/tools/build-wasm.sh for the list). This file only plays the host:

      - it constructs a `NipkowPlugin`, as EffectPlugin.cpp's registration
        would, and calls InitGL / DeInitGL on it;
      - it reads the parameter declarations back through the FFGL SDK's own
        host-facing getters (CFFGLPluginManager::GetParamName and the rest,
        the SDK compiled unmodified too), so the page's panel is built from
        the plugin's constructor rather than from a copy of it;
      - it forwards the host's calls: SetFloatParameter, SetTime, and
        ProcessOpenGL with one input texture, as tools/nktest's Rig does.

    The read-outs (nk_convert) call Controls.cpp's own conversions, and the
    line under the picture reads the plugin's own state through the accessors
    its harness reads (Chain(), LastFrame(), LastView(), ClockScale()), the
    same numbers `nktest --stats` prints.

    GL calls go to the page's WebGL2 context through emscripten's GL library;
    demo/wasm/gl_shim.cpp replaces the six entry points that cannot be passed
    straight through.
*/
#include "Controls.h"
#include "Diag.h"
#include "Nipkow.h"

#include <emscripten/emscripten.h>

#include <sys/stat.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

using namespace nipkow;

namespace
{
/// One plugin instance and the host-side structs it is handed.
struct Instance
{
	NipkowPlugin plugin;
	bool initialised = false;

	FFGLTextureStruct input {};
	FFGLTextureStruct* inputs[ 1 ] = { &input };
	ProcessOpenGLStruct process {};

	std::string scratch;///< a string handed back to JS lives here until the next call
};

float asFloat( FFMixed mixed )
{
	float value = 0.0f;
	static_assert( sizeof( value ) == sizeof( mixed.UIntValue ), "FFMixed carries a float in its bits" );
	std::memcpy( &value, &mixed.UIntValue, sizeof( value ) );
	return value;
}

const char* hold( Instance* instance, std::string text )
{
	instance->scratch = std::move( text );
	return instance->scratch.c_str();
}
} // namespace

extern "C"
{
//---------------------------------------------------------------------------
// Lifetime.
//---------------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE Instance* nk_new()
{
	return new Instance();
}

EMSCRIPTEN_KEEPALIVE void nk_delete( Instance* instance )
{
	if( instance == nullptr )
		return;
	if( instance->initialised )
		instance->plugin.DeInitGL();
	delete instance;
}

/// InitGL, with the viewport a host would pass. 1 on success.
EMSCRIPTEN_KEEPALIVE int nk_init_gl( Instance* instance, int width, int height )
{
	FFGLViewportStruct viewport {};
	viewport.width        = static_cast< GLuint >( width );
	viewport.height       = static_cast< GLuint >( height );
	instance->initialised = instance->plugin.InitGL( &viewport ) == FF_SUCCESS;
	return instance->initialised ? 1 : 0;
}

//---------------------------------------------------------------------------
// The declarations, read as a host reads them.
//---------------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE int nk_param_count( Instance* instance )
{
	return static_cast< int >( instance->plugin.GetNumParams() );
}

EMSCRIPTEN_KEEPALIVE const char* nk_param_name( Instance* instance, int index )
{
	const char* name = instance->plugin.GetParamName( static_cast< unsigned int >( index ) );
	return name ? name : "";
}

EMSCRIPTEN_KEEPALIVE int nk_param_type( Instance* instance, int index )
{
	return static_cast< int >( instance->plugin.GetParamType( static_cast< unsigned int >( index ) ) );
}

EMSCRIPTEN_KEEPALIVE const char* nk_param_group( Instance* instance, int index )
{
	return hold( instance, instance->plugin.GetParamGroup( static_cast< unsigned int >( index ) ) );
}

/// The declared default of a numeric parameter (FF_TYPE_STANDARD already
/// clamped into 0..1 by the SDK, as a host sees it; Lines the integer).
EMSCRIPTEN_KEEPALIVE float nk_param_default( Instance* instance, int index )
{
	return asFloat( instance->plugin.GetParamDefault( static_cast< unsigned int >( index ) ) );
}

EMSCRIPTEN_KEEPALIVE int nk_param_element_count( Instance* instance, int index )
{
	return static_cast< int >( instance->plugin.GetNumParamElements( static_cast< unsigned int >( index ) ) );
}

EMSCRIPTEN_KEEPALIVE const char* nk_param_element_name( Instance* instance, int index, int element )
{
	const char* name = instance->plugin.GetParamElementName( static_cast< unsigned int >( index ), static_cast< unsigned int >( element ) );
	return name ? name : "";
}

EMSCRIPTEN_KEEPALIVE float nk_param_element_value( Instance* instance, int index, int element )
{
	return asFloat( instance->plugin.GetParamElementDefault( static_cast< unsigned int >( index ), static_cast< unsigned int >( element ) ) );
}

/// An FF_TYPE_INTEGER's declared range (SetParamRange), as a host reads it.
EMSCRIPTEN_KEEPALIVE float nk_param_range_min( Instance* instance, int index )
{
	return instance->plugin.GetParamRange( static_cast< unsigned int >( index ) ).min;
}

EMSCRIPTEN_KEEPALIVE float nk_param_range_max( Instance* instance, int index )
{
	return instance->plugin.GetParamRange( static_cast< unsigned int >( index ) ).max;
}

EMSCRIPTEN_KEEPALIVE int nk_max_inputs( Instance* instance )
{
	return static_cast< int >( instance->plugin.GetMaxInputs() );
}

//---------------------------------------------------------------------------
// The host's calls.
//---------------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE int nk_set_float( Instance* instance, int index, float value )
{
	return instance->plugin.SetFloatParameter( static_cast< unsigned int >( index ), value ) == FF_SUCCESS ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE float nk_get_float( Instance* instance, int index )
{
	return instance->plugin.GetFloatParameter( static_cast< unsigned int >( index ) );
}

/// One frame: SetTime, then ProcessOpenGL into whatever the page has bound,
/// with one input of clipWidth x clipHeight whose GL name is `clip` (a
/// texture the page registered with emscripten's GL tables). 1 on success.
EMSCRIPTEN_KEEPALIVE int nk_process( Instance* instance, double seconds, int clip, int clipWidth, int clipHeight )
{
	instance->plugin.SetTime( seconds );
	instance->input.Width = instance->input.HardwareWidth = static_cast< FFUInt32 >( clipWidth );
	instance->input.Height = instance->input.HardwareHeight = static_cast< FFUInt32 >( clipHeight );
	instance->input.Handle                                 = static_cast< GLuint >( clip );
	instance->process.numInputTextures                     = 1;
	instance->process.inputTextures                        = instance->inputs;
	instance->process.HostFBO                              = 0;
	return instance->plugin.ProcessOpenGL( &instance->process ) == FF_SUCCESS ? 1 : 0;
}

/// The harness's hook, for demo/tools/crosscheck ONLY: settles the host
/// clock's unit as nktest's Rig does (SetClockScaleForTest( 1.0 )), so a
/// page frame and a harness frame see the same intervals. The page never
/// calls it: there the plugin's own vote decides, as in a host.
EMSCRIPTEN_KEEPALIVE void nk_clock_scale_for_test( Instance* instance, double scale )
{
	instance->plugin.SetClockScaleForTest( scale );
}

//---------------------------------------------------------------------------
// For the panel's read-outs: Controls.cpp's conversion for a parameter, the
// one Nipkow.cpp's CurrentSettings() and render() apply to it. NaN for an
// option (an index) or anything with no conversion.
//---------------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE double nk_convert( int index, float v )
{
	switch( index )
	{
	case PT_LINES: return LinesFromParam( v );
	case PT_RATE: return RateFromParam( v );
	case PT_BANDWIDTH: return BandwidthFromParam( v );
	case PT_LAMP: return LampFromParam( v );
	case PT_SPEED: return SpeedFromParam( v );
	case PT_FRAMING: return FramingFromParam( v );
	case PT_PERSISTENCE: return PersistenceFromParam( v );
	case PT_MIX: return MixFromParam( v );
	default: return std::numeric_limits< double >::quiet_NaN();
	}
}

//---------------------------------------------------------------------------
// What the line under the picture reports: the plugin's own state, the
// numbers `nktest --stats` prints (tools/nktest/main.cpp, printStats).
//---------------------------------------------------------------------------
EMSCRIPTEN_KEEPALIVE int nk_samples( Instance* instance )
{
	return instance->plugin.LastFrame().count;
}
/// The lit share of the picture's samples this frame.
EMSCRIPTEN_KEEPALIVE double nk_lit_share( Instance* instance )
{
	const apparatus::FrameOut& f = instance->plugin.LastFrame();
	return f.pictureSamples > 0 ? static_cast< double >( f.pictureLit ) / f.pictureSamples : 0.0;
}
EMSCRIPTEN_KEEPALIVE double nk_current( Instance* instance )
{
	return instance->plugin.Chain().lastCurrent;
}
/// The mean pull C of the lines this frame ended, mA^2 (the last whole
/// line's, if none ended).
EMSCRIPTEN_KEEPALIVE double nk_pull( Instance* instance )
{
	const apparatus::FrameOut& f = instance->plugin.LastFrame();
	return f.pullLines > 0 ? f.pullSum / f.pullLines : instance->plugin.Chain().pull;
}
EMSCRIPTEN_KEEPALIVE double nk_psi( Instance* instance )
{
	return instance->plugin.Chain().psi;
}
EMSCRIPTEN_KEEPALIVE double nk_psi_dot( Instance* instance )
{
	return instance->plugin.Chain().psiDot;
}
EMSCRIPTEN_KEEPALIVE double nk_strikes( Instance* instance )
{
	return static_cast< double >( instance->plugin.Chain().strikes );
}
EMSCRIPTEN_KEEPALIVE int nk_lit( Instance* instance )
{
	return instance->plugin.Chain().lit ? 1 : 0;
}
/// The interval the last frame ran the apparatus over, s.
EMSCRIPTEN_KEEPALIVE double nk_interval( Instance* instance )
{
	return instance->plugin.Interval();
}
/// The host clock's unit as the plugin settled it: 1 seconds, 0.001 ms, 0 not yet.
EMSCRIPTEN_KEEPALIVE double nk_clock_scale( Instance* instance )
{
	return instance->plugin.ClockScale();
}
/// The pull-out speed at the current pull, as a fraction of the picture rate:
/// the largest motor speed error the wheel holds against (Apparatus.h).
EMSCRIPTEN_KEEPALIVE double nk_pull_out( Instance* instance )
{
	const apparatus::ChainSettings s = instance->plugin.CurrentSettings();
	return apparatus::PullOut( s.lines, s.rate, nk_pull( instance ) );
}

/// Give the plugin's log (Diag.cpp) somewhere to go: a directory in the
/// page's in-memory file system, named through the override Diag.cpp reads.
/// Its own `mkdir -p` goes through system(), which a browser does not have.
/// Call before the first InitGL, which is when the log opens.
EMSCRIPTEN_KEEPALIVE void nk_prepare_log()
{
	setenv( "NIPKOW_LOG_DIR", "/nipkow/logs", 1 );
	mkdir( "/nipkow", 0755 );
	mkdir( "/nipkow/logs", 0755 );
}

/// The plugin's log file (Diag.cpp), in the page's in-memory file system.
EMSCRIPTEN_KEEPALIVE const char* nk_log_path()
{
	static std::string path;
	path = diag::logPath();
	return path.c_str();
}

} // extern "C"
