// The checks. Included into main.cpp's anonymous namespace; see main.cpp for
// the Rig and the pictures.

constexpr double hPi = 3.14159265358979323846;

/// The larger of two errors, NaN winning: std::max( worst, NaN ) keeps
/// worst, so a model that produced NaN would pass every "worst <= tolerance"
/// (a mutant that divides by alpha did exactly that).
double worse( double a, double b )
{
	return ( a != a || b != b ) ? std::numeric_limits< double >::quiet_NaN() : std::max( a, b );
}
template< typename... T >
double worse( double a, double b, T... rest )
{
	return worse( worse( a, b ), rest... );
}


void defaults( Rig& rig )
{
	rig.Set( PT_LINES, 30.0f );
	rig.Set( PT_RATE, ParamFromRate( 12.5 ) );
	rig.Set( PT_SCANNER, static_cast< int >( kDrum ) );
	rig.Set( PT_BANDWIDTH, ParamFromBandwidth( 9000.0 ) );
	rig.Set( PT_LAMP, ParamFromLamp( 20.0 ) );
	rig.Set( PT_SYNC, static_cast< int >( kLocked ) );
	rig.Set( PT_SPEED, ParamFromSpeed( 0.0 ) );
	rig.Set( PT_FRAMING, ParamFromFraming( 0.0 ) );
	rig.Set( PT_PERSISTENCE, ParamFromPersistence( 0.1 ) );
	rig.Set( PT_FRAME, static_cast< int >( kBaird ) );
	rig.Set( PT_MIX, 1.0f );
}

bool startRig( Rig& rig, int w, int h, const Perturb& perturb, const Floats* picture = nullptr, double fps = 60.0 )
{
	rig.fps = fps;
	if( !rig.Init( w, h, picture ) )
		return false;
	rig.plugin.HooksForTest() = perturb;
	defaults( rig );
	return true;
}

Floats grey( int w, int h, double code, double alpha = 1.0 )
{
	return flatCard( w, h, static_cast< float >( code ), static_cast< float >( code ), static_cast< float >( code ), static_cast< float >( alpha ) );
}

//===========================================================================
// --glstate
//===========================================================================
int runGLState( const Perturb& perturb )
{
	std::printf( "\n=== glstate: the host's GL state comes back as it was handed over\n" );
	for( const Raster& raster : kRasters )
	{
		Rig rig;
		if( !startRig( rig, raster.w, raster.h, perturb ) )
			return 1;
		GLuint vao = 0, buffer = 0;
		glGenVertexArrays( 1, &vao );
		glGenBuffers( 1, &buffer );
		int wrong = 0;
		for( int frame = 0; frame < 3; ++frame )
		{
			glBindFramebuffer( GL_FRAMEBUFFER, rig.outputFBO );
			glViewport( 3, 5, raster.w - 7, raster.h - 9 );
			glEnable( GL_BLEND );
			glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ZERO );
			glClearColor( 0.1f, 0.2f, 0.3f, 0.4f );
			glBindVertexArray( vao );
			glBindBuffer( GL_ARRAY_BUFFER, buffer );
			glActiveTexture( GL_TEXTURE3 );
			rig.plugin.SetTime( rig.Seconds( frame ) );
			if( rig.plugin.ProcessOpenGL( &rig.process ) != FF_SUCCESS )
				return 1;
			GLint vp[ 4 ], src = 0, dst = 0, srcA = 0, dstA = 0, va = 0, ab = 0, at = 0;
			GLfloat cc[ 4 ];
			glGetIntegerv( GL_VIEWPORT, vp );
			glGetIntegerv( GL_BLEND_SRC_RGB, &src );
			glGetIntegerv( GL_BLEND_DST_RGB, &dst );
			glGetIntegerv( GL_BLEND_SRC_ALPHA, &srcA );
			glGetIntegerv( GL_BLEND_DST_ALPHA, &dstA );
			glGetFloatv( GL_COLOR_CLEAR_VALUE, cc );
			glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &va );
			glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &ab );
			glGetIntegerv( GL_ACTIVE_TEXTURE, &at );
			wrong += vp[ 0 ] != 3 || vp[ 1 ] != 5 || vp[ 2 ] != raster.w - 7 || vp[ 3 ] != raster.h - 9;
			wrong += !glIsEnabled( GL_BLEND ) || src != GL_SRC_ALPHA || dst != GL_ONE || srcA != GL_ONE || dstA != GL_ZERO;
			wrong += cc[ 0 ] != 0.1f || cc[ 1 ] != 0.2f || cc[ 2 ] != 0.3f || cc[ 3 ] != 0.4f;
			wrong += va != static_cast< GLint >( vao ) || ab != static_cast< GLint >( buffer ) || at != GL_TEXTURE3;
			wrong += glGetError() != GL_NO_ERROR;
		}
		glBindVertexArray( 0 );
		glDisable( GL_BLEND );
		glActiveTexture( GL_TEXTURE0 );
		glDeleteVertexArrays( 1, &vao );
		glDeleteBuffers( 1, &buffer );
		Check( wrong == 0, fmt( "%4dx%-4d three frames with a host viewport, blend, clear colour, vertex array, buffer and texture unit of its own: %d "
		                        "things not put back, GL errors included",
		                        raster.w, raster.h, wrong ) );
	}
	return Verdict();
}


//===========================================================================
// The registry.
//===========================================================================
struct CheckEntry
{
	const char* flag;
	CheckFn run;
	bool offline;///< needs no GL context
};

const std::vector< CheckEntry >& checks()
{
	static const std::vector< CheckEntry > list = {
		{ "glstate", runGLState, false },
	};
	return list;
}

bool isOffline( const std::string& flag )
{
	for( const CheckEntry& c : checks() )
		if( flag == c.flag )
			return c.offline;
	return false;
}

int runNegative( bool offlineOnly = false )
{
	( void )offlineOnly;
	return 0;
}

int runOffline()
{
	return 0;
}

int runBench()
{
	return 0;
}
