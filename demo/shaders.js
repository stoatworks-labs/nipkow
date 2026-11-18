// GENERATED from source/Shaders.cpp by demo/tools/check_shaders.py --write.
// Do not edit: tools/verify.sh fails if a character of this differs from the
// plugin's. The one escape is \` for a backtick inside a comment.
export const VERSION = '#version 410 core\n';

// kQuadVertex, source/Shaders.cpp
export const QUAD_VERTEX = `
void main()
{
	vec2 p = vec2( float( ( gl_VertexID << 1 ) & 2 ), float( gl_VertexID & 2 ) );
	gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 );
}
`;

// kCommon, source/Shaders.cpp
export const COMMON = `
vec3 toLinear( vec3 c )
{
	c       = max( c, vec3( 0.0 ) );
	vec3 lo = c / 12.92;
	vec3 hi = pow( ( c + 0.055 ) / 1.055, vec3( 2.4 ) );
	return mix( lo, hi, step( vec3( 0.04045 ), c ) );
}
vec3 toCode( vec3 c )
{
	c       = clamp( c, 0.0, 1.0 );
	vec3 lo = c * 12.92;
	vec3 hi = 1.055 * pow( c, vec3( 1.0 / 2.4 ) ) - 0.055;
	return mix( lo, hi, step( vec3( 0.0031308 ), c ) );
}
`;

// kTableFragment, source/Shaders.cpp
export const TABLE_FRAGMENT = `
uniform sampler2D InputTexture;
uniform ivec2 ClipSize;
uniform int Lines;
uniform int SamplesLine;
uniform int Scanner;// 0 the drum; 1 the disc
uniform vec2 Centre;// the field's centre, pixels
uniform vec2 Scale; // pixels a hole, each axis
uniform float Rc;
uniform float Phi;
uniform int Premultiply;// 1 the plugin; 0 the negative control
out vec4 FragColor;

float lightAt( ivec2 p )
{
	vec4 src  = texelFetch( InputTexture, p, 0 );
	float lum = dot( toLinear( src.rgb ), vec3( 0.2126, 0.7152, 0.0722 ) );
	return Premultiply == 1 ? lum * clamp( src.a, 0.0, 1.0 ) : lum;
}

// sin to a float's precision over the field: Apple's software renderer's
// sin is off by up to 1.1e-3 for |x| <= 0.4 (measured; the GPU's 6e-8),
// which bends the disc's arcs by 1% there. Here |x| <= pi / 8 (half the
// field at 8 lines), so the series to x^7 serves: the first term left out,
// x^9 / 9!, is 6e-10.
float sine( float x )
{
	float q = x * x;
	return x * ( 1.0 - q / 6.0 * ( 1.0 - q / 20.0 * ( 1.0 - q / 42.0 ) ) );
}

void main()
{
	int j   = int( gl_FragCoord.x );
	int k   = int( gl_FragCoord.y );
	float s = ( float( j ) + 0.5 ) / float( SamplesLine );
	float n = float( Lines );
	vec2 offset;
	if( Scanner == 0 )
		offset = vec2( 0.5 * n - ( float( k ) + 0.5 ), ( s - 0.5 ) * 7.0 * n / 3.0 );
	else
	{
		float d   = 0.5 * ( n - 1.0 ) - float( k );
		float rk  = Rc + d;
		float phi = ( s - 0.5 ) * Phi;
		float h   = sine( 0.5 * phi );
		offset    = vec2( d - 2.0 * rk * h * h, rk * sine( phi ) );
	}
	vec2 centre = Centre + offset * Scale;
	vec2 lo     = centre - 0.5 * Scale;
	vec2 hi     = centre + 0.5 * Scale;
	ivec2 i0    = max( ivec2( floor( lo ) ), ivec2( 0 ) );
	ivec2 i1    = min( ivec2( ceil( hi ) ) - 1, ClipSize - 1 );
	ivec2 stride = max( ( i1 - i0 + 64 ) / 64, ivec2( 1 ) );
	float total = 0.0;
	for( int y = i0.y; y <= i1.y; y += stride.y )
	{
		float oy = min( hi.y, float( y + stride.y ) ) - max( lo.y, float( y ) );
		if( oy <= 0.0 )
			continue;
		for( int x = i0.x; x <= i1.x; x += stride.x )
		{
			float ox = min( hi.x, float( x + stride.x ) ) - max( lo.x, float( x ) );
			if( ox > 0.0 )
				total += ox * oy * lightAt( ivec2( x, y ) );
		}
	}
	FragColor = vec4( total / ( Scale.x * Scale.y ), 0.0, 0.0, 0.0 );
}
`;

// kExposureFragment, source/Shaders.cpp
export const EXPOSURE_FRAGMENT = `
uniform sampler2D Samples;// r u, g light, b glow, a u's remainder; sample i at ( i & 4095, i >> 12 )
uniform sampler2D Previous;
uniform int Count;
uniform float FirstU;
uniform float LastU;
uniform float Decay;
uniform float SampleSeconds;
uniform int Lines;
uniform float Rc;
uniform float Phi;
uniform vec2 Centre;       // the field's centre, pixels
uniform vec2 UnitsPerPixel;// holes a pixel, each axis
uniform float HoleSpacing; // 1 the disc's spiral; the negative controls' 1.02 or 0
uniform int HoleWidthModel;// 0 the wedge, 1 / r_k; 1 one angular width
uniform int ScanModel;     // 0 every scan in the frame; 1 at most one
out vec4 FragColor;

vec4 sampleAt( int i )
{
	return texelFetch( Samples, ivec2( i & 4095, i >> 12 ), 0 );
}

// The integral of clamp( w, 0, 2a ) from -inf to v.
float ramp( float v, float a )
{
	if( v <= 0.0 )
		return 0.0;
	if( v <= 2.0 * a )
		return 0.5 * v * v;
	return 2.0 * a * ( v - a );
}
// The integral, over the hole's centre from -inf to x, of the length of
// [ x - a, x + a ] inside [ -L, L ]: x relative to the window's middle.
float swept( float x, float a, float L )
{
	return ramp( x + a + L, a ) - ramp( x + a - L, a );
}

// The angle of ( X, Y ), X > 0, to a float's precision over the field:
// Apple's software renderer's atan is off by up to 6e-6 rad, 3e-5 of the
// field's half-angle. Half the angle, t = tan( phi / 2 ) = Y / ( r + X ), and
// atan's series to t^11 (|t| < 0.25 covers the widest field, pi / 8 either
// side; the first term left out, t^13 / 13, is 1e-9). Beyond that the pixel
// is outside the field and the built-in serves.
float angleOf( float X, float Y, float r )
{
	float t = Y / ( r + X );
	if( X <= 0.0 || abs( t ) >= 0.25 )
		return atan( Y, X );
	float q = t * t;
	return 2.0 * t * ( 1.0 + q * ( -1.0 / 3.0 + q * ( 1.0 / 5.0 + q * ( -1.0 / 7.0 + q * ( 1.0 / 9.0 - q / 11.0 ) ) ) ) );
}

void main()
{
	ivec2 at       = ivec2( gl_FragCoord.xy );
	float previous = texelFetch( Previous, at, 0 ).r * Decay;
	vec2 d         = ( gl_FragCoord.xy - Centre ) * UnitsPerPixel;
	float X        = Rc + d.x;
	float Y        = d.y;
	float r        = sqrt( X * X + Y * Y );
	float phi      = angleOf( X, Y, r );
	float n        = float( Lines );
	float c        = X / r;
	float s        = Y / r;
	// The footprint: exact at phi = 0 and pi / 2, the pixel's area kept.
	float wr        = exp( c * c * log( UnitsPerPixel.x ) + s * s * log( UnitsPerPixel.y ) );
	float wt        = UnitsPerPixel.x * UnitsPerPixel.y / wr;
	float hr        = 0.5 * wr;
	float hphi      = 0.5 * wt / r;
	float halfField = 0.5 * Phi;
	// The window relative to the pixel's angle: phi ~ 0.1 rounds to 7e-9,
	// 5e-5 of hphi, and phi + hphi less phi - hphi would carry it.
	float wlo = max( -hphi, -halfField - phi );
	float whi = min( hphi, halfField - phi );
	float lo  = phi + wlo;
	float hi  = phi + whi;
	if( whi <= wlo || Count <= 0 )
	{
		FragColor = vec4( previous, 0.0, 0.0, 0.0 );
		return;
	}
	int k0 = 0;
	int k1 = Lines - 1;
	if( HoleSpacing > 0.0 )
	{
		float kf  = 0.5 * ( n - 1.0 ) - ( r - Rc ) / HoleSpacing;
		float ext = ( hr + 0.5 ) / HoleSpacing;
		k0        = max( 0, int( ceil( kf - ext ) ) );
		k1        = min( Lines - 1, int( floor( kf + ext ) ) );
	}
	float sum = 0.0;
	for( int k = k0; k <= k1; ++k )
	{
		// Radii relative to the hole's: at r ~ 350 a float resolves 3e-5,
		// and r + hr less r - hr would round 2 hr the same way at every pixel.
		float rk  = Rc + HoleSpacing * ( 0.5 * ( n - 1.0 ) - float( k ) );
		precise float rel = r - rk;
		precise float rlo = max( rel - hr, -0.5 );
		precise float rhi = min( rel + hr, 0.5 );
		if( rhi <= rlo )
			continue;
		float a   = HoleWidthModel == 1 ? 0.5 / Rc : 0.5 / rk;
		float off = float( k ) + 0.5;
		int mlo   = int( ceil( ( FirstU - off - ( hi + a ) / Phi ) / n ) );
		int mhi   = int( floor( ( LastU - off - ( lo - a ) / Phi ) / n ) );
		for( int m = mlo; m <= mhi && m <= mlo + 8; ++m )
		{
			float start = off + float( m ) * n;
			float ulo   = start + ( lo - a ) / Phi;
			float uhi   = start + ( hi + a ) / Phi;
			// The last sample that starts at or before ulo.
			int first = 0;
			if( sampleAt( 0 ).r <= ulo )
			{
				int top = Count - 1;
				while( first < top )
				{
					int mid = ( first + top + 1 ) >> 1;
					if( sampleAt( mid ).r <= ulo )
						first = mid;
					else
						top = mid - 1;
				}
			}
			float got = 0.0;
			vec4 here = sampleAt( first );
			for( int i = first; i < Count && i < first + 65536; ++i )
			{
				if( here.r >= uhi )
					break;
				vec4 next = sampleAt( i + 1 );
				if( here.g > 0.0 && next.r > here.r )
				{
					float clo = wlo;
					float chi = whi;
					float rl  = rlo;
					float rh  = rhi;
					if( here.b < 1.0 )
					{
						float q = sqrt( max( here.b, 0.0 ) );
						clo     = max( clo, -q * halfField - phi );
						chi     = min( chi, q * halfField - phi );
						rl      = max( rl, Rc - rk - q * 0.5 * n );
						rh      = min( rh, Rc - rk + q * 0.5 * n );
					}
					if( chi > clo && rh > rl )
					{
						float mid  = phi + 0.5 * ( clo + chi );
						float L    = 0.5 * ( chi - clo );
						// u in two floats: near the hole's start the difference
						// is exact, and the remainder carries what a float of
						// u ~ 30 drops (2e-6 of a line, 5e-4 of a sample).
						// precise: Apple's compiler otherwise reassociates
						// ( u - start ) + rest into ( u + rest ) - start and
						// the remainder is lost.
						precise float da = here.r - start;
						precise float db = next.r - start;
						precise float ua = da + here.a;
						precise float ub = db + next.a;
						float pa   = ua * Phi - mid;
						float pb   = ub * Phi - mid;
						float over = swept( pb, a, L ) - swept( pa, a, L );
						got += here.g * ( rh - rl ) * over / ( pb - pa );
					}
				}
				here = next;
			}
			if( got > 0.0 )
			{
				sum += got;
				if( ScanModel == 1 )
					break;
			}
		}
	}
	float exposure = sum * SampleSeconds / ( 4.0 * hr * hphi );
	FragColor      = vec4( previous + exposure, 0.0, 0.0, 0.0 );
}
`;

// kResampleFragment, source/Shaders.cpp
export const RESAMPLE_FRAGMENT = `
uniform sampler2D OldStore;
uniform ivec2 OldSize;
uniform vec2 Ratio;// old pixels per new pixel, each axis
out vec4 FragColor;
void main()
{
	vec2 p   = floor( gl_FragCoord.xy );
	vec2 lo  = p * Ratio;
	vec2 hi  = ( p + 1.0 ) * Ratio;
	ivec2 i0 = clamp( ivec2( floor( lo ) ), ivec2( 0 ), OldSize - 1 );
	ivec2 i1 = clamp( ivec2( ceil( hi ) ) - 1, ivec2( 0 ), OldSize - 1 );
	i1       = min( i1, i0 + 63 );
	float total = 0.0;
	for( int y = i0.y; y <= i1.y; ++y )
		for( int x = i0.x; x <= i1.x; ++x )
		{
			float ox = min( hi.x, float( x + 1 ) ) - max( lo.x, float( x ) );
			float oy = min( hi.y, float( y + 1 ) ) - max( lo.y, float( y ) );
			total += max( ox, 0.0 ) * max( oy, 0.0 ) * texelFetch( OldStore, ivec2( x, y ), 0 ).r;
		}
	FragColor = vec4( total / ( Ratio.x * Ratio.y ), 0.0, 0.0, 0.0 );
}
`;

// kCompositeFragment, source/Shaders.cpp
export const COMPOSITE_FRAGMENT = `
uniform sampler2D InputTexture;
uniform sampler2D Exposure;
uniform ivec2 ViewOrigin;
uniform float DisplayScale;
uniform vec3 NeonRGB;
uniform float MixAmount;
uniform int AlphaModel;// 0 the plugin; 1 the clip's alpha passed through
out vec4 FragColor;
void main()
{
	ivec2 at = ivec2( gl_FragCoord.xy ) - ViewOrigin;
	vec4 src = texelFetch( InputTexture, at, 0 );
	if( MixAmount <= 0.0 )
	{
		FragColor = src;
		return;
	}
	float shown = texelFetch( Exposure, at, 0 ).r * DisplayScale;
	vec3 rgb    = toCode( shown * NeonRGB );
	float outA  = AlphaModel == 1 ? src.a : 1.0;
	FragColor   = vec4( mix( src.rgb, rgb, MixAmount ), mix( src.a, outA, MixAmount ) );
}
`;

// Each stage's source, piece by piece, in the order the plugin joins them:
// every pass as Nipkow.cpp's InitGL compiles it, its vertex stage from
// Shaders.cpp's AssembleVertex() and its fragment stage from Assemble().
export const ASSEMBLY = {
  table_vertex: ['VERSION', 'QUAD_VERTEX'],
  table_fragment: ['VERSION', 'COMMON', 'TABLE_FRAGMENT'],
  exposure_vertex: ['VERSION', 'QUAD_VERTEX'],
  exposure_fragment: ['VERSION', 'EXPOSURE_FRAGMENT'],
  resample_vertex: ['VERSION', 'QUAD_VERTEX'],
  resample_fragment: ['VERSION', 'RESAMPLE_FRAGMENT'],
  composite_vertex: ['VERSION', 'QUAD_VERTEX'],
  composite_fragment: ['VERSION', 'COMMON', 'COMPOSITE_FRAGMENT'],
};

// The variables each stage declares `precise`: the kit's port() removes the
// word (GLSL ES 3.00 has none), and the page checks at load that it removed
// exactly these and changed nothing else (plugin.js, shaderSource).
export const PRECISE = {
  table_vertex: [],
  table_fragment: [],
  exposure_vertex: [],
  exposure_fragment: ['rel', 'rlo', 'rhi', 'da', 'db', 'ua', 'ub'],
  resample_vertex: [],
  resample_fragment: [],
  composite_vertex: [],
  composite_fragment: [],
};
