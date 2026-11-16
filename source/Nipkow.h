#pragma once

#include "Apparatus.h"
#include "Controls.h"
#include "PassBuffer.h"

#include <FFGLSDK.h>
// After FFGLSDK.h, which is where FFUInt32 comes from.
#include "StoatworksAboutParams.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace nipkow
{

/// The harness's negative controls that reach the shaders or the plugin's
/// own bookkeeping. All off in the plugin.
struct PluginHooks
{
	apparatus::ChainHooks chain;
	float holeSpacing  = 1.0f; ///< 1.02 a spiral 2% too steep; 0 every hole on one circle
	int holeWidthModel = 0;    ///< 1 every hole the same angular width (no 1 / r)
	int scanModel      = 0;    ///< 1 at most one scan of a band a frame
	bool drumAsDisc    = false;///< the drum's straight lines explored along the disc's arcs
	int alphaModel     = 0;    ///< 1 the clip's alpha passed through
	bool premultiply   = true; ///< false: the clip's light not weighted by its alpha
	bool whiteScreen   = false;///< the lamp drawn in white, not the neon's colour
	bool resizeResets  = false;///< a new raster clears the exposure
	bool floatClock    = false;///< the host's clock read in single precision
	bool leakGLState   = false;///< the host's GL state not put back
};

/// Where the field sits in the host's frame, as the last frame drew it.
struct View
{
	int lines            = 30;
	int samplesLine      = 0;
	double phi           = 0.0;///< the angle between holes, rad
	double rc            = 0.0;///< the field's centre radius, holes
	double scaleX        = 0.0;///< pixels a hole, x
	double scaleY        = 0.0;///< pixels a hole, y
	double centreX       = 0.0;///< the field's centre, pixels
	double centreY       = 0.0;
	double displayScale  = 0.0;///< brightness per unit exposure
};

/**
    The plugin, `SW Nipkow`: the clip is the scene a 30-line mechanical
    television transmitter scans, and the output is what the Televisor's disc
    shows. Every frame: the clock; the table pass (the transmitter); the
    signal chain on the CPU (amplifier, lamp, phonic wheel); the exposure pass
    (the disc and the eye's stand-in, Persistence); the composite.

    Host indices are the ParamIds, About last.
*/
class NipkowPlugin : public CFFGLPlugin
{
public:
	NipkowPlugin();

	FFResult InitGL( const FFGLViewportStruct* viewport ) override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* input ) override;
	FFResult DeInitGL() override;

	FFResult SetFloatParameter( unsigned int index, float value ) override;
	float GetFloatParameter( unsigned int index ) override;
	char* GetTextParameter( unsigned int index ) override;
	/// The base class's stub fails, and a failed default deletes the instance
	/// -- so without this no real host can load the plugin.
	FFResult SetTextParameter( unsigned int index, const char* value ) override;
	FFResult SetTime( double time ) override;

	void SetById( unsigned int id, float value )
	{
		SetFloatParameter( id, value );
	}
	float GetById( unsigned int id ) const
	{
		return id < PT_COUNT ? params[ id ] : 0.0f;
	}

	/// The controls in physical units.
	apparatus::ChainSettings CurrentSettings() const;

	//-------------------------------------------------------------------
	// For the harness. Nothing in the plugin's own operation calls these.
	//-------------------------------------------------------------------
	PluginHooks& HooksForTest()
	{
		return hooks;
	}
	void SetClockScaleForTest( double scale )
	{
		clockScale = scale;
	}
	const apparatus::ChainState& Chain() const
	{
		return chain;
	}
	const apparatus::FrameOut& LastFrame() const
	{
		return frameOut;
	}
	/// The transmitter's last table, N rows of M.
	const std::vector< float >& Table() const
	{
		return table;
	}
	const View& LastView() const
	{
		return view;
	}
	/// The screen's exposure, mA s per pixel, full raster.
	GLuint ExposureTextureID() const
	{
		return exposure[ exposureCurrent ].TextureID();
	}
	double HostNow() const
	{
		return now;
	}
	double ClockScale() const
	{
		return clockScale;
	}
	/// The interval the last frame ran the apparatus over, s.
	double Interval() const
	{
		return static_cast< double >( intervalMicros ) * 1e-6;
	}
	uint32_t FrameIndex() const
	{
		return frameIndex;
	}

private:
	void UpdateClock();
	View computeView( int width, int height, int lines, int frameMode ) const;
	void draw( PassBuffer& target );
	bool render( const FFGLTextureStruct* input, const GLint* hostViewport, GLuint hostFBO, int width, int height, int64_t micros );

	GLint where( const ffglex::FFGLShader& shader, const char* name );

	float params[ PT_COUNT ] = {};
	PluginHooks hooks;
	uint32_t frameIndex = 0;

	apparatus::ChainState chain;
	apparatus::FrameOut frameOut;
	std::vector< float > table;
	View view;

	ffglex::FFGLShader tableShader, exposureShader, resampleShader, compositeShader;
	GLuint vao = 0;
	std::vector< std::pair< std::pair< GLuint, const char* >, GLint > > uniformCache;

	PassBuffer tableBuffer;
	PassBuffer exposure[ 3 ];
	int exposureCurrent = 0;
	int exposureWidth = 0, exposureHeight = 0;
	GLuint sampleTexture = 0;
	int sampleRows       = 0;

	// The clock (ferrofluid's vote on seconds or milliseconds, fountain's
	// offline fallback), kept in whole microseconds.
	double hostTime        = -1.0;
	double clockScale      = 0.0;
	double lastRawTime     = -1.0;
	double lastWallTime    = -1.0;
	double wallStart       = -1.0;
	int secondsVotes       = 0;
	int millisVotes        = 0;
	int clockSamples       = 0;
	int clockFrames        = 0;
	bool settledJump       = false;
	double now             = 0.0;
	bool haveLast          = false;
	int64_t lastMicros     = 0;
	int64_t intervalMicros = 16667;
};

} // namespace nipkow
