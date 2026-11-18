/*
    The six GL entry points emscripten's WebGL2 library cannot take from the
    plugin as they are: glShaderSource, glEnable, glDisable, glIsEnabled,
    glTexImage2D and glReadPixels. Every other GL call the plugin and the FFGL
    SDK make goes straight to emscripten's implementation, on the page's own
    WebGL2 context. schlieren's shim (zoetrope's and polyhedral's before it),
    with nipkow's names and one entry point more.

    glShaderSource
        The plugin hands GL desktop GLSL 4.10 (`#version 410 core`); WebGL2
        compiles GLSL ES 3.00. The page's `nipkowShaderSource` first REQUIRES
        the text to be byte for byte one of the page's own assemblies of the
        plugin's shaders (demo/shaders.js, which demo/tools/check_shaders.py
        holds to source/Shaders.cpp, and its ASSEMBLY table to
        AssembleVertex(), Assemble() and InitGL), then applies the demo kit's
        `port()`: the version line, the ES precision defaults and the removal
        of `precise`, nothing else -- and checks that it did exactly that
        (plugin.js, `shaderSource`). A shader the plugin assembled differently
        from the page's copy is refused (an empty source, so the plugin's
        InitGL fails and logs which pass), never compiled quietly.

    glEnable / glDisable / glIsEnabled
        GL_PROGRAM_POINT_SIZE is desktop-only state (in ES a point is always
        sized by the shader, and WebGL2 rejects the enum with INVALID_ENUM).
        The plugin's GLState.h saves and restores it on every frame, as a
        plugin inside Resolume must. Here it reads as off and enabling it is
        ignored; the plugin draws no points. Every other capability is passed
        through unchanged.

    glTexImage2D
        The transmitter's table and the screen's three exposure buffers are
        R32F render targets, allocated by the FFGL SDK's FFGLFBO::Initialise
        (PassBuffer's base) as glTexImage2D( ..., GL_R32F, ..., GL_RGBA,
        GL_FLOAT, NULL ). Desktop GL takes any client format with no data;
        OpenGL ES 3.0, and so WebGL2, takes exactly one per sized format
        (ES 3.0.6, table 3.2), and for R32F it is GL_RED -- with GL_RGBA it
        answers INVALID_OPERATION, the texture is never allocated, the
        framebuffer is incomplete and the plugin's PassBuffer::Ensure fails.
        So an allocation with NO data is given the client format ES pairs with
        its internal format. With no data there is nothing for a client format
        to describe, so nothing about what is allocated changes. Only the
        plugin's two no-data allocations are carried (R32F from the SDK;
        RGBA32F, the samples, from Nipkow.cpp, which already passes the ES
        pair and goes through as it is); anything else -- and any call that
        carries data -- aborts, by design, so a change in the plugin cannot be
        absorbed here quietly. The samples reach their texture through
        glTexSubImage2D, which is emscripten's own.

    glReadPixels
        The plugin reads the transmitter's table back every frame as
        glReadPixels( 0, 0, M, N, GL_RED, GL_FLOAT, table ) from its R32F
        framebuffer. WebGL2 accepts two format/type pairs from a float colour
        buffer: RGBA/FLOAT, always, and the one the implementation names in
        IMPLEMENTATION_COLOR_READ_FORMAT/_TYPE for the bound framebuffer.
        Where that is RED/FLOAT the call goes through as the plugin made it.
        Where it is not, the same rectangle is read as RGBA/FLOAT and its red
        channel copied into the plugin's buffer, one float to one float: the
        values are the framebuffer's, unchanged; only the layout they arrive
        in differs. Any other read-back aborts, as above. The page reports
        which path the browser took (data-readback on the canvas).

    Rendering into an R32F target needs EXT_color_buffer_float, which the page
    asks for and stops without; the samples (RGBA32F) and the exposure are
    read only by texelFetch, with nearest filtering, so no float filtering is
    needed.
*/
#include <GLES3/gl3.h>
#include <emscripten/emscripten.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
constexpr GLenum kProgramPointSize = 0x8642;///< GL_PROGRAM_POINT_SIZE, desktop GL 3.2

// clang-format off
EM_JS( void, nkShaderSource, ( GLuint shader, const char* source ), {
	const object = GL.shaders[ shader ];
	const stage  = GLctx.getShaderParameter( object, 0x8B4F /* GL_SHADER_TYPE */ ) === 0x8B31 ? 'vertex' : 'fragment';
	GLctx.shaderSource( object, Module[ 'nipkowShaderSource' ]( stage, UTF8ToString( source ) ) );
} );
EM_JS( void, nkEnable, ( GLenum cap ), { GLctx.enable( cap ); } );
EM_JS( void, nkDisable, ( GLenum cap ), { GLctx.disable( cap ); } );
EM_JS( int, nkIsEnabled, ( GLenum cap ), { return GLctx.isEnabled( cap ) ? 1 : 0; } );
EM_JS( void, nkTexImage2DEmpty, ( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type ), {
	GLctx.texImage2D( target, level, internalformat, width, height, border, format, type, null );
} );
// The red channel of a float framebuffer's rectangle into `out` (width x
// height floats, rows bottom first, as GL_PACK_ALIGNMENT 4 and row length 0
// lay out GL_RED floats).
EM_JS( void, nkReadRedFloat, ( GLint x, GLint y, GLsizei width, GLsizei height, float* out ), {
	const n      = width * height;
	const dst    = HEAPF32.subarray( out >> 2, ( out >> 2 ) + n );
	const format = GLctx.getParameter( 0x8B9B /* GL_IMPLEMENTATION_COLOR_READ_FORMAT */ );
	const type   = GLctx.getParameter( 0x8B9A /* GL_IMPLEMENTATION_COLOR_READ_TYPE */ );
	if( format === 0x1903 /* GL_RED */ && type === 0x1406 /* GL_FLOAT */ )
	{
		GLctx.readPixels( x, y, width, height, 0x1903, 0x1406, dst );
		Module[ 'nipkowReadback' ] = 'RED/FLOAT';
		return;
	}
	const rgba = new Float32Array( 4 * n );
	GLctx.readPixels( x, y, width, height, 0x1908 /* GL_RGBA */, 0x1406, rgba );
	for( let i = 0; i < n; ++i )
		dst[ i ] = rgba[ 4 * i ];
	Module[ 'nipkowReadback' ] = 'RGBA/FLOAT, red taken';
} );
// clang-format on

[[noreturn]] void refuse( const char* call, const char* why, GLint internalformat, GLenum format, GLenum type )
{
	std::fprintf( stderr, "nipkow demo: %s( internal 0x%x, format 0x%x, type 0x%x ) %s -- not a call this page's GL shim carries\n", call,
	              static_cast< unsigned >( internalformat ), format, type, why );
	std::abort();
}
} // namespace

extern "C"
{
void glShaderSource( GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length )
{
	std::string source;
	for( GLsizei i = 0; i < count; ++i )
	{
		if( string[ i ] == nullptr )
			continue;
		const size_t n = ( length != nullptr && length[ i ] >= 0 ) ? static_cast< size_t >( length[ i ] ) : std::strlen( string[ i ] );
		source.append( string[ i ], n );
	}
	nkShaderSource( shader, source.c_str() );
}

void glEnable( GLenum cap )
{
	if( cap != kProgramPointSize )
		nkEnable( cap );
}

void glDisable( GLenum cap )
{
	if( cap != kProgramPointSize )
		nkDisable( cap );
}

GLboolean glIsEnabled( GLenum cap )
{
	if( cap == kProgramPointSize )
		return GL_FALSE;
	return nkIsEnabled( cap ) ? GL_TRUE : GL_FALSE;
}

void glTexImage2D( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format,
                   GLenum type, const void* pixels )
{
	if( target != GL_TEXTURE_2D || level != 0 )
		refuse( "glTexImage2D", "is not a level-0 2D texture", internalformat, format, type );
	if( pixels != nullptr )
		refuse( "glTexImage2D", "carries data", internalformat, format, type );
	if( type != GL_FLOAT || format != GL_RGBA )
		refuse( "glTexImage2D", "is not the plugin's GL_RGBA / GL_FLOAT", internalformat, format, type );

	//The one pair ES 3.0 accepts for each sized float format, with no data.
	GLenum esFormat = 0;
	switch( internalformat )
	{
	case GL_R32F: esFormat = GL_RED; break;
	case GL_RGBA32F: esFormat = GL_RGBA; break;
	default: refuse( "glTexImage2D", "is not one of the plugin's two float formats", internalformat, format, type );
	}
	nkTexImage2DEmpty( target, level, internalformat, width, height, border, esFormat, GL_FLOAT );
}

void glReadPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels )
{
	if( format != GL_RED || type != GL_FLOAT || pixels == nullptr )
		refuse( "glReadPixels", "is not the table's GL_RED / GL_FLOAT read-back", 0, format, type );
	nkReadRedFloat( x, y, width, height, static_cast< float* >( pixels ) );
}
} // extern "C"
