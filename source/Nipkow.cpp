#include "Nipkow.h"

#include "Diag.h"
#include "GLState.h"
#include "Shaders.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>

using namespace ffglex;

namespace nipkow
{
namespace
{
/// Frames that must agree before the host's clock unit is settled.
constexpr int kClockVotes = 4;
/// Frames of advancing host time after which an unsettled vote is decided by
/// the size of the host's step alone (UpdateClock).
constexpr int kClockPatience = 30;
/// Microseconds a single frame may run the apparatus for.
constexpr int64_t kMaxFrameMicros = 100000;

double wallSeconds()
{
	using namespace std::chrono;
	static const steady_clock::time_point start = steady_clock::now();
	return duration_cast< duration< double > >( steady_clock::now() - start ).count();
}

std::string glStringOrUnknown( GLenum name )
{
	const GLubyte* value = glGetString( name );
	return value ? reinterpret_cast< const char* >( value ) : "unknown";
}

/// The pixel-transfer state the table's read-back and the samples' upload
/// use, put back as found: a host that leaves a pixel buffer bound would
/// otherwise have the read-back written into it.
struct ScopedPixelTransfer
{
	GLint packAlignment = 4, unpackAlignment = 4, packRowLength = 0, unpackRowLength = 0;
	GLint packBuffer = 0, unpackBuffer = 0, readFramebuffer = 0;
	GLint packSkipRows = 0, packSkipPixels = 0, unpackSkipRows = 0, unpackSkipPixels = 0;
	bool restore = true;

	ScopedPixelTransfer()
	{
		glGetIntegerv( GL_PACK_ALIGNMENT, &packAlignment );
		glGetIntegerv( GL_UNPACK_ALIGNMENT, &unpackAlignment );
		glGetIntegerv( GL_PACK_ROW_LENGTH, &packRowLength );
		glGetIntegerv( GL_UNPACK_ROW_LENGTH, &unpackRowLength );
		glGetIntegerv( GL_PACK_SKIP_ROWS, &packSkipRows );
		glGetIntegerv( GL_PACK_SKIP_PIXELS, &packSkipPixels );
		glGetIntegerv( GL_UNPACK_SKIP_ROWS, &unpackSkipRows );
		glGetIntegerv( GL_UNPACK_SKIP_PIXELS, &unpackSkipPixels );
		glGetIntegerv( GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer );
		glGetIntegerv( GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer );
		glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer );
		glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
		glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		glPixelStorei( GL_PACK_ROW_LENGTH, 0 );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
		glPixelStorei( GL_PACK_SKIP_ROWS, 0 );
		glPixelStorei( GL_PACK_SKIP_PIXELS, 0 );
		glPixelStorei( GL_UNPACK_SKIP_ROWS, 0 );
		glPixelStorei( GL_UNPACK_SKIP_PIXELS, 0 );
	}
	~ScopedPixelTransfer()
	{
		if( !restore )
			return;
		glPixelStorei( GL_PACK_ALIGNMENT, packAlignment );
		glPixelStorei( GL_UNPACK_ALIGNMENT, unpackAlignment );
		glPixelStorei( GL_PACK_ROW_LENGTH, packRowLength );
		glPixelStorei( GL_UNPACK_ROW_LENGTH, unpackRowLength );
		glPixelStorei( GL_PACK_SKIP_ROWS, packSkipRows );
		glPixelStorei( GL_PACK_SKIP_PIXELS, packSkipPixels );
		glPixelStorei( GL_UNPACK_SKIP_ROWS, unpackSkipRows );
		glPixelStorei( GL_UNPACK_SKIP_PIXELS, unpackSkipPixels );
		glBindBuffer( GL_PIXEL_PACK_BUFFER, static_cast< GLuint >( packBuffer ) );
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, static_cast< GLuint >( unpackBuffer ) );
		glBindFramebuffer( GL_READ_FRAMEBUFFER, static_cast< GLuint >( readFramebuffer ) );
	}
	ScopedPixelTransfer( const ScopedPixelTransfer& ) = delete;
	ScopedPixelTransfer& operator=( const ScopedPixelTransfer& ) = delete;
};
} // namespace

static_assert( PT_COUNT - PT_ABOUT_TEXT == stoatworks::about::kParamCount, "the About run no longer matches StoatworksAbout.h" );

//---------------------------------------------------------------------------
NipkowPlugin::NipkowPlugin()
{
	SetMinInputs( 1 );
	SetMaxInputs( 1 );

	//The disc turns, the lamp strikes and the wheel hunts in seconds.
	SetTimeSupported( true );

	for( unsigned int id = 0; id < PT_ABOUT_TEXT; ++id )
		params[ id ] = InfoOf( id ).defaultValue;

	for( unsigned int id = 0; id < PT_ABOUT_TEXT; ++id )
	{
		const ControlInfo& info = InfoOf( id );
		switch( info.kind )
		{
		case Kind::Standard: SetParamInfo( id, info.name, FF_TYPE_STANDARD, params[ id ] ); break;
		case Kind::Integer:
			//The STANDARD clamp in SetParamInfo is guarded by the type: an
			//integer's default passes through, and the range then widens it.
			SetParamInfo( id, info.name, FF_TYPE_INTEGER, params[ id ] );
			SetParamRange( id, info.low, info.high );
			break;
		case Kind::Option:
			SetOptionParamInfo( id, info.name, static_cast< unsigned int >( info.optionCount ), params[ id ] );
			for( int i = 0; i < info.optionCount; ++i )
				SetParamElementInfo( id, static_cast< unsigned int >( i ), info.options[ i ], static_cast< float >( i ) );
			break;
		}
		SetParamGroup( id, info.group );
	}

	// The About block, last. Inline rather than through a helper: SetParamInfo
	// is protected on CFFGLPlugin, so nothing outside the class can call it.
	SetParamInfo( PT_ABOUT_TEXT, "About", FF_TYPE_TEXT, stoatworks::about::defaultText() );
	{
		FFUInt32 aboutId = PT_ABOUT_TEXT + 1;
		for( const auto& b : stoatworks::about::buttons() )
			SetParamInfo( aboutId++, b.label, FF_TYPE_EVENT, false );
	}
	for( FFUInt32 i = PT_ABOUT_TEXT; i < PT_COUNT; ++i )
		SetParamGroup( i, "About" );
}

//---------------------------------------------------------------------------
GLint NipkowPlugin::where( const FFGLShader& shader, const char* name )
{
	//Keyed by the literal's address: every name here is a string literal.
	const auto key = std::make_pair( shader.GetGLID(), name );
	for( const auto& entry : uniformCache )
		if( entry.first == key )
			return entry.second;
	const GLint location = glGetUniformLocation( shader.GetGLID(), name );
	uniformCache.emplace_back( key, location );
	return location;
}

//---------------------------------------------------------------------------
FFResult NipkowPlugin::InitGL( const FFGLViewportStruct* vp )
{
	diag::init();
	diag::info( std::string( "GL vendor=" ) + glStringOrUnknown( GL_VENDOR ) + " renderer=" + glStringOrUnknown( GL_RENDERER )
	            + " version=" + glStringOrUnknown( GL_VERSION ) );

	struct Stage
	{
		FFGLShader* shader;
		shaders::Pass pass;
		const char* name;
	};
	const Stage stages[] = {
		{ &tableShader, shaders::Pass::Table, "table" },
		{ &exposureShader, shaders::Pass::Exposure, "exposure" },
		{ &resampleShader, shaders::Pass::Resample, "resample" },
		{ &compositeShader, shaders::Pass::Composite, "composite" },
	};
	for( const Stage& stage : stages )
	{
		const std::string vertex   = shaders::AssembleVertex( stage.pass );
		const std::string fragment = shaders::Assemble( stage.pass );
		if( stage.shader->Compile( vertex.c_str(), fragment.c_str() ) )
			continue;
		//Returning FF_FAIL is invisible to the operator: the plugin simply
		//does nothing in Resolume. These lines are the only record of which
		//pass it was.
		diag::error( std::string( "the " ) + stage.name + " shader failed to compile - the plugin will do nothing" );
		FFGLLog::LogToHost( "Nipkow: shader failed to compile" );
		DeInitGL();
		return FF_FAIL;
	}

	//A core profile draws nothing without a vertex array bound; the
	//triangle needs no attributes.
	glGenVertexArrays( 1, &vao );
	diag::info( "initialised" );
	return CFFGLPlugin::InitGL( vp );
}

//---------------------------------------------------------------------------
apparatus::ChainSettings NipkowPlugin::CurrentSettings() const
{
	apparatus::ChainSettings s;
	s.lines       = LinesFromParam( params[ PT_LINES ] );
	s.rate        = RateFromParam( params[ PT_RATE ] );
	s.bandwidth   = BandwidthFromParam( params[ PT_BANDWIDTH ] );
	s.lampMa      = LampFromParam( params[ PT_LAMP ] );
	s.sync        = OptionIndex( params[ PT_SYNC ], 3 );
	s.speed       = SpeedFromParam( params[ PT_SPEED ] );
	s.framing     = FramingFromParam( params[ PT_FRAMING ] );
	s.persistence = PersistenceFromParam( params[ PT_PERSISTENCE ] );
	return s;
}

View NipkowPlugin::computeView( int width, int height, int lines, int frameMode ) const
{
	View v;
	const double n = static_cast< double >( lines );
	v.lines        = lines;
	v.samplesLine  = apparatus::SamplesPerLine( lines );
	v.phi          = apparatus::HoleAngle( lines );
	v.rc           = apparatus::CentreRadius( lines );
	if( frameMode == kFill )
	{
		//The field's nominal rectangle, N wide and 7N/3 high at its centre
		//radius, stretched to the frame; the transmitter explores the clip.
		v.scaleX = width / n;
		v.scaleY = height / ( 7.0 * n / 3.0 );
	}
	else
	{
		//The whole sector to the host's height, holes square.
		const double outer = v.rc + 0.5 * n;
		v.scaleX = v.scaleY = height / ( 2.0 * outer * std::sin( 0.5 * v.phi ) );
	}
	v.centreX      = 0.5 * width;
	v.centreY      = 0.5 * height;
	v.displayScale = apparatus::kPi * v.rc / ( PersistenceFromParam( params[ PT_PERSISTENCE ] ) * apparatus::kReferenceCurrent );
	return v;
}

void NipkowPlugin::draw( PassBuffer& target )
{
	glBindFramebuffer( GL_FRAMEBUFFER, target.GetGLID() );
	glViewport( 0, 0, static_cast< GLsizei >( target.GetWidth() ), static_cast< GLsizei >( target.GetHeight() ) );
	glDrawArrays( GL_TRIANGLES, 0, 3 );
}

//---------------------------------------------------------------------------
bool NipkowPlugin::render( const FFGLTextureStruct* input, const GLint* hostViewport, GLuint hostFBO, int width, int height, int64_t micros )
{
	using S = PassBuffer::Sampling;
	const apparatus::ChainSettings settings = CurrentSettings();
	const int frameMode                     = OptionIndex( params[ PT_FRAME ], 2 );
	const int scanner                       = OptionIndex( params[ PT_SCANNER ], 2 );
	view                                    = computeView( width, height, settings.lines, frameMode );
	const int N = view.lines, M = view.samplesLine;

	ScopedPixelTransfer transfer;
	transfer.restore = !hooks.leakGLState;
	glBindVertexArray( vao );

	//1. The transmitter's table, read back.
	if( !tableBuffer.Ensure( M, N, GL_R32F, S::Nearest ) )
	{
		diag::error( "could not allocate the transmitter's table" );
		return false;
	}
	glUseProgram( tableShader.GetGLID() );
	bindUnit( 0, input->Handle );
	glUniform1i( where( tableShader, "InputTexture" ), 0 );
	glUniform2i( where( tableShader, "ClipSize" ), width, height );
	glUniform1i( where( tableShader, "Lines" ), N );
	glUniform1i( where( tableShader, "SamplesLine" ), M );
	glUniform1i( where( tableShader, "Scanner" ), ( scanner == kDisc || hooks.drumAsDisc ) ? 1 : 0 );
	glUniform2f( where( tableShader, "Centre" ), static_cast< float >( view.centreX ), static_cast< float >( view.centreY ) );
	glUniform2f( where( tableShader, "Scale" ), static_cast< float >( view.scaleX ), static_cast< float >( view.scaleY ) );
	glUniform1f( where( tableShader, "Rc" ), static_cast< float >( view.rc ) );
	glUniform1f( where( tableShader, "Phi" ), static_cast< float >( view.phi ) );
	glUniform1i( where( tableShader, "Premultiply" ), hooks.premultiply ? 1 : 0 );
	draw( tableBuffer );
	table.resize( static_cast< size_t >( M ) * N );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, tableBuffer.GetGLID() );
	glReadPixels( 0, 0, M, N, GL_RED, GL_FLOAT, table.data() );

	//2. The amplifier, the lamp and the wheel.
	apparatus::RunFrame( chain, settings, table.data(), static_cast< double >( micros ) * 1e-6, hooks.chain, frameOut );

	//3. The samples, uploaded.
	const int entries = frameOut.count + 1;
	const int rows    = ( entries + shaders::kSampleRow - 1 ) / shaders::kSampleRow;
	if( sampleTexture == 0 || rows > sampleRows )
	{
		if( sampleTexture == 0 )
			glGenTextures( 1, &sampleTexture );
		sampleRows = std::max( rows, 1 );
		glBindTexture( GL_TEXTURE_2D, sampleTexture );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, shaders::kSampleRow, sampleRows, 0, GL_RGBA, GL_FLOAT, nullptr );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}
	glBindTexture( GL_TEXTURE_2D, sampleTexture );
	{
		const int full = entries / shaders::kSampleRow, rest = entries % shaders::kSampleRow;
		const float* data = reinterpret_cast< const float* >( frameOut.samples.data() );
		if( full > 0 )
			glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, shaders::kSampleRow, full, GL_RGBA, GL_FLOAT, data );
		if( rest > 0 )
			glTexSubImage2D( GL_TEXTURE_2D, 0, 0, full, rest, 1, GL_RGBA, GL_FLOAT, data + static_cast< size_t >( full ) * shaders::kSampleRow * 4 );
	}

	//4. The exposure, carried across a change of raster: resampled into a
	//fresh buffer before the old one is released (photofinish's trap).
	if( exposureWidth > 0 && ( exposureWidth != width || exposureHeight != height ) )
	{
		if( hooks.resizeResets )
		{
			for( PassBuffer& b : exposure )
				b.Destroy();
			exposureWidth = exposureHeight = 0;
		}
		else
		{
			const int spare = ( exposureCurrent + 1 ) % 3;
			if( exposure[ spare ].Ensure( width, height, GL_R32F, S::Nearest ) )
			{
				glUseProgram( resampleShader.GetGLID() );
				bindUnit( 0, exposure[ exposureCurrent ].TextureID() );
				glUniform1i( where( resampleShader, "OldStore" ), 0 );
				glUniform2i( where( resampleShader, "OldSize" ), exposureWidth, exposureHeight );
				glUniform2f( where( resampleShader, "Ratio" ), static_cast< float >( static_cast< double >( exposureWidth ) / width ),
				             static_cast< float >( static_cast< double >( exposureHeight ) / height ) );
				draw( exposure[ spare ] );
				exposureCurrent = spare;
			}
		}
	}
	const bool fresh = exposureWidth == 0;
	const int next   = ( exposureCurrent + 1 ) % 3;
	if( !exposure[ exposureCurrent ].Ensure( width, height, GL_R32F, S::Nearest ) || !exposure[ next ].Ensure( width, height, GL_R32F, S::Nearest ) )
	{
		diag::error( "could not allocate the screen's exposure" );
		return false;
	}
	if( fresh )
	{
		//A new screen is dark.
		glBindFramebuffer( GL_FRAMEBUFFER, exposure[ exposureCurrent ].GetGLID() );
		glViewport( 0, 0, width, height );
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		glClear( GL_COLOR_BUFFER_BIT );
	}
	exposureWidth  = width;
	exposureHeight = height;

	glUseProgram( exposureShader.GetGLID() );
	bindUnit( 0, sampleTexture );
	bindUnit( 1, exposure[ exposureCurrent ].TextureID() );
	glUniform1i( where( exposureShader, "Samples" ), 0 );
	glUniform1i( where( exposureShader, "Previous" ), 1 );
	glUniform1i( where( exposureShader, "Count" ), frameOut.count );
	glUniform1f( where( exposureShader, "FirstU" ), frameOut.samples.front().u );
	glUniform1f( where( exposureShader, "LastU" ), frameOut.samples.back().u );
	glUniform1f( where( exposureShader, "Decay" ), static_cast< float >( frameOut.decay ) );
	glUniform1f( where( exposureShader, "SampleSeconds" ), static_cast< float >( frameOut.sampleSeconds ) );
	glUniform1i( where( exposureShader, "Lines" ), N );
	glUniform1f( where( exposureShader, "Rc" ), static_cast< float >( view.rc ) );
	glUniform1f( where( exposureShader, "Phi" ), static_cast< float >( view.phi ) );
	glUniform2f( where( exposureShader, "Centre" ), static_cast< float >( view.centreX ), static_cast< float >( view.centreY ) );
	glUniform2f( where( exposureShader, "UnitsPerPixel" ), static_cast< float >( 1.0 / view.scaleX ), static_cast< float >( 1.0 / view.scaleY ) );
	glUniform1f( where( exposureShader, "HoleSpacing" ), hooks.holeSpacing );
	glUniform1i( where( exposureShader, "HoleWidthModel" ), hooks.holeWidthModel );
	glUniform1i( where( exposureShader, "ScanModel" ), hooks.scanModel );
	draw( exposure[ next ] );
	exposureCurrent = next;

	//5. The composite, into the host's framebuffer and viewport.
	glBindFramebuffer( GL_FRAMEBUFFER, hostFBO );
	glViewport( hostViewport[ 0 ], hostViewport[ 1 ], hostViewport[ 2 ], hostViewport[ 3 ] );
	glUseProgram( compositeShader.GetGLID() );
	bindUnit( 0, input->Handle );
	bindUnit( 1, exposure[ exposureCurrent ].TextureID() );
	glUniform1i( where( compositeShader, "InputTexture" ), 0 );
	glUniform1i( where( compositeShader, "Exposure" ), 1 );
	glUniform2i( where( compositeShader, "ViewOrigin" ), hostViewport[ 0 ], hostViewport[ 1 ] );
	glUniform1f( where( compositeShader, "DisplayScale" ), static_cast< float >( view.displayScale ) );
	double rgb[ 3 ];
	apparatus::NeonShown( rgb );
	if( hooks.whiteScreen )
		rgb[ 0 ] = rgb[ 1 ] = rgb[ 2 ] = 1.0;
	glUniform3f( where( compositeShader, "NeonRGB" ), static_cast< float >( rgb[ 0 ] ), static_cast< float >( rgb[ 1 ] ), static_cast< float >( rgb[ 2 ] ) );
	glUniform1f( where( compositeShader, "MixAmount" ), static_cast< float >( MixFromParam( params[ PT_MIX ] ) ) );
	glUniform1i( where( compositeShader, "AlphaModel" ), hooks.alphaModel );
	glDrawArrays( GL_TRIANGLES, 0, 3 );

	unbindTextureUnits( 2 );
	glActiveTexture( GL_TEXTURE0 );
	glUseProgram( 0 );
	return true;
}

//---------------------------------------------------------------------------
void NipkowPlugin::UpdateClock()
{
	const double wallNow = wallSeconds();
	if( wallStart < 0.0 )
		wallStart = wallNow;
	const double raw = hostTime;

	//Resolume has been seen sending both seconds and milliseconds through
	//SetTime, and nothing in the call says which. Vote on it against the wall
	//clock for a few frames, then stop asking (ferrofluid's, by way of
	//rosette and millpond).
	if( clockScale == 0.0 && raw >= 0.0 && lastRawTime >= 0.0 && lastWallTime >= 0.0 )
	{
		const double hostDelta = raw - lastRawTime;
		const double wallDelta = wallNow - lastWallTime;
		if( hostDelta > 0.0 && wallDelta >= 0.0005 )
		{
			const double ratio = hostDelta / wallDelta;
			if( ratio > 0.1 && ratio < 10.0 )
				++secondsVotes;
			else if( ratio > 100.0 && ratio < 10000.0 )
				++millisVotes;
			if( secondsVotes >= kClockVotes || millisVotes >= kClockVotes )
			{
				clockScale  = millisVotes > secondsVotes ? 0.001 : 1.0;
				settledJump = true;//wall time to the host's own: no interval across it
			}
		}
		//A host that renders much faster than real time -- an offline render,
		//oxbow's selftest, nktest -- never wins the vote. After kClockPatience
		//frames the step's size decides: a frame is well under a second and
		//well over a millisecond (fountain's).
		if( hostDelta > 0.0 && clockScale == 0.0 && ++clockSamples >= kClockPatience )
		{
			clockScale  = hostDelta < 1.0 ? 1.0 : 0.001;
			settledJump = true;
		}
	}
	if( raw >= 0.0 )
		lastRawTime = raw;
	lastWallTime = wallNow;

	//Double throughout: Resolume's clock has been measured at ~499 million ms,
	//where a float resolves 0.03 s. Only differences are used.
	now = ( raw >= 0.0 && clockScale != 0.0 ) ? raw * clockScale : wallNow - wallStart;
	if( hooks.floatClock )
		now = static_cast< double >( static_cast< float >( now ) );
	if( ++clockFrames == 60 )
		diag::info( "host clock at frame 60: raw=" + std::to_string( raw ) + " scale=" + std::to_string( clockScale )
		            + " seconds=" + std::to_string( now ) );
}

//---------------------------------------------------------------------------
FFResult NipkowPlugin::ProcessOpenGL( ProcessOpenGLStruct* pgl )
{
	if( pgl == nullptr || pgl->numInputTextures < 1 || pgl->inputTextures[ 0 ] == nullptr )
		return FF_FAIL;
	const FFGLTextureStruct* input = pgl->inputTextures[ 0 ];

	//The host's state, put back however this returns.
	ScopedGLState restore;
	restore.restoreOnExit     = !hooks.leakGLState;
	const GLint* hostViewport = restore.saved.viewport;
	glDisable( GL_BLEND );

	const int width  = static_cast< int >( input->Width );
	const int height = static_cast< int >( input->Height );
	if( width <= 0 || height <= 0 || vao == 0 )
		return FF_FAIL;

	UpdateClock();
	if( settledJump )
	{
		haveLast    = false;
		settledJump = false;
	}
	//The host's time in whole microseconds, and the interval as the
	//difference of two: summed over frames it telescopes to the host's own
	//elapsed time (no drift from rounding each frame), and a clock at
	//4.99e8 ms gives the same intervals as one from 0 (a frame is never near
	//a half microsecond at any common rate).
	const int64_t micros = std::llround( now * 1e6 );
	int64_t interval     = haveLast ? std::clamp< int64_t >( micros - lastMicros, 0, kMaxFrameMicros ) : 0;
	lastMicros           = micros;
	haveLast             = true;
	//A frame always runs a frame's worth: a host whose clock stands still
	//gets the last real frame's length, so the disc keeps turning.
	if( interval > 0 )
		intervalMicros = interval;

	if( !render( input, hostViewport, pgl->HostFBO, width, height, intervalMicros ) )
		return FF_FAIL;
	++frameIndex;
	return FF_SUCCESS;
}

//---------------------------------------------------------------------------
FFResult NipkowPlugin::DeInitGL()
{
	uniformCache.clear();
	for( FFGLShader* s : { &tableShader, &exposureShader, &resampleShader, &compositeShader } )
		s->FreeGLResources();
	if( vao != 0 )
		glDeleteVertexArrays( 1, &vao );
	vao = 0;
	if( sampleTexture != 0 )
		glDeleteTextures( 1, &sampleTexture );
	sampleTexture = 0;
	sampleRows    = 0;
	tableBuffer.Destroy();
	for( PassBuffer& b : exposure )
		b.Destroy();
	exposureWidth = exposureHeight = 0;
	return FF_SUCCESS;
}

//---------------------------------------------------------------------------
FFResult NipkowPlugin::SetTime( double time )
{
	hostTime = time;
	return FF_SUCCESS;
}

char* NipkowPlugin::GetTextParameter( unsigned int index )
{
	if( index == PT_ABOUT_TEXT )
	{
		static const std::string text = stoatworks::about::textParam( 0 );
		return const_cast< char* >( text.c_str() );
	}
	return CFFGLPlugin::GetTextParameter( index );
}

FFResult NipkowPlugin::SetTextParameter( unsigned int index, const char* value )
{
	if( index == PT_ABOUT_TEXT )
		return FF_SUCCESS;
	return CFFGLPlugin::SetTextParameter( index, value );
}

FFResult NipkowPlugin::SetFloatParameter( unsigned int index, float value )
{
	if( index >= PT_COUNT )
		return FF_FAIL;

	// The About buttons open a browser and store nothing.
	if( index >= PT_ABOUT_TEXT )
		return stoatworks::about::handleParam( index - PT_ABOUT_TEXT, value ) ? FF_SUCCESS : FF_FAIL;

	params[ index ] = value;
	return FF_SUCCESS;
}

float NipkowPlugin::GetFloatParameter( unsigned int index )
{
	return index < PT_COUNT ? params[ index ] : 0.0f;
}

} // namespace nipkow
