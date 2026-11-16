// The checks. Included into main.cpp's anonymous namespace; see main.cpp for
// the Rig and the pictures.

//===========================================================================
// The harness's own physics, restated from SPEC-nipkow.md and not from the
// plugin's source. Every constant is given again with its source.
//===========================================================================
constexpr double hPi       = 3.14159265358979323846;
constexpr double hSwing    = 10.0; ///< mA either side of the standing current (Television, Feb 1930 p. 598)
constexpr double hCover    = 5.0;  ///< mA, the glow covers the plate (Raytheon 1928)
constexpr double hStrike   = ( 170.0 - 140.0 ) / 10e3 * 1e3;///< mA: (V_b - V_m) / R (M&BC p. 73; Collins pp. 148-149; R chosen)
constexpr double hLag0     = 300e-6;///< s just above breakdown (GE p. 4)
constexpr double hLag65    = 25e-6; ///< s at 65 V over (GE p. 4)
constexpr double hLagFloor = 10e-6; ///< s (Signalite)
constexpr double hTauM     = 1.0;   ///< s, the motor (chosen)
constexpr double hHunt     = 1.5;   ///< Hz, the hunting at the reference pull (chosen)
constexpr double hLref     = 20.0;  ///< mA, the display's reference
constexpr int hSync        = 4;     ///< samples: the last picture element of every line

/// The larger of two errors, NaN winning: std::max( worst, NaN ) keeps
/// worst, so a model that produced NaN would pass every "worst <= tolerance".
double worse( double a, double b )
{
	return ( a != a || b != b ) ? std::numeric_limits< double >::quiet_NaN() : std::max( a, b );
}
template< typename... T >
double worse( double a, double b, T... rest )
{
	return worse( worse( a, b ), rest... );
}
/// |x| that keeps NaN.
double mag( double x )
{
	return x != x ? x : std::fabs( x );
}

double hRc( int n )
{
	return 7.0 * n * n / ( 6.0 * hPi );
}
double hPhi( int n )
{
	return 2.0 * hPi / n;
}
double hRk( int n, int k )
{
	return hRc( n ) + 0.5 * ( n - 1 ) - k;
}
int hM( int n )
{
	return 4 * ( ( 7 * n + 2 ) / 3 );
}
double hLag( double overvolts )
{
	if( overvolts <= 0.0 )
		return std::numeric_limits< double >::infinity();
	return std::max( hLagFloor, hLag0 * std::pow( hLag65 / hLag0, overvolts / 65.0 ) );
}
/// The controls' conversions, restated: geometric or linear over the spec's ranges.
double hRate( float v )
{
	return 2.0 * std::pow( 15.0, std::clamp( static_cast< double >( v ), 0.0, 1.0 ) );
}
double hBandwidth( float v )
{
	return 500.0 * std::pow( 80.0, std::clamp( static_cast< double >( v ), 0.0, 1.0 ) );
}
double hLamp( float v )
{
	return 30.0 * std::clamp( static_cast< double >( v ), 0.0, 1.0 );
}
double hSpeed( float v )
{
	return -0.05 + 0.1 * std::clamp( static_cast< double >( v ), 0.0, 1.0 );
}
double hFraming( float v )
{
	return -0.5 + std::clamp( static_cast< double >( v ), 0.0, 1.0 );
}
double hPersistence( float v )
{
	return 0.02 * std::pow( 50.0, std::clamp( static_cast< double >( v ), 0.0, 1.0 ) );
}

/// Where the field sits: the whole sector to the height (Baird), or the
/// nominal N x 7N/3 rectangle to the frame (Fill); its centre at the frame's.
struct HGeo
{
	int n = 30, w = 0, h = 0;
	double rc = 0, phi = 0, sx = 0, sy = 0, cx = 0, cy = 0;
	void polar( int x, int y, double& r, double& a ) const
	{
		const double X = rc + ( x + 0.5 - cx ) / sx, Y = ( y + 0.5 - cy ) / sy;
		r = std::hypot( X, Y );
		a = std::atan2( Y, X );
	}
	/// The pixel square's half diagonal and its tangential half-width at r, holes.
	double halfDiagonal() const
	{
		return 0.5 * std::hypot( 1.0 / sx, 1.0 / sy );
	}
	double halfAngle( double r ) const
	{
		return 0.5 / ( sy * r );
	}
};
HGeo hGeo( int w, int h, int n, bool fill = false )
{
	HGeo g;
	g.n   = n;
	g.w   = w;
	g.h   = h;
	g.rc  = hRc( n );
	g.phi = hPhi( n );
	if( fill )
	{
		g.sx = static_cast< double >( w ) / n;
		g.sy = h / ( 7.0 * n / 3.0 );
	}
	else
		g.sx = g.sy = h / ( 2.0 * ( g.rc + 0.5 * n ) * std::sin( 0.5 * g.phi ) );
	g.cx = 0.5 * w;
	g.cy = 0.5 * h;
	return g;
}

/// Samples processed by host time `micros` (whole microseconds since the
/// first frame's start): floor( us x N f M / 1e6 ).
int64_t hSamplesBy( int64_t micros, int n, double rate )
{
	return static_cast< int64_t >( std::floor( static_cast< double >( micros ) * ( static_cast< double >( n ) * hM( n ) * rate ) / 1e6 ) );
}
/// The microseconds the plugin has run by the end of frame f on the rig's
/// clock (frame 0 runs the default 1/60 s; each later frame the difference
/// of the host's times in whole microseconds, at most 0.1 s).
int64_t hMicrosBy( int f, double fps )
{
	int64_t total = 16667;
	for( int i = 1; i <= f; ++i )
	{
		const int64_t a = std::llround( ( i - 1 ) / fps * 1e6 ), b = std::llround( i / fps * 1e6 );
		total += std::min< int64_t >( b - a, 100000 );
	}
	return total;
}

//---------------------------------------------------------------------------
// Pictures, built in linear light and stored as code values: a pixel partly
// covered gets its exact share, so the transmitter's aperture sees the
// geometry and not the raster.
//---------------------------------------------------------------------------
struct Linear
{
	int w, h;
	std::vector< double > v;
	Linear( int w_, int h_, double value = 0.0 )
	    : w( w_ ), h( h_ ), v( static_cast< size_t >( w_ ) * h_, value )
	{
	}
	void rect( double x0, double x1, double y0, double y1, double value )
	{
		for( int y = std::max( 0, static_cast< int >( std::floor( y0 ) ) ); y < std::min( h, static_cast< int >( std::ceil( y1 ) ) ); ++y )
			for( int x = std::max( 0, static_cast< int >( std::floor( x0 ) ) ); x < std::min( w, static_cast< int >( std::ceil( x1 ) ) ); ++x )
			{
				const double ox = std::min( x1, x + 1.0 ) - std::max( x0, static_cast< double >( x ) );
				const double oy = std::min( y1, y + 1.0 ) - std::max( y0, static_cast< double >( y ) );
				if( ox > 0.0 && oy > 0.0 )
					v[ static_cast< size_t >( y ) * w + x ] += ox * oy * value;
			}
	}
	Floats card( double alpha = 1.0 ) const
	{
		Floats c( v.size() * 4 );
		for( size_t i = 0; i < v.size(); ++i )
		{
			const float code = static_cast< float >( codeOf( std::clamp( v[ i ], 0.0, 1.0 ) ) );
			c[ 4 * i ] = c[ 4 * i + 1 ] = c[ 4 * i + 2 ] = code;
			c[ 4 * i + 3 ]                              = static_cast< float >( alpha );
		}
		return c;
	}
};

/// The drum's line k explores x in [ cx + sx ( N/2 - k - 1 ), cx + sx ( N/2 - k ) ].
void drumStripe( const HGeo& g, int k, double& x0, double& x1 )
{
	x0 = g.cx + g.sx * ( 0.5 * g.n - k - 1 );
	x1 = g.cx + g.sx * ( 0.5 * g.n - k );
}
/// The whole pixels inside line k's strip. A pixel the strip's edge cuts
/// holds its light spread over the whole pixel, so the neighbouring line's
/// aperture would see a share of it: a picture meant for one line keeps to
/// the pixels wholly inside it.
void drumPixels( const HGeo& g, int k, double& x0, double& x1 )
{
	drumStripe( g, k, x0, x1 );
	x0 = std::ceil( x0 );
	x1 = std::floor( x1 );
}

//---------------------------------------------------------------------------
// The rig.
//---------------------------------------------------------------------------
void defaults( Rig& rig )
{
	rig.Set( PT_LINES, 30.0f );
	rig.Set( PT_RATE, ParamFromRate( 12.5 ) );
	rig.Set( PT_SCANNER, static_cast< int >( kDrum ) );
	rig.Set( PT_BANDWIDTH, ParamFromBandwidth( 40000.0 ) );
	rig.Set( PT_LAMP, ParamFromLamp( 15.0 ) );
	rig.Set( PT_SYNC, static_cast< int >( kLocked ) );
	rig.Set( PT_SPEED, ParamFromSpeed( 0.0 ) );
	rig.Set( PT_FRAMING, ParamFromFraming( 0.0 ) );
	rig.Set( PT_PERSISTENCE, ParamFromPersistence( 1.0 ) );
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

/// The last frame's own light: A_n - decay x A_{n-1}, with the decay the
/// plugin applied (the frame's samples' span over Persistence).
std::vector< double > frameLight( const std::vector< double >& now, const std::vector< double >& before, double decay )
{
	std::vector< double > e( now.size() );
	for( size_t i = 0; i < now.size(); ++i )
		e[ i ] = now[ i ] - static_cast< double >( static_cast< float >( decay ) ) * before[ i ];
	return e;
}

/// Light-weighted moments of a set of pixels.
struct Moments
{
	double w = 0, m1 = 0, m2 = 0;
	void add( double weight, double x )
	{
		w += weight;
		m1 += weight * x;
		m2 += weight * x * x;
	}
	double mean() const
	{
		return m1 / w;
	}
	double variance() const
	{
		const double m = mean();
		return m2 / w - m * m;
	}
};

//===========================================================================
// --spiral
//===========================================================================
int runSpiral( const Perturb& perturb )
{
	std::printf( "\n=== spiral: the first frame's light, alternate lines white (the lines 0..6 the frame scans): each lit line's light-weighted radius\n"
	             "    against r_k + 1/(12 r_k), and the scanning hole's angle at the frame's end against ( u_b - k - 1/2 ) Phi\n" );
	const int n = 30;
	for( const Raster& raster : kRasters )
	{
		const HGeo g = hGeo( raster.w, raster.h, n );
		Linear stripes( raster.w, raster.h );
		for( int k = 0; k < n; k += 2 )
		{
			double x0, x1;
			drumPixels( g, k, x0, x1 );
			stripes.rect( x0, x1, 0.0, raster.h, 1.0 );
		}
		const Floats lit = stripes.card(), dark = grey( raster.w, raster.h, 0.0 );
		Rig a, b;
		if( !startRig( a, raster.w, raster.h, perturb, &lit ) || !startRig( b, raster.w, raster.h, perturb, &dark ) )
			return 1;
		if( !a.Render( 1 ) || !b.Render( 1 ) )
			return 1;
		const std::vector< double > ea = a.Exposure(), eb = b.Exposure();
		std::vector< double > d( ea.size() );
		for( size_t i = 0; i < d.size(); ++i )
			d[ i ] = ea[ i ] - eb[ i ];

		//The radius of each fully lit white line.
		const double margin = g.halfDiagonal();
		double worstR       = 0.0;
		std::string radii;
		for( int k = 0; k <= 4; k += 2 )
		{
			Moments m;
			const double rk = hRk( n, k );
			for( int y = 0; y < raster.h; ++y )
				for( int x = 0; x < raster.w; ++x )
				{
					double r, phi;
					g.polar( x, y, r, phi );
					if( std::fabs( r - rk ) <= 0.5 + margin )
						m.add( d[ static_cast< size_t >( y ) * raster.w + x ], r );
				}
			const double err = ( m.mean() - ( rk + 1.0 / ( 12.0 * rk ) ) ) * g.sx;
			worstR           = worse( worstR, mag( err ) );
			radii += fmt( " k=%d %+.4f px", k, err );
		}

		//The hole at the frame's end: the first frame runs 1/60 s (16667 us).
		const int64_t samples = hSamplesBy( 16667, n, 12.5 );
		const double ub       = static_cast< double >( samples ) / hM( n );
		const int k           = static_cast< int >( std::floor( ub ) );
		const double rk       = hRk( n, k ), phi = hPhi( n ), a6 = 0.5 / rk;
		//The harness's model of line k's light: a step at u = k through the
		//amplifier, weighted by the persistence to the frame's end, through
		//the hole's window clipped to the field.
		const double T     = 1.0 / ( n * 12.5 );
		const double tau   = 1.0 / ( 2.0 * hPi * hBandwidth( ParamFromBandwidth( 40000.0 ) ) );
		const double tEnd  = ub * T, tk = k * T;
		const double tauP  = hPersistence( ParamFromPersistence( 1.0 ) );
		double num = 0.0, den = 0.0;
		const int steps = 200000;
		for( int i = 0; i < steps; ++i )
		{
			const double t  = tk + ( tEnd - tk ) * ( i + 0.5 ) / steps;
			const double l  = ( 1.0 - std::exp( -( t - tk ) / tau ) ) * std::exp( -( tEnd - t ) / tauP );
			const double ph = ( t / T - k - 0.5 ) * phi;
			const double lo = std::max( ph - a6, -0.5 * phi ), hi = std::min( ph + a6, 0.5 * phi );
			if( hi > lo )
			{
				num += l * 0.5 * ( hi * hi - lo * lo );
				den += l * ( hi - lo );
			}
		}
		const double want = num / den;
		Moments m;
		for( int y = 0; y < raster.h; ++y )
			for( int x = 0; x < raster.w; ++x )
			{
				double r, ph;
				g.polar( x, y, r, ph );
				if( std::fabs( r - rk ) <= 0.5 + margin )
					m.add( d[ static_cast< size_t >( y ) * raster.w + x ], ph );
			}
		const double errA = ( m.mean() - want ) * rk * g.sy;
		//Tolerances: half a pixel. Each pixel holds its footprint's average,
		//so its light sits within half a pixel of its centre, and so does
		//the light-weighted centroid of all of them; the neighbouring lines
		//are dark, so the pixels taken tile the band.
		const double tolerance = 0.5;
		Check( worstR <= tolerance && mag( errA ) <= tolerance && m.w > 0.0,
		       fmt( "%4dx%-4d line radius off r_k + 1/(12 r_k):%s (tolerance %.3f px); the scanning hole (line %d, u_b %.4f) at %.5f rad, its light's "
		            "centroid %+.4f px along the arc from the model's (tolerance %.3f px)",
		            raster.w, raster.h, radii.c_str(), tolerance, k, ub, ( ub - k - 0.5 ) * phi, errA, tolerance ) );
	}
	return Verdict();
}

//===========================================================================
// --arc
//===========================================================================
int runArc( const Perturb& perturb )
{
	std::printf( "\n=== arc: a dot 4 holes high on the outermost and the innermost line, lit for that line alone, drawn 18 holes below and 18 above\n"
	             "    the middle: its arc's centroid moves r_k / r_c per hole of the picture with the drum and 1 with the disc (asin, exactly), so\n"
	             "    a dot of height h draws an arc h r_k / r_c long with the drum and h with the disc\n" );
	const int n    = 30;
	const double h = 4.0, offset = 18.0;
	for( const Raster& raster : kRasters )
	{
		const HGeo g = hGeo( raster.w, raster.h, n );
		for( const int scanner : { static_cast< int >( kDrum ), static_cast< int >( kDisc ) } )
			for( const int k : { 0, n - 1 } )
			{
				const double rk = hRk( n, k ), rc = hRc( n ), phi = hPhi( n );
				double centroid[ 2 ] = {}, want[ 2 ] = {}, light[ 2 ] = {}, centre[ 2 ] = {};
				for( int side = 0; side < 2; ++side )
				{
					//Whole pixel rows, so the dot's edges are the rows'.
					const double yc = side == 0 ? -offset : offset;
					const int y0    = static_cast< int >( std::lround( g.cy + ( yc - 0.5 * h ) * g.sy ) );
					const int y1    = y0 + static_cast< int >( std::lround( h * g.sy ) );
					Linear dot( raster.w, raster.h );
					for( int y = y0; y < y1; ++y )
					{
						//The line's path at the row: straight with the drum; with
						//the disc, rk - sqrt( rk^2 - Y^2 ) towards the disc's
						//centre. A tenth of a hole in from either side, then
						//whole pixels: no neighbouring line sees any of it.
						const double Y     = ( y + 0.5 - g.cy ) / g.sy;
						const double bend  = scanner == kDisc ? rk - std::sqrt( rk * rk - Y * Y ) : 0.0;
						const double xc    = g.cx + g.sx * ( 0.5 * n - k - 0.5 - bend );
						const double xlo   = std::ceil( xc - 0.4 * g.sx ), xhi = std::floor( xc + 0.4 * g.sx );
						dot.rect( xlo, xhi, y, y + 1.0, 1.0 );
					}
					const double Ya = ( y0 - g.cy ) / g.sy, Yb = ( y1 - g.cy ) / g.sy;
					centre[ side ]  = 0.5 * ( Ya + Yb );
					want[ side ]    = scanner == kDrum ? 0.5 * ( Ya + Yb ) / rc : 0.5 * ( std::asin( Ya / rk ) + std::asin( Yb / rk ) );
					const Floats lit = dot.card(), dark = grey( raster.w, raster.h, 0.0 );
					Rig a, b;
					if( !startRig( a, raster.w, raster.h, perturb, &lit ) || !startRig( b, raster.w, raster.h, perturb, &dark ) )
						return 1;
					for( Rig* r : { &a, &b } )
						r->Set( PT_SCANNER, scanner );
					if( !a.Render( 30 ) || !b.Render( 30 ) )
						return 1;
					const std::vector< double > ea = a.Exposure(), eb = b.Exposure();
					//Every pixel whose footprint can reach band k: the
					//neighbouring bands are dark, so the pixels tile the band.
					const double reach = 0.5 + g.halfDiagonal();
					Moments m;
					for( int y = 0; y < raster.h; ++y )
						for( int x = 0; x < raster.w; ++x )
						{
							double r, ph;
							g.polar( x, y, r, ph );
							const size_t i = static_cast< size_t >( y ) * raster.w + x;
							if( std::fabs( r - rk ) <= reach && std::fabs( ph ) < 0.5 * phi )
								m.add( ea[ i ] - eb[ i ], ph );
						}
					centroid[ side ] = m.mean();
					light[ side ]    = m.w;
				}
				const double moved = rk * ( centroid[ 1 ] - centroid[ 0 ] ), expected = rk * ( want[ 1 ] - want[ 0 ] );
				const double perHole = moved / ( centre[ 1 ] - centre[ 0 ] );
				//The tolerance: each centroid within half a pixel along the arc
				//(a footprint-averaged pixel's light sits within half a pixel
				//of its centre), and an eighth of a table cell (the table
				//point-samples a trapezoid; a kink inside a cell moves the
				//midpoint sum's centroid by at most cell / 16), at each end,
				//over the arc moved.
				const double cellArc   = rk * phi / hM( n );
				const double tolerance = ( 1.0 / g.sy + 0.125 * cellArc ) / std::fabs( expected );
				Check( mag( moved / expected - 1.0 ) <= tolerance && light[ 0 ] > 0.0 && light[ 1 ] > 0.0,
				       fmt( "%4dx%-4d %s, line %2d (r_k %.2f): the dot's arc moved %.4f holes against %.4f (%.5f a hole of the picture; r_k / r_c = %.5f): "
				            "%+.3f%% (tolerance %.3f%%)",
				            raster.w, raster.h, scanner == kDrum ? "drum" : "disc", k, rk, moved, expected, perHole, rk / rc, 100.0 * ( moved / expected - 1.0 ),
				            100.0 * tolerance ) );
			}
	}
	return Verdict();
}

//===========================================================================
// --dwell
//===========================================================================
int runDwell( const Perturb& perturb )
{
	std::printf( "\n=== dwell: one picture's exposure under a steady lamp (12.5 fps, the frame one picture long): each line's mean brightness\n"
	             "    against L w / ( r_k omega ) -- proportional to 1/r_k; each line's two ends, from the light in the pixels they cut, r_k Phi\n"
	             "    apart; the field's light N lines of the lamp's\n" );
	const int n = 30;
	for( const Raster& raster : kRasters )
	{
		const HGeo g      = hGeo( raster.w, raster.h, n );
		const Floats dark = grey( raster.w, raster.h, 0.0 );
		Rig rig;
		if( !startRig( rig, raster.w, raster.h, perturb, &dark, 12.5 ) )
			return 1;
		rig.Set( PT_LAMP, ParamFromLamp( 25.0 ) );
		const double L = hLamp( ParamFromLamp( 25.0 ) ) - hSwing;//black: 15 mA, the glow covering
		if( !rig.Render( 1 ) )
			return 1;
		const std::vector< double > a1 = rig.Exposure();
		if( !rig.Render( 1 ) )
			return 1;
		const std::vector< double > e = frameLight( rig.Exposure(), a1, rig.plugin.LastFrame().decay );

		//The frame covers u from u_a to u_b, exactly one picture.
		const int M       = hM( n );
		const int64_t s1  = hSamplesBy( hMicrosBy( 0, 12.5 ), n, 12.5 ), s2 = hSamplesBy( hMicrosBy( 1, 12.5 ), n, 12.5 );
		const double ua   = static_cast< double >( s1 ) / M, ub = static_cast< double >( s2 ) / M;
		const double T    = 1.0 / ( n * 12.5 );
		const double tauP = hPersistence( ParamFromPersistence( 1.0 ) );
		const double phi  = hPhi( n ), omega = phi * n * 12.5;
		//The weight of a scan at phase u: e^( -( u_b - u ) T / tau ). A
		//point at angle p of band k is scanned at u = k + 1/2 + p / Phi,
		//moved by whole pictures into [ u_a, u_b ).
		auto scanU = [ & ]( int k, double p ) {
			double u = k + 0.5 + p / phi;
			while( u < ua )
				u += n;
			while( u >= ub )
				u -= n;
			return u;
		};
		auto weight = [ & ]( double u ) { return std::exp( -( ub - u ) * T / tauP ); };
		//The frame's boundary falls inside one band, where the weight jumps:
		//pixels whose window reaches it are left out of the means.
		const double cut   = std::fmod( ua, static_cast< double >( n ) );
		const int cutBand  = static_cast< int >( std::floor( cut ) );
		const double cutAt = ( cut - cutBand - 0.5 ) * phi;
		const double inside = 0.5 - g.halfDiagonal();
		double worstMean = 0.0, worstLength = 0.0, totalAll = 0.0, wantAll = 0.0;
		int edgePixels   = 0;
		std::vector< double > ratio( n );
		for( int k = 0; k < n; ++k )
		{
			const double rk      = hRk( n, k );
			const double plateau = L / ( rk * omega );
			double sum = 0.0;
			int count  = 0;
			Moments top, bottom;
			for( int y = 0; y < raster.h; ++y )
				for( int x = 0; x < raster.w; ++x )
				{
					double r, p;
					g.polar( x, y, r, p );
					if( std::fabs( r - rk ) > inside )
						continue;
					const double hp   = g.halfAngle( r );
					const double frac = e[ static_cast< size_t >( y ) * raster.w + x ] / ( plateau * weight( scanU( k, p ) ) );
					const double reach = hp + 0.5 / rk;
					if( std::fabs( p ) < 0.5 * phi - reach && !( k == cutBand && std::fabs( p - cutAt ) < reach ) )
					{
						sum += frac;
						++count;
					}
					//A pixel the field's edge cuts holds the share of its
					//footprint inside: the edge sits that share in from its
					//inner side.
					if( std::fabs( std::fabs( p ) - 0.5 * phi ) < hp )
					{
						if( p > 0.0 )
							top.add( 1.0, p - hp + 2.0 * hp * frac );
						else
							bottom.add( 1.0, p + hp - 2.0 * hp * frac );
						++edgePixels;
					}
				}
			const double mean                   = sum / count;
			ratio[ static_cast< size_t >( k ) ] = mean;
			worstMean                           = worse( worstMean, mag( mean - 1.0 ) );
			const double length                 = top.w > 0.0 && bottom.w > 0.0 ? rk * ( top.mean() - bottom.mean() ) : 0.0;
			worstLength                         = worse( worstLength, mag( length / ( rk * phi ) - 1.0 ) );
			double wbar = 0.0;
			for( int i = 0; i < 4000; ++i )
				wbar += weight( scanU( k, ( ( i + 0.5 ) / 4000.0 - 0.5 ) * phi ) ) / 4000.0;
			wantAll += L * T * wbar;
		}
		for( size_t i = 0; i < e.size(); ++i )
			totalAll += e[ i ] / ( g.sx * g.sy );
		const double allErr = totalAll / wantAll - 1.0;
		//Tolerances. The mean: 1e-5 -- a pixel sums ~8 sample terms of ~20
		//float operations each, 6e-8 apiece. The length: a thousandth of a
		//pixel at each end (the share is good to the mean's 1e-5; the
		//shader's float angle to 1e-7 rad, 3e-4 px at r_k 350, 10 px a
		//hole). The field: 1e-5 (the same, summed).
		const double tolLength = 2.0 * 1e-3 / ( g.sy * hRk( n, n - 1 ) * phi );
		Check( worstMean <= 1e-5 && worstLength <= tolLength && mag( allErr ) <= 1e-5 && edgePixels >= 2 * n,
		       fmt( "%4dx%-4d mean over L w / ( r_k omega ) worst %.1e off 1 (tolerance 1e-5; outer %.7f, inner %.7f; brightness outer/inner %.5f = r_29/r_0 "
		            "%.5f); length worst %.1e off r_k Phi (tolerance %.1e, %d edge pixels); the field's light %+.1e off N lines' (tolerance 1e-5)",
		            raster.w, raster.h, worstMean, ratio.front(), ratio.back(), ratio.front() / ratio.back() * hRk( n, n - 1 ) / hRk( n, 0 ),
		            hRk( n, n - 1 ) / hRk( n, 0 ), worstLength, tolLength, edgePixels, allErr ) );
	}
	return Verdict();
}

//===========================================================================
// --bandwidth
//===========================================================================
int runBandwidth( const Perturb& perturb )
{
	std::printf( "\n=== bandwidth: a horizontal edge on alternate lines, at 2 kHz and 20 kHz: the light the amplifier withholds after the edge\n"
	             "    moves the edge up each line by tau = 1/(2 pi B) -- the same angle on every line, a longer arc on the outer\n" );
	const int n = 30;
	for( const Raster& raster : kRasters )
	{
		const HGeo g = hGeo( raster.w, raster.h, n );
		Linear edge( raster.w, raster.h );
		for( int k = 0; k < n; k += 2 )
		{
			double x0, x1;
			drumPixels( g, k, x0, x1 );
			edge.rect( x0, x1, g.cy, raster.h, 1.0 );
		}
		const Floats card = edge.card();
		const double bands[ 2 ] = { 2000.0, 20000.0 };
		std::vector< double > e[ 2 ];
		double decay[ 2 ] = {};
		for( int i = 0; i < 2; ++i )
		{
			Rig rig;
			if( !startRig( rig, raster.w, raster.h, perturb, &card, 12.5 ) )
				return 1;
			rig.Set( PT_BANDWIDTH, ParamFromBandwidth( bands[ i ] ) );
			if( !rig.Render( 1 ) )
				return 1;
			const std::vector< double > a1 = rig.Exposure();
			if( !rig.Render( 1 ) )
				return 1;
			e[ i ]     = frameLight( rig.Exposure(), a1, rig.plugin.LastFrame().decay );
			decay[ i ] = rig.plugin.LastFrame().decay;
		}
		const double tau1 = 1.0 / ( 2.0 * hPi * hBandwidth( ParamFromBandwidth( bands[ 0 ] ) ) );
		const double tau2 = 1.0 / ( 2.0 * hPi * hBandwidth( ParamFromBandwidth( bands[ 1 ] ) ) );
		const int M       = hM( n );
		const double ub   = static_cast< double >( hSamplesBy( hMicrosBy( 1, 12.5 ), n, 12.5 ) ) / M;
		const double ua   = static_cast< double >( hSamplesBy( hMicrosBy( 0, 12.5 ), n, 12.5 ) ) / M;
		const double T    = 1.0 / ( n * 12.5 );
		const double tauP = hPersistence( ParamFromPersistence( 1.0 ) );
		const double phi = hPhi( n ), omega = phi * n * 12.5;
		//The window: from 0.3 of the line, before the edge, to 0.95, clear of
		//the sync at 0.986 by the hole's half-width and a pixel.
		const double sLow = 0.3, sTop = 0.95;
		double worstAngle = 0.0, minArc = 1e30, maxArc = 0.0;
		std::string lines;
		for( int k = 0; k < n; k += 2 )
		{
			const double rk = hRk( n, k );
			//The white the line carries: its whole pixels' share of the strip.
			double x0, x1;
			drumPixels( g, k, x0, x1 );
			const double level = ( x1 - x0 ) / g.sx;
			double u           = k + 0.5;
			while( u < ua )
				u += n;
			const double w = std::exp( -( ub - u ) * T / tauP );
			double diff    = 0.0;
			for( int y = 0; y < raster.h; ++y )
				for( int x = 0; x < raster.w; ++x )
				{
					double r, p;
					g.polar( x, y, r, p );
					if( std::fabs( r - rk ) > 1.0 )
						continue;
					//The window, each pixel by the share of its width inside.
					const double hp = g.halfAngle( r );
					const double lo = ( sLow - 0.5 ) * phi, hi = ( sTop - 0.5 ) * phi;
					const double share = std::clamp( ( std::min( p + hp, hi ) - std::max( p - hp, lo ) ) / ( 2.0 * hp ), 0.0, 1.0 );
					const size_t i     = static_cast< size_t >( y ) * raster.w + x;
					diff += share * ( e[ 1 ][ i ] - e[ 0 ][ i ] ) / ( g.sx * g.sy );
				}
			//The withheld light is 20 mA x tau x the weight: the difference
			//of the two gives tau1 - tau2.
			const double dtau  = diff / ( 2.0 * hSwing * level * w );
			const double angle = omega * dtau;
			//The light withheld after the edge is weighted by the persistence
			//at its own time, e^( t / tau_p ) later: tau / ( 1 - tau / tau_p ).
			const double eff   = tau1 / ( 1.0 - tau1 / tauP ) - tau2 / ( 1.0 - tau2 / tauP );
			worstAngle         = worse( worstAngle, mag( dtau / eff - 1.0 ) );
			minArc             = std::min( minArc, rk * angle );
			maxArc             = std::max( maxArc, rk * angle );
			if( k == 0 || k == n - 2 )
				lines += fmt( " line %d %.6f rad (arc %.4f holes);", k, angle, rk * angle );
		}
		//The tolerance: 2e-4. A pixel's exposure is good to 1e-5 (--dwell's
		//bound), and the window holds 0.45 of a line of white, 17 times the
		//0.027 of a line the edge moves: 1.7e-4. The 2 kHz edge's residue
		//at the window's top is e^-15. (The withheld light's weight, 8e-5
		//above the edge's at 2 kHz, is in the expectation.)
		Check( worstAngle <= 2e-4 && maxArc > minArc,
		       fmt( "%4dx%-4d edge moved by omega ( tau1 - tau2 ) = %.6f rad: worst line %.2e off (tolerance 2e-4);%s arcs %.4f..%.4f holes", raster.w,
		            raster.h, omega * ( tau1 - tau2 ), worstAngle, lines.c_str(), minArc, maxArc ) );
		( void )decay;
	}
	return Verdict();
}

//===========================================================================
// --lamp
//===========================================================================
/// The harness's lamp: a black-to-grey step ( level ell, the drum's
/// aperture a ramp T_ap long centred on the edge ) through the amplifier,
/// striking after the formative lag, light = current. The light missing
/// against an instant lamp, as a time at the final current.
double missingTime( double lamp, double ell, double tau, double tAp, double& strikeAt )
{
	const double target = lamp + hSwing * ( 2.0 * ell - 1.0 );
	const double dt     = 2e-9;
	double y = 0.0, progress = 0.0, missing = 0.0;
	bool lit = false;
	strikeAt = -1.0;
	for( double t = -tAp; t < 2e-3; t += dt )
	{
		const double x = ell * std::clamp( ( t + 0.5 * dt ) / tAp + 0.5, 0.0, 1.0 );
		y              = x + ( y - x ) * std::exp( -dt / tau );
		const double I = lamp + hSwing * ( 2.0 * y - 1.0 );
		if( !lit )
		{
			if( I > hStrike )
			{
				progress += dt / hLag( 10.0 * ( I - hStrike ) );
				if( progress >= 1.0 )
				{
					lit      = true;
					strikeAt = t;
				}
			}
			else
				progress = 0.0;
		}
		const double ideal = t + 0.5 * dt > 0.0 ? target : 0.0;
		missing += ( ideal - ( lit ? I : 0.0 ) ) * dt;
	}
	return missing / target;
}

int runLamp( const Perturb& perturb )
{
	std::printf( "\n=== lamp: brightness proportional to the drive current (two greys); the striking delay after a black-to-grey step with the\n"
	             "    lamp out, against the formative lag at 30 V and 60 V over; below 5 mA the glow's area proportional to the current\n" );
	for( const Raster& raster : kRasters )
	{
		//(a) Two greys, the same frame of each.
		{
			const int n  = 30;
			const HGeo g = hGeo( raster.w, raster.h, n );
			const double ell[ 2 ] = { 0.25, 0.75 };
			double sums[ 2 ] = {};
			for( int i = 0; i < 2; ++i )
			{
				const Floats card = grey( raster.w, raster.h, codeOf( ell[ i ] ) );
				Rig rig;
				if( !startRig( rig, raster.w, raster.h, perturb, &card, 12.5 ) || !rig.Render( 2 ) )
					return 1;
				const std::vector< double > a = rig.Exposure();
				for( int y = 0; y < raster.h; ++y )
					for( int x = 0; x < raster.w; ++x )
					{
						double r, p;
						g.polar( x, y, r, p );
						if( std::fabs( r - g.rc ) < 0.5 * n - 1.0 && std::fabs( p ) < 0.3 * g.phi )
							sums[ i ] += a[ static_cast< size_t >( y ) * raster.w + x ];
					}
			}
			const double lamp = hLamp( ParamFromLamp( 15.0 ) );
			const double I0 = lamp + hSwing * ( 2.0 * linearOf( static_cast< float >( codeOf( ell[ 0 ] ) ) ) - 1.0 );
			const double I1 = lamp + hSwing * ( 2.0 * linearOf( static_cast< float >( codeOf( ell[ 1 ] ) ) ) - 1.0 );
			const double err = sums[ 1 ] / sums[ 0 ] / ( I1 / I0 ) - 1.0;
			Check( mag( err ) <= 2e-5, fmt( "%4dx%-4d brightness at %.3f mA over brightness at %.3f mA: %.6f against %.6f, %+.2e (tolerance 2e-5: each sum to --dwell's 1e-5)", raster.w,
			                                raster.h, I1, I0, sums[ 1 ] / sums[ 0 ], I1 / I0, err ) );
		}
		//(b) The strike. 120 lines at 12.5 pictures a second: a sample is
		//0.6 us, so the lag (30 to 95 us) is 50 to 160 samples, and the
		//lamp, struck by 0.65 of the line, burns steadily over 0.8..0.95.
		//The frame is one picture and starts at a line's start (u 25), so
		//every line is scanned once, whole, in it.
		{
			const int n  = 120;
			const HGeo g = hGeo( raster.w, raster.h, n );
			const double lamp = 8.0;
			const double tau  = 1.0 / ( 2.0 * hPi * hBandwidth( ParamFromBandwidth( 40000.0 ) ) );
			const double T    = 1.0 / ( n * 12.5 );
			const double tAp  = T * 3.0 / ( 7.0 * n );
			std::string text;
			double worst = 0.0;
			for( const double over : { 30.0, 60.0 } )
			{
				const double target = hStrike + over / 10.0;
				const double ell    = ( ( target - lamp ) / hSwing + 1.0 ) / 2.0;
				Linear step( raster.w, raster.h );
				step.rect( 0.0, raster.w, g.cy, raster.h, ell );
				const Floats card = step.card();
				Rig rig;
				if( !startRig( rig, raster.w, raster.h, perturb, &card, 12.5 ) )
					return 1;
				rig.Set( PT_LINES, static_cast< float >( n ) );
				rig.Set( PT_RATE, ParamFromRate( 12.5 ) );
				rig.Set( PT_LAMP, ParamFromLamp( lamp ) );
				if( !rig.Render( 1 ) )
					return 1;
				const std::vector< double > a1 = rig.Exposure();
				if( !rig.Render( 1 ) )
					return 1;
				const std::vector< double > e = frameLight( rig.Exposure(), a1, rig.plugin.LastFrame().decay );
				const double phi = hPhi( n );
				double s1 = 0.0, s2 = 0.0;
				const double sLow = 0.3, sMid = 0.8, sTop = 0.95;
				for( int y = 0; y < raster.h; ++y )
					for( int x = 0; x < raster.w; ++x )
					{
						double r, p;
						g.polar( x, y, r, p );
						if( std::fabs( r - g.rc ) > 0.5 * n )
							continue;
						const double hp = g.halfAngle( r );
						auto share      = [ & ]( double a, double b ) {
                            return std::clamp( ( std::min( p + hp, ( b - 0.5 ) * phi ) - std::max( p - hp, ( a - 0.5 ) * phi ) ) / ( 2.0 * hp ), 0.0, 1.0 );
						};
						const double v = e[ static_cast< size_t >( y ) * raster.w + x ];
						s1 += share( sLow, sTop ) * v;
						s2 += share( sMid, sTop ) * v;
					}
				const double measured = T * ( ( sTop - 0.5 ) - s1 / s2 * ( sTop - sMid ) );
				double strikeAt       = 0.0;
				const double want     = missingTime( lamp, linearOf( static_cast< float >( codeOf( ell ) ) ), tau, tAp, strikeAt );
				worst                 = worse( worst, mag( measured - want ) );
				text += fmt( " %g V over (lag %.2f us): light missing for %.3f us, the model %.3f us (strikes %.3f us after the edge);", over, 1e6 * hLag( over ),
				             1e6 * measured, 1e6 * want, 1e6 * strikeAt );
			}
			//The tolerance: two samples (1.2 us) -- the lamp runs its lag on
			//each sample's average current, and strikes within a sample.
			Check( worst <= 2.0 * T / hM( n ), fmt( "%4dx%-4d%s worst %.3f us (tolerance %.2f us, two samples)", raster.w, raster.h, text.c_str(), 1e6 * worst,
			                                       2e6 * T / hM( n ) ) );
		}
		//(c) A starved glow: 4 mA, then 2.5 mA (below the strike, so it
		//stays lit only because it was lit). The lit area against I / 5 mA.
		{
			const int n  = 30;
			const HGeo g = hGeo( raster.w, raster.h, n );
			const Floats dark = grey( raster.w, raster.h, 0.0 );
			Rig rig;
			if( !startRig( rig, raster.w, raster.h, perturb, &dark, 12.5 ) )
				return 1;
			std::string text;
			double worst = 0.0;
			double totals[ 2 ] = {};
			const double currents[ 2 ] = { 4.0, 2.5 };
			for( int i = 0; i < 2; ++i )
			{
				rig.Set( PT_LAMP, ParamFromLamp( hSwing + currents[ i ] ) );
				if( !rig.Render( 10 ) )
					return 1;
				const std::vector< double > a1 = rig.Exposure();
				if( !rig.Render( 1 ) )
					return 1;
				const std::vector< double > e = frameLight( rig.Exposure(), a1, rig.plugin.LastFrame().decay );
				const double I = hLamp( ParamFromLamp( hSwing + currents[ i ] ) ) - hSwing;
				//Divide out each point's scan weight; the dwell's 1/r cancels
				//the area's r, so the glow is uniform in ( r, phi ).
				const int M       = hM( n );
				const double ub   = static_cast< double >( hSamplesBy( hMicrosBy( rig.frame - 1, 12.5 ), n, 12.5 ) ) / M;
				const double T    = 1.0 / ( n * 12.5 );
				const double phi  = hPhi( n );
				Moments mr, mp;
				for( int y = 0; y < raster.h; ++y )
					for( int x = 0; x < raster.w; ++x )
					{
						double r, p;
						g.polar( x, y, r, p );
						const int k = static_cast< int >( std::floor( g.rc + 0.5 * n - r ) );
						if( k < 0 || k >= n )
							continue;
						double u = k + 0.5 + p / phi;
						while( u >= ub )
							u -= n;
						while( u < ub - n )
							u += n;
						const double v = e[ static_cast< size_t >( y ) * raster.w + x ] / std::exp( -( ub - u ) * T / 1.0 );
						mr.add( v, r );
						mp.add( v, p );
					}
				totals[ i ] = mr.w;
				const double pr = 1.0 / g.sx, pp = 1.0 / ( g.sy * g.rc );
				const double gr = std::sqrt( 12.0 * ( mr.variance() - pr * pr / 12.0 ) ) / n;
				const double gp = std::sqrt( 12.0 * ( mp.variance() - pp * pp / 12.0 ) ) / phi;
				const double want = std::sqrt( I / hCover );
				worst = worse( worst, mag( gr / want - 1.0 ), mag( gp / want - 1.0 ) );
				text += fmt( " %.2f mA: glow %.4f of the field across, %.4f along, area %.4f (I/5mA %.4f);", I, gr, gp, gr * gp, I / hCover );
			}
			//The tolerance: the lattice at the glow's four edges, a pixel^2/12
			//of variance each way, as a share (half, in extent) of the glow's.
			const double tol = 0.5 * ( 1.0 / ( g.sx * g.sx ) ) / ( n * n * 0.5 ) + 1e-4;
			const double ratio = totals[ 1 ] / totals[ 0 ];
			const double I0 = hLamp( ParamFromLamp( hSwing + currents[ 0 ] ) ) - hSwing, I1 = hLamp( ParamFromLamp( hSwing + currents[ 1 ] ) ) - hSwing;
			Check( worst <= tol && mag( ratio / ( I1 / I0 ) - 1.0 ) <= 1e-4 && rig.Chain().lit,
			       fmt( "%4dx%-4d%s extents worst %.2e off sqrt( I / 5 mA ) (tolerance %.1e); total light %.6f against %.6f (tolerance 1e-4); lamp %s at 2.5 mA",
			            raster.w, raster.h, text.c_str(), worst, tol, ratio, I1 / I0, rig.Chain().lit ? "still lit" : "OUT" ) );
		}
	}
	return Verdict();
}

//===========================================================================
// --hunting
//===========================================================================
/// The pull C for a steady grey: the amplifier's periodic state over a line
/// (picture at ell, the sync's samples at 0), the lamp lit throughout.
double hPull( double lamp, double ell, double tau, int n, double rate )
{
	const int M     = hM( n );
	const double dt = 1.0 / ( n * M * rate );
	const double e = std::exp( -dt / tau ), K = tau / dt * ( 1.0 - e );
	double y = ell, pic = 0.0, syn = 0.0;
	for( int pass = 0; pass < 3; ++pass )
	{
		pic = syn = 0.0;
		for( int j = 0; j < M; ++j )
		{
			const double x   = j < M - hSync ? ell : 0.0;
			const double avg = x + ( y - x ) * K;
			y                = x + ( y - x ) * e;
			const double I   = lamp + hSwing * ( 2.0 * avg - 1.0 );
			( j < M - hSync ? pic : syn ) += I * I;
		}
	}
	return pic / ( M - hSync ) - syn / hSync;
}
/// The grey whose pull is `ratio` x the reference.
double hGreyFor( double ratio, double lamp, double tau, int n, double rate )
{
	double lo = 0.0, hi = 1.0;
	for( int i = 0; i < 60; ++i )
	{
		const double mid = 0.5 * ( lo + hi );
		( hPull( lamp, mid, tau, n, rate ) < ratio * apparatus::kReferencePull ? lo : hi ) = mid;
	}
	return 0.5 * ( lo + hi );
}

int runHunting( const Perturb& perturb )
{
	std::printf( "\n=== hunting: the phonic wheel after a framing step -- frequency and decay against the linearised swing equation with C from\n"
	             "    the lamp's currents; the lock's offset under a speed error against asin; slipping past pull-out and not before\n" );
	const int n       = 30;
	const double rate = 12.5;
	const double fps  = 240.0;
	const int w = 64, h = 36;//the wheel's state does not depend on the raster
	const double lamp = 20.0;
	const double tau  = 1.0 / ( 2.0 * hPi * hBandwidth( ParamFromBandwidth( 40000.0 ) ) );
	const double wh   = 2.0 * hPi * hHunt;

	//(a) Small swings at twice the reference pull.
	{
		const double ell  = hGreyFor( 2.0, lamp, tau, n, rate );
		const Floats card = grey( w, h, codeOf( ell ) );
		const double C    = hPull( lamp, linearOf( static_cast< float >( codeOf( ell ) ) ), tau, n, rate );
		Rig rig;
		if( !startRig( rig, w, h, perturb, &card, fps ) )
			return 1;
		rig.Set( PT_LAMP, ParamFromLamp( lamp ) );
		rig.Set( PT_SYNC, static_cast< int >( kHunting ) );
		if( !rig.Render( static_cast< int >( fps ) ) )
			return 1;
		const double step = 0.02;
		rig.Set( PT_FRAMING, ParamFromFraming( step ) );
		const double F = hFraming( ParamFromFraming( step ) );
		std::vector< double > e;
		double last = rig.Chain().psi, unwrapped = last;
		for( int f = 0; f < static_cast< int >( 8 * fps ); ++f )
		{
			if( !rig.Render( 1 ) )
				return 1;
			double d = rig.Chain().psi - last;
			d -= std::round( d );
			unwrapped += d;
			last = rig.Chain().psi;
			e.push_back( n * unwrapped - F );
		}
		//Crossings by linear interpolation, extrema by a parabola through
		//three frames.
		std::vector< double > crossings, peaks, peakTimes;
		for( size_t i = 1; i < e.size(); ++i )
			if( ( e[ i - 1 ] < 0.0 ) != ( e[ i ] < 0.0 ) )
				crossings.push_back( ( i - 1 + e[ i - 1 ] / ( e[ i - 1 ] - e[ i ] ) ) / fps );
		for( size_t i = 1; i + 1 < e.size(); ++i )
			if( std::fabs( e[ i ] ) >= std::fabs( e[ i - 1 ] ) && std::fabs( e[ i ] ) > std::fabs( e[ i + 1 ] ) )
			{
				const double a = e[ i - 1 ], b = e[ i ], c = e[ i + 1 ];
				const double off = 0.5 * ( a - c ) / ( a - 2.0 * b + c );
				peaks.push_back( std::fabs( b - 0.25 * ( a - c ) * off ) );
				peakTimes.push_back( ( i + off ) / fps );
			}
		const double wn2   = wh * wh * C / apparatus::kReferencePull;
		const double wd    = std::sqrt( wn2 - 1.0 / ( 4.0 * hTauM * hTauM ) );
		const double sigma = 1.0 / ( 2.0 * hTauM );
		double measuredW = std::numeric_limits< double >::quiet_NaN(), measuredS = measuredW;
		if( crossings.size() >= 6 && peaks.size() >= 6 )
		{
			const size_t last2 = crossings.size() - 1 - ( ( crossings.size() - 1 ) % 2 );
			measuredW          = hPi * last2 / ( crossings[ last2 ] - crossings[ 0 ] );
			const size_t pa = 1, pb = peaks.size() - 1;
			measuredS = std::log( peaks[ pa ] / peaks[ pb ] ) / ( peakTimes[ pb ] - peakTimes[ pa ] );
		}
		//Tolerances: the swing's own amplitude (0.02 lines: 2 pi x 0.02 rad,
		//a pendulum slows by theta^2/16 = 1e-3) plus the interpolation's
		//(omega dt)^3 ~ 2e-4; for the decay, 1% (the extrema's parabola over
		//a 7e-4 amplitude change between samples).
		Check( mag( measuredW / wd - 1.0 ) <= 1.5e-3 && mag( measuredS / sigma - 1.0 ) <= 0.01,
		       fmt( "C = %.3f mA^2 (%.3f x the reference) from a grey of %.4f at %.0f mA: swings at %.5f rad/s against %.5f (%+.2e, tolerance 1.5e-3), "
		            "decaying at %.5f /s against 1/(2 tau_m) = %.5f (%+.2e, tolerance 1e-2); %zu crossings",
		            C, C / apparatus::kReferencePull, ell, lamp, measuredW, wd, measuredW / wd - 1.0, measuredS, sigma, measuredS / sigma - 1.0, crossings.size() ) );
	}
	//(b) The lock's offset under a speed error, and (c) pull-out, at half
	//the reference pull.
	for( const double ratio : { 2.0, 0.5 } )
	{
		const double ell  = hGreyFor( ratio, lamp, tau, n, rate );
		const Floats card = grey( w, h, codeOf( ell ) );
		const double C    = hPull( lamp, linearOf( static_cast< float >( codeOf( ell ) ) ), tau, n, rate );
		const double pullOut = hTauM * wh * wh * ( C / apparatus::kReferencePull ) / ( 2.0 * hPi * n * rate );
		const std::vector< double > fractions = ratio > 1.0 ? std::vector< double > { 0.5 } : std::vector< double > { 0.9, 1.1 };
		for( const double fraction : fractions )
		{
			const double speed = fraction * pullOut;
			Rig rig;
			if( !startRig( rig, w, h, perturb, &card, fps ) )
				return 1;
			rig.Set( PT_LAMP, ParamFromLamp( lamp ) );
			rig.Set( PT_SYNC, static_cast< int >( kHunting ) );
			//The speed ramped in over 10 s (slower than the 2 s decay: the
			//wheel follows its equilibrium), then held 6 s.
			const int ramp = static_cast< int >( 10 * fps ), hold = static_cast< int >( 6 * fps );
			rig.beforeFrame = [ & ]( int f ) { rig.Set( PT_SPEED, ParamFromSpeed( speed * std::min( 1.0, f / static_cast< double >( ramp ) ) ) ); };
			double last = rig.Chain().psi, unwrapped = 0.0;
			std::vector< double > e;
			for( int f = 0; f < ramp + hold; ++f )
			{
				if( !rig.Render( 1 ) )
					return 1;
				double d = rig.Chain().psi - last;
				d -= std::round( d );
				if( f > 0 )
					unwrapped += d;
				last = rig.Chain().psi;
				e.push_back( n * unwrapped );
			}
			const double nu = hSpeed( ParamFromSpeed( speed ) ) * rate;
			const double s  = 2.0 * hPi * n * nu / ( hTauM * wh * wh * C / apparatus::kReferencePull );
			if( fraction < 1.0 )
			{
				//The mean over the last whole periods of the small swing.
				const double wn   = wh * std::sqrt( C / apparatus::kReferencePull * std::sqrt( std::max( 0.0, 1.0 - s * s ) ) );
				const int period  = static_cast< int >( std::round( 2.0 * hPi / wn * fps ) );
				double mean       = 0.0;
				for( int i = 0; i < 2 * period; ++i )
					mean += e[ e.size() - 1 - static_cast< size_t >( i ) ] / ( 2 * period );
				const double want = std::asin( s ) / ( 2.0 * hPi );
				double span       = 0.0;
				for( const double v : e )
					span = std::max( span, std::fabs( v ) );
				//The tolerance: the residual swing over two of its periods
				//(the ramp's lag decays as e^-3 over the hold), 2% of the
				//offset.
				Check( mag( mean / want - 1.0 ) <= 0.02 && span < 0.5,
				       fmt( "C = %.3f x the reference, speed %+.3f%% (%.0f%% of pull-out %.3f%%): locked %.5f lines off the framing against asin(...)/(2 pi) = "
				            "%.5f (%+.2e, tolerance 2e-2); never more than %.3f lines off",
				            C / apparatus::kReferencePull, 100.0 * speed, 100.0 * fraction, 100.0 * pullOut, mean, want, mean / want - 1.0, span ) );
			}
			else
			{
				const double drift = e.back();
				Check( drift > 1.0, fmt( "C = %.3f x the reference, speed %+.3f%% (%.0f%% of pull-out %.3f%%): slipped %.2f lines in the 6 s held (it must slip)",
				                         C / apparatus::kReferencePull, 100.0 * speed, 100.0 * fraction, 100.0 * pullOut, drift ) );
			}
		}
	}
	return Verdict();
}

//===========================================================================
// --slip
//===========================================================================
int runSlip( const Perturb& perturb )
{
	std::printf( "\n=== slip: a free disc, a white field, frames one picture long: the sync's dark band -- the light it takes, against the same\n"
	             "    run with the sync's samples carrying the picture -- has its centroid along the lines advance N x Speed x f lines a second\n"
	             "    (the framing set so the band travels about the lines' middle, clear of their ends)\n" );
	const int n       = 30;
	const double rate = 12.5, fps = 12.5;
	for( const Raster& raster : kRasters )
	{
		const HGeo g       = hGeo( raster.w, raster.h, n );
		const Floats white = grey( raster.w, raster.h, 1.0 );
		std::string text;
		double worst = 0.0, deficit = 1e30;
		//The tolerance: each frame's centroid within half a pixel's angle at
		//the innermost line (every pixel holds its footprint's average, and
		//the pixels tile the field), at each end, in lines.
		const double tol = 2.0 * g.halfAngle( hRk( n, n - 1 ) - 0.5 ) / hPhi( n );
		for( const double speed : { 0.002, -0.002, 0.004, -0.004 } )
		{
			Rig rig, ref;
			Perturb plain = perturb;
			plain.chain.noSync = true;
			if( !startRig( rig, raster.w, raster.h, perturb, &white, fps ) || !startRig( ref, raster.w, raster.h, plain, &white, fps ) )
				return 1;
			for( Rig* r : { &rig, &ref } )
			{
				r->Set( PT_SYNC, static_cast< int >( kFree ) );
				r->Set( PT_SPEED, ParamFromSpeed( speed ) );
			}
			const int pictures = std::fabs( speed ) < 0.003 ? 6 : 3;
			//The framing that centres the band's travel mid-line: it moves
			//N nu lines a second, from frame 1 (its middle at 0.057 s) for
			//the pictures measured, and a frame spreads it over N nu / fps;
			//framing is a phase, so it is taken modulo a line.
			const double travel = n * hSpeed( ParamFromSpeed( speed ) ) * rate;
			double framing      = -0.5 - travel * ( ( 1.0 / 60.0 + 0.5 / fps ) + 0.5 * pictures / fps );
			framing -= std::floor( framing + 0.5 );
			for( Rig* r : { &rig, &ref } )
				r->Set( PT_FRAMING, ParamFromFraming( framing ) );
			std::vector< double > position;
			std::vector< double > before( static_cast< size_t >( raster.w ) * raster.h, 0.0 ), beforeRef = before;
			for( int f = 0; f <= 1 + pictures; ++f )
			{
				if( !rig.Render( 1 ) || !ref.Render( 1 ) )
					return 1;
				const std::vector< double > now = rig.Exposure(), nowRef = ref.Exposure();
				if( f >= 1 )
				{
					const std::vector< double > e  = frameLight( now, before, rig.plugin.LastFrame().decay );
					const std::vector< double > er = frameLight( nowRef, beforeRef, ref.plugin.LastFrame().decay );
					Moments m;
					for( int y = 0; y < raster.h; ++y )
						for( int x = 0; x < raster.w; ++x )
						{
							double r, p;
							g.polar( x, y, r, p );
							const size_t i = static_cast< size_t >( y ) * raster.w + x;
							m.add( er[ i ] - e[ i ], p );
						}
					position.push_back( m.mean() / hPhi( n ) + 0.5 );
					deficit = std::min( deficit, m.w );
				}
				before    = now;
				beforeRef = nowRef;
			}
			const double moved = position.back() - position.front();
			const double dt    = pictures / fps;
			const double want  = n * hSpeed( ParamFromSpeed( speed ) ) * rate * dt;
			worst              = worse( worst, mag( moved - want ) );
			text += fmt( " %+.1f%%: %+.5f lines in %.2f s against %+.5f (at %.3f..%.3f);", 100.0 * speed, moved, dt, want,
			             std::min( position.front(), position.back() ), std::max( position.front(), position.back() ) );
		}
		Check( worst <= tol && deficit > 0.0,
		       fmt( "%4dx%-4d%s worst %.1e lines (tolerance %.5f: half a pixel at each end)", raster.w, raster.h, text.c_str(), worst, tol ) );
	}
	return Verdict();
}

//===========================================================================
// --flicker
//===========================================================================
/// The integral of a piecewise-linear function ( t_i, v_i ) times
/// e^( -( t - t' ) / tau ), over t' up to t.
double expIntegral( const std::vector< std::pair< double, double > >& knots, double t, double tau )
{
	double total = 0.0;
	for( size_t i = 0; i + 1 < knots.size(); ++i )
	{
		double a = knots[ i ].first, b = knots[ i + 1 ].first;
		if( a >= t )
			break;
		double va = knots[ i ].second, vb = knots[ i + 1 ].second;
		if( b > t )
		{
			vb = va + ( vb - va ) * ( t - a ) / ( b - a );
			b  = t;
		}
		if( b <= a )
			continue;
		//v(t') = va + s ( t' - a ); integral of v e^( ( t' - t ) / tau ).
		const double s  = ( vb - va ) / ( b - a );
		const double ea = std::exp( ( a - t ) / tau ), eb = std::exp( ( b - t ) / tau );
		total += tau * ( vb * eb - va * ea ) - s * tau * tau * ( eb - ea );
	}
	return total;
}

int runFlicker( const Perturb& perturb )
{
	std::printf( "\n=== flicker: a pixel's exposure frame after frame under a steady lamp, against the closed form of the exponential exposure of\n"
	             "    its scans -- at 60, 24 and 8 fps (the last a frame longer than a picture, and longer than the 0.1 s a frame may run)\n" );
	const int n       = 30;
	const double rate = 12.5;
	for( const Raster& raster : kRasters )
	{
		const HGeo g      = hGeo( raster.w, raster.h, n );
		const Floats dark = grey( raster.w, raster.h, 0.0 );
		std::string text;
		double worstAll = 0.0;
		for( const double fps : { 60.0, 24.0, 8.0 } )
		{
			Rig rig;
			if( !startRig( rig, raster.w, raster.h, perturb, &dark, fps ) )
				return 1;
			rig.Set( PT_LAMP, ParamFromLamp( 25.0 ) );
			rig.Set( PT_PERSISTENCE, ParamFromPersistence( 0.1 ) );
			const double tauP = hPersistence( ParamFromPersistence( 0.1 ) );
			const double L    = hLamp( ParamFromLamp( 25.0 ) ) - hSwing;
			const double phi = hPhi( n ), T = 1.0 / ( n * rate ), M = hM( n );
			//Pixels well inside five bands.
			struct Probe
			{
				int x, y, k;
				double r, p;
			};
			std::vector< Probe > probes;
			for( const int k : { 2, 9, 16, 23, 29 } )
			{
				const double rk = hRk( n, k );
				const double target = ( k % 3 - 1 ) * 0.3 * phi;
				const int x = static_cast< int >( std::floor( g.cx + ( rk * std::cos( target ) - g.rc ) * g.sx ) );
				const int y = static_cast< int >( std::floor( g.cy + rk * std::sin( target ) * g.sy ) );
				Probe pr { x, y, k, 0, 0 };
				g.polar( x, y, pr.r, pr.p );
				if( std::fabs( pr.r - rk ) <= 0.5 - g.halfDiagonal() )
					probes.push_back( pr );
			}
			const int frames = static_cast< int >( 2.0 * fps );
			double worst     = 0.0;
			int compared     = 0;
			for( int f = 0; f < frames; ++f )
			{
				if( !rig.Render( 1 ) )
					return 1;
				const std::vector< double > a = rig.Exposure();
				const int64_t s     = hSamplesBy( hMicrosBy( f, fps ), n, rate );
				const double t      = s / ( n * M * rate );
				for( const Probe& pr : probes )
				{
					//The hole's window over the pixel's width, scan by scan:
					//a trapezoid in time.
					const double rk = hRk( n, pr.k ), a6 = 0.5 / rk, hp = g.halfAngle( pr.r );
					const double lo = std::max( pr.p - hp, -0.5 * phi ), hi = std::min( pr.p + hp, 0.5 * phi );
					const double wr = 0.5 * std::min( a6, 0.5 * ( hi - lo ) ) * 2.0;
					double total = 0.0;
					bool during  = false;
					for( int m = 0; m <= static_cast< int >( t * rate ) + 1; ++m )
					{
						//The hole's centre at angle q at t' = ( k + 1/2 + q/Phi + m N ) T.
						auto at = [ & ]( double q ) { return ( pr.k + 0.5 + q / phi + m * n ) * T; };
						const double c0 = lo - a6, c3 = hi + a6;
						const double c1 = std::min( lo + a6, hi - a6 ), c2 = std::max( lo + a6, hi - a6 );
						const double top = std::min( 2.0 * a6, hi - lo );
						std::vector< std::pair< double, double > > knots = { { at( c0 ), 0.0 }, { at( c1 ), top }, { at( c2 ), top }, { at( c3 ), 0.0 } };
						if( t > knots.front().first && t < knots.back().first )
							during = true;
						total += expIntegral( knots, t, tauP );
					}
					( void )wr;
					if( during )
						continue;
					const double want = L * total / ( 2.0 * hp );
					const double got  = a[ static_cast< size_t >( pr.y ) * raster.w + pr.x ];
					if( want > 0.0 )
					{
						worst = worse( worst, mag( got / want - 1.0 ) );
						++compared;
					}
				}
			}
			worstAll = worse( worstAll, worst );
			text += fmt( " %g fps worst %.2e over %d;", fps, worst, compared );
		}
		//The tolerance: 1e-5 -- a frame's light to --dwell's 1e-5, and the
		//decay and sum, 2 float ulps a frame, over the 0.1 s persistence's
		//memory of ~6 frames at 60 fps: 1.4e-6.
		Check( worstAll <= 1e-5, fmt( "%4dx%-4d exposure against the closed form:%s (tolerance 1e-5)", raster.w, raster.h, text.c_str() ) );
	}
	return Verdict();
}

//===========================================================================
// --rate
//===========================================================================
int runRate( const Perturb& perturb )
{
	std::printf( "\n=== rate: the disc, the wheel and the lamp at 24, 30, 60 and 120 fps against 240 fps, at 0.5, 1, 1.5 and 2 s: the samples run,\n"
	             "    the lamp's state and strikes, the amplifier, the wheel's phase and speed, and the exposure. The chain runs on the\n"
	             "    transmitter's sample clock, so the rates share its grid by design: this checks the frame boundaries' bookkeeping, and\n"
	             "    that clock against N M f samples a second\n" );
	const double rates[] = { 240.0, 120.0, 60.0, 30.0, 24.0 };
	for( const Raster& raster : kRasters )
	{
		//A grey 0.5 mA over the strike at 9 mA: the sync (-1 mA) puts the
		//lamp out every line and it strikes again 250 us into the next, so
		//about one frame boundary in ten falls inside a formative lag.
		const Floats card = grey( raster.w, raster.h, codeOf( ( ( hStrike + 0.5 - 9.0 ) / hSwing + 1.0 ) / 2.0 ) );
		struct Snap
		{
			apparatus::ChainState c;
			std::vector< double > a;
		};
		std::vector< std::vector< Snap > > snaps;
		for( const double fps : rates )
		{
			Rig rig;
			if( !startRig( rig, raster.w, raster.h, perturb, &card, fps ) )
				return 1;
			//A hunting wheel off its free speed.
			rig.Set( PT_LAMP, ParamFromLamp( 9.0 ) );
			rig.Set( PT_SYNC, static_cast< int >( kHunting ) );
			rig.Set( PT_SPEED, ParamFromSpeed( 0.01 ) );
			rig.Set( PT_BANDWIDTH, ParamFromBandwidth( 9000.0 ) );
			rig.Set( PT_PERSISTENCE, ParamFromPersistence( 0.1 ) );
			std::vector< Snap > s;
			for( int q = 1; q <= 4; ++q )
			{
				//The plugin's own clock: the first frame runs the default
				//1/60 s, then each frame the host's step, so after F frames it
				//has run 1/60 + ( F - 1 ) / fps: the same time at every rate
				//when F - 1 = t fps.
				const int target = static_cast< int >( std::lround( 0.5 * q * fps ) ) + 1;
				while( rig.frame < target )
					if( !rig.Render( 1 ) )
						return 1;
				s.push_back( { rig.Chain(), rig.Exposure() } );
			}
			snaps.push_back( s );
		}
		//The anchor the comparisons cannot give (a clock wrong by the same
		//factor at every rate passes them): f pictures a second, so after
		//the plugin's t = 1/60 + 0.5 q s the transmitter has run t N M f
		//samples, to the one it is part-way through.
		double worstCount = 0.0;
		for( size_t r = 0; r < snaps.size(); ++r )
			for( size_t q = 0; q < 4; ++q )
			{
				const double t = 1.0 / 60.0 + 0.5 * static_cast< double >( q + 1 );
				worstCount     = worse( worstCount, mag( static_cast< double >( snaps[ r ][ q ].c.samples ) - t * 30.0 * hM( 30 ) * 12.5 ) );
			}
		int exactWrong = 0;
		double worstPsi = 0.0, worstDot = 0.0, worstA = 0.0;
		for( size_t r = 1; r < snaps.size(); ++r )
			for( size_t q = 0; q < 4; ++q )
			{
				const apparatus::ChainState &a = snaps[ 0 ][ q ].c, &b = snaps[ r ][ q ].c;
				exactWrong += a.samples != b.samples || a.cell != b.cell || a.amp != b.amp || a.lit != b.lit || a.strike != b.strike || a.strikes != b.strikes;
				double d = a.psi - b.psi;
				d -= std::round( d );
				worstPsi = worse( worstPsi, mag( d ) );
				worstDot = worse( worstDot, mag( a.psiDot - b.psiDot ) );
				double peak = 0.0, diff = 0.0;
				for( size_t i = 0; i < snaps[ 0 ][ q ].a.size(); ++i )
				{
					peak = std::max( peak, std::fabs( snaps[ 0 ][ q ].a[ i ] ) );
					diff = worse( diff, mag( snaps[ 0 ][ q ].a[ i ] - snaps[ r ][ q ].a[ i ] ) );
				}
				worstA = worse( worstA, diff / peak );
			}
		//Tolerances. The samples, the cell, the amplifier, the lamp and its
		//strike progress: bit-exact (the same arithmetic in the same order).
		//psi: 1e-9 rev -- psi is reduced to one revolution at frame ends,
		//so its additions round at different magnitudes: 2e5 steps x ulp(2)
		//= 4e-11, x25. The exposure: 480 frames' decay and sum, 2 float
		//ulps each (1.2e-4), and a scan split between frames computed
		//differently (1e-5, --dwell's bound): 1.3e-4.
		Check( exactWrong == 0 && worstPsi <= 1e-9 && worstDot <= 1e-9 && worstA <= 1.3e-4 && snaps[ 0 ][ 3 ].c.strikes > 100 && worstCount <= 1.0,
		       fmt( "%4dx%-4d %d exact quantities differ (bit-exact); psi worst %.1e rev (tolerance 1e-9), psi' %.1e rev/s (tolerance 1e-9); exposure worst "
		            "%.1e of its peak (tolerance 1.3e-4); %lld strikes by 2 s; the samples run %.2f from t N M f at worst (tolerance 1, the sample under way)",
		            raster.w, raster.h, exactWrong, worstPsi, worstDot, worstA, static_cast< long long >( snaps[ 0 ][ 3 ].c.strikes ), worstCount ) );
	}
	return Verdict();
}

//===========================================================================
// --colour
//===========================================================================
void hNeon( double rgb[ 3 ] )
{
	//CIE 1931 (0.665, 0.334) at Y = 1 (Lamptech), IEC 61966-2-1's matrix,
	//then mixed with white at constant luminance until nothing is negative.
	const double x = 0.665, y = 0.334;
	const double X = x / y, Y = 1.0, Z = ( 1.0 - x - y ) / y;
	const double c[ 3 ] = { 3.2406 * X - 1.5372 * Y - 0.4986 * Z, -0.9689 * X + 1.8758 * Y + 0.0415 * Z, 0.0557 * X - 0.2040 * Y + 1.0570 * Z };
	double t = 0.0;
	for( const double v : c )
		t = std::max( t, v < 0.0 ? -v / ( 1.0 - v ) : 0.0 );
	for( int i = 0; i < 3; ++i )
		rgb[ i ] = ( 1.0 - t ) * c[ i ] + t;
}

int runColour( const Perturb& perturb )
{
	std::printf( "\n=== colour: the output's linear RGB in the mapped neon's proportions; the mapping keeps the luminance and the hue line through D65\n" );
	double rgb[ 3 ];
	hNeon( rgb );
	{
		//The mapping, offline.
		const double Y  = 0.2126 * rgb[ 0 ] + 0.7152 * rgb[ 1 ] + 0.0722 * rgb[ 2 ];
		const double X  = 0.4124 * rgb[ 0 ] + 0.3576 * rgb[ 1 ] + 0.1805 * rgb[ 2 ];
		const double Z  = 0.0193 * rgb[ 0 ] + 0.1192 * rgb[ 1 ] + 0.9505 * rgb[ 2 ];
		const double cx = X / ( X + Y + Z ), cy = Y / ( X + Y + Z );
		const double cross = ( 0.665 - 0.3127 ) * ( cy - 0.3290 ) - ( 0.334 - 0.3290 ) * ( cx - 0.3127 );
		//1e-4: the published matrices' four decimals do not invert exactly.
		Check( std::fabs( Y - 1.0 ) <= 1e-4 && std::fabs( cross ) <= 1e-4 && rgb[ 2 ] >= -1e-12,
		       fmt( "the neon (0.665, 0.334) maps to linear (%.4f, %.4f, %.4f): luminance %.6f (tolerance 1e-4 of 1), chromaticity (%.4f, %.4f) off the line from D65 "
		            "by %.1e (tolerance 1e-4)",
		            rgb[ 0 ], rgb[ 1 ], rgb[ 2 ], Y, cx, cy, cross ) );
	}
	for( const Raster& raster : kRasters )
	{
		Rig rig;
		if( !startRig( rig, raster.w, raster.h, perturb ) )
			return 1;
		rig.Set( PT_LAMP, ParamFromLamp( 20.0 ) );
		if( !rig.Render( 30 ) )
			return 1;
		const Floats out = rig.Output();
		double worstG = 0.0, worstB = 0.0;
		int used      = 0;
		for( size_t p = 0; p < out.size() / 4; ++p )
		{
			const double R = linearOf( out[ 4 * p ] ), G = linearOf( out[ 4 * p + 1 ] ), B = linearOf( out[ 4 * p + 2 ] );
			if( R < 0.02 || R > 0.98 )
				continue;
			worstG = worse( worstG, mag( G / R - rgb[ 1 ] / rgb[ 0 ] ) );
			worstB = worse( worstB, mag( B ) );
			++used;
		}
		//1e-4 on G/R: the float sRGB curve out and back (1e-6) with margin.
		Check( used > 1000 && worstG <= 1e-4 && worstB <= 1e-6,
		       fmt( "%4dx%-4d %d unclipped pixels: G/R worst %.1e off %.5f (tolerance 1e-4), B worst %.1e (tolerance 1e-6)", raster.w, raster.h, used, worstG,
		            rgb[ 1 ] / rgb[ 0 ], worstB ) );
	}
	//The brightness (the spec): a lamp held at L_ref = 20 mA averages 0.5,
	//linear, at the field's centre radius. Held: a linear mid-grey puts the
	//amplifier at 1/2 and the current at the Lamp's 20 mA (the sync, at the
	//line's end, is the field's edge, not its middle). The output's linear R
	//(the neon is shown with R = 1) over a 5 x 5 patch at the frame's centre
	//(the field's middle, astride the lines either side of r_c: 1/r averages
	//to 1/r_c there to 1e-5), averaged over 600 frames at 32.725 fps after 2 s
	//(20 persistences), so the frames fall at every phase of the picture. The
	//phase each frame ends at is read off the transmitter's own clock (its
	//sample within the picture). Tolerance: the average of a periodic
	//function over N phases is off its mean by at most its variation over a
	//period times the phases' discrepancy on the circle (Koksma); the
	//variation, twice the swing seen, is doubled for margin; plus 1e-4 for
	//the float sRGB curve.
	for( const Raster& raster : kRasters )
	{
		Rig rig;
		const double fps = 32.725;// 12.5 / fps = 0.38197, the golden ratio's 2 - phi: the best spread phases
		const Floats mid = grey( raster.w, raster.h, codeOf( 0.5 ) );
		if( !startRig( rig, raster.w, raster.h, perturb, &mid, fps ) )
			return 1;
		rig.Set( PT_LAMP, ParamFromLamp( 20.0 ) );
		rig.Set( PT_PERSISTENCE, ParamFromPersistence( 0.1 ) );
		if( !rig.Render( static_cast< int >( 2.0 * fps ) ) )
			return 1;
		const int frames = 600;
		std::vector< double > phase, value;
		const int cx = raster.w / 2, cy = raster.h / 2;
		for( int f = 0; f < frames; ++f )
		{
			if( !rig.Render( 1 ) )
				return 1;
			const Floats out = rig.Output();
			double sum       = 0.0;
			for( int y = cy - 2; y <= cy + 2; ++y )
				for( int x = cx - 2; x <= cx + 2; ++x )
					sum += linearOf( out[ ( static_cast< size_t >( y ) * raster.w + x ) * 4 ] );
			value.push_back( sum / 25.0 );
			const apparatus::ChainState& c = rig.Chain();
			phase.push_back( static_cast< double >( c.cell ) / ( static_cast< double >( c.lines ) * c.samplesLine ) );
		}
		double mean = 0.0, lo = 1e9, hi = -1e9;
		for( const double v : value )
		{
			mean += v / frames;
			lo = std::min( lo, v );
			hi = std::max( hi, v );
		}
		//The discrepancy on the circle: D+ + D- of the sorted phases.
		std::sort( phase.begin(), phase.end() );
		double above = 0.0, below = 0.0;
		for( int i = 0; i < frames; ++i )
		{
			above = std::max( above, static_cast< double >( i + 1 ) / frames - phase[ static_cast< size_t >( i ) ] );
			below = std::max( below, phase[ static_cast< size_t >( i ) ] - static_cast< double >( i ) / frames );
		}
		const double discrepancy = above + below;
		const double tol         = 2.0 * ( 2.0 * ( hi - lo ) ) * discrepancy + 1e-4;
		Check( mag( mean - 0.5 ) <= tol && hi < 0.999 && hi - lo > 0.05,
		       fmt( "%4dx%-4d a lamp held at 20 mA: the field's middle averages %.5f linear over %d frames (want 0.5, tolerance %.1e: the swing %.3f to "
		            "%.3f, the phases' discrepancy %.1e)",
		            raster.w, raster.h, mean, frames, tol, lo, hi, discrepancy ) );
	}
	return Verdict();
}

//===========================================================================
// --alpha
//===========================================================================
int runAlpha( const Perturb& perturb )
{
	std::printf( "\n=== alpha: output alpha = mix( a, 1, Mix ); white under alpha 0 sends the transmitter nothing; Mix 0 hands the clip through\n" );
	for( const Raster& raster : kRasters )
	{
		const int w = raster.w, h = raster.h;
		//Left half: white at alpha 0. Right half: a grey under an alpha ramp.
		Floats card = grey( w, h, 0.5 ), blacked = grey( w, h, 0.5 );
		for( int y = 0; y < h; ++y )
			for( int x = 0; x < w; ++x )
			{
				float* o = &card[ ( static_cast< size_t >( y ) * w + x ) * 4 ];
				float* b = &blacked[ ( static_cast< size_t >( y ) * w + x ) * 4 ];
				if( x < w / 2 )
				{
					o[ 0 ] = o[ 1 ] = o[ 2 ] = 1.0f, o[ 3 ] = 0.0f;
					b[ 0 ] = b[ 1 ] = b[ 2 ] = 0.0f, b[ 3 ] = 0.0f;
				}
				else
					o[ 3 ] = b[ 3 ] = std::round( 255.0f * ( 2.0f * x / w - 1.0f ) ) / 255.0f;
			}
		//The same light sent opaque: black on the left, and on the right the
		//grey's linear light times its alpha, encoded back to a code value.
		Floats weighted = grey( w, h, 0.0 );
		for( int y = 0; y < h; ++y )
			for( int x = w / 2; x < w; ++x )
			{
				float* o      = &weighted[ ( static_cast< size_t >( y ) * w + x ) * 4 ];
				const float a = card[ ( static_cast< size_t >( y ) * w + x ) * 4 + 3 ];
				o[ 0 ] = o[ 1 ] = o[ 2 ] = static_cast< float >( codeOf( linearOf( 0.5f ) * a ) );
			}
		Rig rig, ref, opaque;
		if( !startRig( rig, w, h, perturb, &card ) || !startRig( ref, w, h, perturb, &blacked ) || !startRig( opaque, w, h, perturb, &weighted ) )
			return 1;
		if( !opaque.Render( 3 ) )
			return 1;
		double worst[ 2 ] = {}, weightErr = 0.0, peak = 0.0;
		const float mixes[ 2 ] = { 1.0f, 0.5f };
		size_t underAlpha = 0;
		for( int k = 0; k < 2; ++k )
		{
			rig.Set( PT_MIX, mixes[ k ] );
			ref.Set( PT_MIX, mixes[ k ] );
			if( !rig.Render( 3 ) || !ref.Render( 3 ) )
				return 1;
			const Floats out = rig.Output();
			for( size_t p = 0; p < out.size() / 4; ++p )
			{
				const double a = card[ 4 * p + 3 ];
				worst[ k ]     = worse( worst[ k ], mag( out[ 4 * p + 3 ] - ( a + ( 1.0 - a ) * mixes[ k ] ) ) );
			}
			const std::vector< double > ea = rig.Exposure(), eb = ref.Exposure();
			for( size_t i = 0; i < ea.size(); ++i )
				underAlpha += ea[ i ] != eb[ i ];
			if( k == 0 )
			{
				const std::vector< double > ew = opaque.Exposure();
				for( size_t i = 0; i < ea.size(); ++i )
				{
					weightErr = worse( weightErr, mag( ea[ i ] - ew[ i ] ) );
					peak      = std::max( peak, ew[ i ] );
				}
			}
		}
		//The weighting's tolerance, 1e-5 of the brightest pixel: the sRGB
		//curve out and back in float holds the light to 1e-6, and the current
		//(5 to 25 mA at the 15 mA Lamp) carries it with no gain above 2.
		weightErr /= std::max( peak, 1e-30 );
		rig.Set( PT_MIX, 0.0f );
		if( !rig.Render( 1 ) )
			return 1;
		const size_t through = differing( rig.Output(), card );
		Check( worst[ 0 ] <= 1e-6 && worst[ 1 ] <= 1e-6 && underAlpha == 0 && through == 0 && weightErr <= 1e-5 && peak > 0.0,
		       fmt( "%4dx%-4d alpha at Mix 1 off mix( a, 1, Mix ) by %.2g, at Mix 0.5 by %.2g (tolerance 1e-6); white under alpha 0 against black there: %zu "
		            "exposure values differ (bit-exact); a grey under alpha a against its light x a sent opaque: %.1e of the brightest pixel (tolerance 1e-5); "
		            "Mix 0: %zu values differ from the clip (bit-exact)",
		            w, h, worst[ 0 ], worst[ 1 ], underAlpha, weightErr, through ) );
	}
	return Verdict();
}

//===========================================================================
// --resize
//===========================================================================
int runResize( const Perturb& perturb )
{
	std::printf( "\n=== resize: 1280x720 -> 320x180 mid-run, the lamp striking every line and the wheel hunting: the chain carries on as in a run\n"
	             "    never resized, and the screen's exposure is carried over by area\n" );
	//A flat mid-grey: the transmitter sees the same picture at both rasters,
	//and every line pulls the wheel alike. (The card's lines pull it from 4
	//to 37 mA^2, a modulation that repeats at the picture rate, 12.5 Hz,
	//twice the wheel's own frequency at that pull: a parametric resonance,
	//and the wheel never settles. Two runs whose tables differ in float
	//rounding then part exponentially, 1e-7 to 3e-3 rev in 75 frames.)
	const Floats small = grey( 320, 180, 0.5 ), big = grey( 1280, 720, 0.5 );
	Rig moved, kept, native;
	if( !startRig( moved, 1280, 720, perturb, &big ) || !startRig( kept, 1280, 720, perturb, &big ) || !startRig( native, 320, 180, perturb, &small ) )
		return 1;
	for( Rig* r : { &moved, &kept, &native } )
	{
		r->Set( PT_LAMP, ParamFromLamp( 9.0 ) );
		r->Set( PT_SYNC, static_cast< int >( kHunting ) );
		r->Set( PT_SPEED, ParamFromSpeed( 0.01 ) );
		r->Set( PT_PERSISTENCE, ParamFromPersistence( 0.1 ) );
	}
	if( !moved.Render( 45 ) || !kept.Render( 45 ) || !native.Render( 45 ) )
		return 1;
	const apparatus::ChainState before = moved.Chain();
	if( !moved.Resize( 320, 180, &small ) )
		return 1;
	if( !moved.Render( 1 ) || !kept.Render( 1 ) || !native.Render( 1 ) )
		return 1;
	auto total = []( const std::vector< double >& a, double area ) {
		double s = 0.0;
		for( const double v : a )
			s += v * area;
		return s;
	};
	const double lightMoved = total( moved.Exposure(), 16.0 ), lightNative = total( native.Exposure(), 16.0 ), lightKept = total( kept.Exposure(), 1.0 );
	//The wheel's swing about its lock at the resize, read off the run kept
	//at 1280x720 over the 30 frames after it (psi's extremes either way).
	double worstPsi = 0.0, pullApart = 0.0, swingLo = 1.0, swingHi = -1.0, pull = 0.0;
	int lampWrong   = 0;
	auto apart      = []( double p, double q ) {
        double d = p - q;
        return mag( d - std::round( d ) );
	};
	auto centred = []( double p ) { return p - std::round( p ); };
	const double framing = 0.0;// the default Framing: the lock is near psi = 0
	for( int f = 0; f < 30; ++f )
	{
		if( !moved.Render( 1 ) || !kept.Render( 1 ) || !native.Render( 1 ) )
			return 1;
		const apparatus::ChainState &a = moved.Chain(), &b = kept.Chain();
		worstPsi  = worse( worstPsi, apart( a.psi, b.psi ) );
		pullApart = worse( pullApart, mag( a.pull / b.pull - 1.0 ) );
		swingLo   = std::min( swingLo, centred( b.psi - framing ) );
		swingHi   = std::max( swingHi, centred( b.psi - framing ) );
		pull      = b.pull;
		lampWrong += a.lit != b.lit || a.strikes != b.strikes || a.samples != b.samples;
	}
	//Tolerances. The light on the screen: 1e-4 -- area resampling
	//conserves it, each raster's light is good to 1e-5 a pixel (--dwell's
	//bound) and its field's light to the same, and the lattices tile the
	//field's edges differently. The wheel: after the resize the table is
	//summed over the other raster's pixels, and the pull differs by float
	//rounding, delta (read off the two runs' own pulls). A pull delta
	//stronger moves the hunt's frequency by delta / 2 and its centre by up
	//to delta of the swing A (psi's extent over the 30 frames), so over t
	//the runs part by at most A delta ( omega t / 2 + 1 ); the tolerance is
	//twice that. A wheel restarted at the framing is A away at once. The
	//lamp and the samples: exact.
	const double lightErr = lightMoved / lightNative - 1.0;
	const double swing    = swingHi - swingLo;
	const double omega    = 2.0 * hPi * hHunt * std::sqrt( pull / apparatus::kReferencePull );
	const double t        = 30.0 / kept.fps;
	const double psiTol   = 2.0 * swing * pullApart * ( 0.5 * omega * t + 1.0 ) + 1e-12;
	Check( mag( lightErr ) <= 1e-4 && worstPsi <= psiTol && lampWrong == 0 && before.strikes > 100 && swing > 1e-4,
	       fmt( "the frame after the resize: the screen's light %.6g (resized) against %.6g (320x180 throughout), %+.2e (tolerance 1e-4; 1280x720 kept: "
	            "%.6g); 30 frames on, the wheel %.1e rev from the run kept at 1280x720 (tolerance %.1e: its swing %.2e rev, the pulls %.1e apart, "
	            "omega %.1f rad/s over %.2f s), the lamp and the samples %d frames off (exact); %lld strikes before",
	            lightMoved, lightNative, lightErr, lightKept, worstPsi, psiTol, swing, pullApart, omega, t, lampWrong,
	            static_cast< long long >( before.strikes ) ) );
	return Verdict();
}

//===========================================================================
// --clock
//===========================================================================
int runClock( const Perturb& perturb )
{
	std::printf( "\n=== clock: a host clock at 4.99e8 ms gives the pictures a clock from 0 s does, bit for bit (same context), and a clock left\n"
	             "    to settle its own unit settles on the right one\n" );
	for( const Raster& raster : kRasters )
	{
		Rig zero, late;
		if( !startRig( zero, raster.w, raster.h, perturb ) || !startRig( late, raster.w, raster.h, perturb ) )
			return 1;
		late.plugin.SetClockScaleForTest( 0.001 );
		late.timeOffset = 4.99e8;
		late.timeScale  = 1000.0;
		for( Rig* r : { &zero, &late } )
		{
			r->Set( PT_LAMP, ParamFromLamp( 9.0 ) );
			r->Set( PT_SYNC, static_cast< int >( kHunting ) );
			r->Set( PT_SPEED, ParamFromSpeed( 0.005 ) );
		}
		size_t differ = 0, values = 0;
		for( int f = 0; f < 120; ++f )
		{
			if( f == 40 )
				for( Rig* r : { &zero, &late } )
					r->Set( PT_FRAMING, ParamFromFraming( 0.2 ) );
			if( !zero.Render( 1 ) || !late.Render( 1 ) )
				return 1;
			if( f % 30 == 29 )
			{
				const Floats a = zero.Output(), b = late.Output();
				differ += differing( a, b );
				values += a.size();
			}
		}
		Check( differ == 0, fmt( "%4dx%-4d 120 frames, the framing stepped at frame 40: %zu of %zu values differ at frames 30, 60, 90, 120 (bit-exact)", raster.w,
		                         raster.h, differ, values ) );

		for( const double unit : { 1.0, 1000.0 } )
		{
			Rig r;
			if( !startRig( r, raster.w, raster.h, perturb ) )
				return 1;
			r.plugin.SetClockScaleForTest( 0.0 );
			r.timeOffset = unit == 1.0 ? 12.5 : 4.99e8;
			r.timeScale  = unit;
			if( !r.Render( 60 ) )
				return 1;
			const double want = 1.0 / unit;
			const double got  = r.plugin.ClockScale();
			const double host = ( r.timeOffset + r.timeScale * r.Seconds( r.frame - 1 ) ) * want;
			Check( got == want && std::fabs( r.plugin.HostNow() - host ) < 1e-6 && std::fabs( r.plugin.Interval() - 1.0 / 60.0 ) < 1e-6,
			       fmt( "%4dx%-4d a host clock in %s at 60 fps, rendered faster than real time: settled on %s by frame 60, reads %.6f s (host %.6f s), runs "
			            "%.6f s a frame",
			            raster.w, raster.h, unit == 1.0 ? "seconds" : "milliseconds", got == 1.0 ? "seconds" : ( got == 0.001 ? "milliseconds" : "nothing" ),
			            r.plugin.HostNow(), host, r.plugin.Interval() ) );
		}
	}
	return Verdict();
}

//===========================================================================
// --glstate
//===========================================================================
int runGLState( const Perturb& perturb )
{
	std::printf( "\n=== glstate: the host's GL state comes back as it was handed over -- the pixel-transfer state the table's read-back uses included\n" );
	for( const Raster& raster : kRasters )
	{
		Rig rig;
		if( !startRig( rig, raster.w, raster.h, perturb ) )
			return 1;
		GLuint vao = 0, buffer = 0, pack = 0;
		glGenVertexArrays( 1, &vao );
		glGenBuffers( 1, &buffer );
		glGenBuffers( 1, &pack );
		glBindBuffer( GL_PIXEL_PACK_BUFFER, pack );
		glBufferData( GL_PIXEL_PACK_BUFFER, 64, nullptr, GL_STREAM_READ );
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
			glBindBuffer( GL_PIXEL_PACK_BUFFER, pack );
			glPixelStorei( GL_PACK_ALIGNMENT, 1 );
			glPixelStorei( GL_UNPACK_ALIGNMENT, 2 );
			glPixelStorei( GL_PACK_ROW_LENGTH, 7 );
			glActiveTexture( GL_TEXTURE3 );
			rig.plugin.SetTime( rig.Seconds( frame ) );
			if( rig.plugin.ProcessOpenGL( &rig.process ) != FF_SUCCESS )
				return 1;
			GLint vp[ 4 ], src = 0, dst = 0, srcA = 0, dstA = 0, va = 0, ab = 0, at = 0, pb = 0, pa = 0, ua = 0, prl = 0;
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
			glGetIntegerv( GL_PIXEL_PACK_BUFFER_BINDING, &pb );
			glGetIntegerv( GL_PACK_ALIGNMENT, &pa );
			glGetIntegerv( GL_UNPACK_ALIGNMENT, &ua );
			glGetIntegerv( GL_PACK_ROW_LENGTH, &prl );
			wrong += vp[ 0 ] != 3 || vp[ 1 ] != 5 || vp[ 2 ] != raster.w - 7 || vp[ 3 ] != raster.h - 9;
			wrong += !glIsEnabled( GL_BLEND ) || src != GL_SRC_ALPHA || dst != GL_ONE || srcA != GL_ONE || dstA != GL_ZERO;
			wrong += cc[ 0 ] != 0.1f || cc[ 1 ] != 0.2f || cc[ 2 ] != 0.3f || cc[ 3 ] != 0.4f;
			wrong += va != static_cast< GLint >( vao ) || ab != static_cast< GLint >( buffer ) || at != GL_TEXTURE3;
			wrong += pb != static_cast< GLint >( pack ) || pa != 1 || ua != 2 || prl != 7;
			wrong += glGetError() != GL_NO_ERROR;
		}
		glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
		glPixelStorei( GL_PACK_ALIGNMENT, 4 );
		glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		glPixelStorei( GL_PACK_ROW_LENGTH, 0 );
		glBindVertexArray( 0 );
		glDisable( GL_BLEND );
		glActiveTexture( GL_TEXTURE0 );
		glDeleteVertexArrays( 1, &vao );
		glDeleteBuffers( 1, &buffer );
		glDeleteBuffers( 1, &pack );
		//The table read back right with a pixel buffer bound: a grey of
		//0.5 linear gives 0.5 in the table's middle.
		Check( wrong == 0, fmt( "%4dx%-4d three frames with a host viewport, blend, clear colour, vertex array, buffer, texture unit, a pixel-pack buffer "
		                        "and pack/unpack alignment of its own: %d things not put back, GL errors included",
		                        raster.w, raster.h, wrong ) );
	}
	return Verdict();
}

//===========================================================================
// --geometry (offline)
//===========================================================================
int runGeometry( const Perturb& perturb )
{
	std::printf( "\n=== geometry: the conversions, the disc's dimensions, the lamp's lag, the colour and the pull-out, restated; and the chain on\n"
	             "    a synthetic table -- the amplifier's step withholds tau of light, the lamp strikes after its lag (no GL)\n" );
	//Conversions round-trip and match the spec's ranges.
	double worstConv = 0.0;
	for( const float v : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f } )
	{
		worstConv = worse( worstConv, mag( RateFromParam( v ) / hRate( v ) - 1.0 ), mag( BandwidthFromParam( v ) / hBandwidth( v ) - 1.0 ),
		                   mag( LampFromParam( v ) - hLamp( v ) ), mag( SpeedFromParam( v ) - hSpeed( v ) ), mag( FramingFromParam( v ) - hFraming( v ) ),
		                   mag( PersistenceFromParam( v ) / hPersistence( v ) - 1.0 ) );
		worstConv = worse( worstConv, mag( ParamFromRate( RateFromParam( v ) ) - v ), mag( ParamFromBandwidth( BandwidthFromParam( v ) ) - v ),
		                   mag( ParamFromLamp( LampFromParam( v ) ) - v ), mag( ParamFromSpeed( SpeedFromParam( v ) ) - v ),
		                   mag( ParamFromFraming( FramingFromParam( v ) ) - v ), mag( ParamFromPersistence( PersistenceFromParam( v ) ) - v ) );
	}
	const bool lines = LinesFromParam( 7.0f ) == 8 && LinesFromParam( 30.4f ) == 30 && LinesFromParam( 500.0f ) == 120 && OptionIndex( 0.6f, 3 ) == 1;
	Check( worstConv <= 1e-6 && lines, fmt( "conversions against the spec's ranges and back: worst %.1e (tolerance 1e-6, float); Lines clamps to 8..120 and rounds", worstConv ) );

	double worstDisc = 0.0;
	for( int n = 8; n <= 120; ++n )
	{
		worstDisc = worse( worstDisc, mag( apparatus::CentreRadius( n ) / hRc( n ) - 1.0 ), mag( apparatus::CentreRadius( n ) * apparatus::HoleAngle( n ) / ( 7.0 * n / 3.0 ) - 1.0 ),
		                   mag( apparatus::HoleRadius( n, 0 ) - ( hRc( n ) + 0.5 * ( n - 1 ) ) ), mag( static_cast< double >( apparatus::SamplesPerLine( n ) - hM( n ) ) ) );
	}
	const double ratio30 = hRk( 30, 0 ) / hRk( 30, 29 );
	Check( worstDisc <= 1e-12 && std::fabs( hRc( 30 ) - 334.225 ) < 1e-3,
	       fmt( "the disc: r_c = 7N^2/(6 pi) (%.3f holes at 30 lines), r_c Phi = 7N/3, hole 0 at r_c + (N-1)/2, M = 4 ceil(7N/3), N = 8..120: worst %.1e "
	            "(tolerance 1e-12); outer over inner hole %.4f",
	            hRc( 30 ), worstDisc, ratio30 ) );

	const double lag0 = apparatus::FormativeLag( 1e-9 ), lag65 = apparatus::FormativeLag( 65.0 ), lagHi = apparatus::FormativeLag( 200.0 );
	Check( std::fabs( lag0 / 300e-6 - 1.0 ) < 1e-6 && std::fabs( lag65 / 25e-6 - 1.0 ) < 1e-9 && lagHi == 10e-6 && std::fabs( apparatus::kStrikeCurrent - hStrike ) < 1e-12 &&
	           std::isinf( apparatus::FormativeLag( 0.0 ) ),
	       fmt( "the lamp: strikes above %.3f mA; formative lag %.2f us just over breakdown, %.3f us at 65 V over, %.1f us floor (GE, Signalite)", apparatus::kStrikeCurrent,
	            1e6 * lag0, 1e6 * lag65, 1e6 * lagHi ) );

	double plugin[ 3 ], mine[ 3 ];
	apparatus::NeonLinear( plugin );
	hNeon( mine );
	Check( std::fabs( plugin[ 0 ] - mine[ 0 ] ) < 1e-9 && std::fabs( plugin[ 1 ] - mine[ 1 ] ) < 1e-9 && std::fabs( plugin[ 2 ] - mine[ 2 ] ) < 1e-9,
	       fmt( "the neon in linear sRGB: (%.4f, %.4f, %.4f), shown as (1, %.5f, %.5f)", plugin[ 0 ], plugin[ 1 ], plugin[ 2 ], plugin[ 1 ] / plugin[ 0 ], plugin[ 2 ] / plugin[ 0 ] ) );

	const double pull = apparatus::PullOut( 30, 12.5, apparatus::kReferencePull );
	const double want = hTauM * std::pow( 2.0 * hPi * hHunt, 2.0 ) / ( 2.0 * hPi * 30.0 * 12.5 );
	Check( std::fabs( pull / want - 1.0 ) < 1e-12, fmt( "pull-out at the reference pull, 30 lines, 12.5 pictures: %.4f%% of the speed", 100.0 * pull ) );

	//The chain on a synthetic table: a step from black to white at a line's
	//middle, the lamp held high so it never goes out. The light withheld
	//after the step is 20 mA x tau.
	{
		const int n = 30, M = hM( n );
		std::vector< float > table( static_cast< size_t >( n ) * M );
		for( int k = 0; k < n; ++k )
			for( int j = 0; j < M; ++j )
				table[ static_cast< size_t >( k ) * M + j ] = j >= M / 2 ? 1.0f : 0.0f;
		apparatus::ChainSettings s;
		s.lines       = n;
		s.rate        = 12.5;
		s.lampMa      = 25.0;
		s.sync        = 0;
		s.persistence = 1e9;
		std::string text;
		double worst = 0.0;
		for( const double band : { 2000.0, 9000.0 } )
		{
			s.bandwidth = band;
			apparatus::ChainState state;
			apparatus::FrameOut out;
			//A whole picture, then one line read.
			apparatus::RunFrame( state, s, table.data(), 0.08, perturb.chain, out );
			apparatus::RunFrame( state, s, table.data(), 1.0 / ( n * 12.5 ), perturb.chain, out );
			double withheld = 0.0;
			for( int i = M / 2; i < out.count && i < M - hSync; ++i )
				withheld += ( 35.0 - out.samples[ static_cast< size_t >( i ) ].light ) * out.sampleSeconds;
			const double tau = 1.0 / ( 2.0 * hPi * band );
			worst            = worse( worst, mag( withheld / ( 20.0 * tau ) - 1.0 ) );
			text += fmt( " %g Hz: %.4f us of light withheld against tau %.4f us;", band, 1e6 * withheld / 20.0, 1e6 * tau );
		}
		//The tolerance: 1e-5 -- the lights are floats (6e-8 of 35 mA a
		//sample, ~140 samples, against 20 mA x tau: 7e-6 at 9 kHz); the
		//step's residue at the sync is e^-16 at 2 kHz.
		Check( worst <= 1e-5, fmt( "the amplifier's step:%s worst %.1e (tolerance 1e-5)", text.c_str(), worst ) );
	}
	{
		//The lamp: out on black (2 mA under the strike), struck by a step to a
		//current 30 V over; its first light comes the lag later.
		const int n = 30, M = hM( n );
		std::vector< float > table( static_cast< size_t >( n ) * M );
		const double lamp = 8.0, target = hStrike + 3.0, ell = ( ( target - lamp ) / hSwing + 1.0 ) / 2.0;
		for( int k = 0; k < n; ++k )
			for( int j = 0; j < M; ++j )
				table[ static_cast< size_t >( k ) * M + j ] = j >= M / 2 ? static_cast< float >( ell ) : 0.0f;
		apparatus::ChainSettings s;
		s.lines       = n;
		s.rate        = 12.5;
		s.lampMa      = lamp;
		s.sync        = 0;
		s.bandwidth   = 40000.0;
		s.persistence = 1e9;
		apparatus::ChainState state;
		apparatus::FrameOut out;
		apparatus::RunFrame( state, s, table.data(), 1.0 / ( n * 12.5 ), perturb.chain, out );
		double dark = 0.0;
		for( int i = M / 2; i < out.count && i < M - hSync; ++i )
			dark += ( target - out.samples[ static_cast< size_t >( i ) ].light ) * out.sampleSeconds;
		dark /= target;
		//The harness's lamp on the same step: the amplifier's rise, the lag
		//run on the instantaneous overvoltage, light = current.
		double strikeAt   = 0.0;
		const double want = missingTime( lamp, ell, 1.0 / ( 2.0 * hPi * s.bandwidth ), 1e-12, strikeAt );
		//The tolerance: one sample (9.5 us) -- the plugin runs the lag on
		//each sample's average current, held, and strikes within a sample.
		const double dtS = 1.0 / ( n * M * 12.5 );
		Check( std::fabs( dark - want ) <= dtS,
		       fmt( "the lamp out on black, a step 30 V over (lag %.2f us): light missing for %.2f us against the model's %.2f us (it strikes %.2f us after the "
		            "step; tolerance %.1f us, one sample)",
		            1e6 * hLag( 30.0 ), 1e6 * dark, 1e6 * want, 1e6 * strikeAt, 1e6 * dtS ) );
	}
	{
		//The strike within its sample: a lamp switched on at 2 mA (under the
		//strike, so out) with the picture already at a grey that holds the
		//current at 30 V and at 60 V over from the first sample (the
		//amplifier starts settled). The lag is then a constant: the lamp
		//must light exactly the lag after the start, wherever that falls in
		//a sample (10.006 and 3.18 samples here). Read off the samples'
		//light: lit time = the light summed over the line's picture, over the
		//current. Tolerance 1 ns: the light is stored in float (6e-8 of
		//2.6 ms is 0.2 ns); a strike rounded to its sample is up to 9.5 us off.
		const int n = 30, M = hM( n );
		const double lamp = 2.0, dtS = 1.0 / ( n * M * 12.5 );
		double worst = 0.0;
		std::string text;
		for( const double over : { 30.0, 60.0 } )
		{
			const double target = hStrike + over / 10.0, ell = ( ( target - lamp ) / hSwing + 1.0 ) / 2.0;
			std::vector< float > table( static_cast< size_t >( n ) * M, static_cast< float >( ell ) );
			apparatus::ChainSettings s;
			s.lines       = n;
			s.rate        = 12.5;
			s.lampMa      = lamp;
			s.sync        = 0;
			s.bandwidth   = 40000.0;
			s.persistence = 1e9;
			apparatus::ChainState state;
			apparatus::FrameOut out;
			apparatus::RunFrame( state, s, table.data(), 1.0 / ( n * 12.5 ), perturb.chain, out );
			const double I = lamp + hSwing * ( 2.0 * static_cast< double >( table[ 0 ] ) - 1.0 );
			double lit     = 0.0;
			for( int i = 0; i < out.count && i < M - hSync; ++i )
				lit += out.samples[ static_cast< size_t >( i ) ].light * out.sampleSeconds;
			const double first = ( M - hSync ) * out.sampleSeconds - lit / I;
			const double lag   = hLag( 10.0 * ( I - hStrike ) );
			worst              = worse( worst, mag( first - lag ) );
			text += fmt( " %g V over: first light %.4f us after the start against the lag %.4f us (%.3f samples);", over, 1e6 * first, 1e6 * lag, lag / dtS );
		}
		Check( worst <= 1e-9, fmt( "the strike within its sample, the lamp held at a constant current:%s worst %.1e s (tolerance 1e-9 s)", text.c_str(), worst ) );
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
		{ "geometry", runGeometry, true },
		{ "spiral", runSpiral, false },
		{ "slip", runSlip, false },
		{ "arc", runArc, false },
		{ "dwell", runDwell, false },
		{ "bandwidth", runBandwidth, false },
		{ "lamp", runLamp, false },
		{ "hunting", runHunting, false },
		{ "flicker", runFlicker, false },
		{ "rate", runRate, false },
		{ "colour", runColour, false },
		{ "alpha", runAlpha, false },
		{ "resize", runResize, false },
		{ "clock", runClock, false },
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

//===========================================================================
// --negative
//===========================================================================
int runNegative( bool offlineOnly = false )
{
	struct Case
	{
		const char* name;
		CheckFn check;
		Perturb perturb;
		const char* what;
	};
	std::vector< Case > cases;
	auto add = [ & ]( const char* name, CheckFn fn, const char* what, std::function< void( Perturb& ) > set ) {
		Perturb p;
		set( p );
		cases.push_back( { name, fn, p, what } );
	};
	add( "geometry", runGeometry, "tau = 1 / B, not 1 / ( 2 pi B )", []( Perturb& p ) { p.chain.tauScale = 2.0 * hPi; } );
	add( "geometry", runGeometry, "an instant strike", []( Perturb& p ) { p.chain.instantStrike = true; } );
	add( "spiral", runSpiral, "holes on a spiral 2% too steep", []( Perturb& p ) { p.holeSpacing = 1.02f; } );
	add( "spiral", runSpiral, "every hole on one circle", []( Perturb& p ) { p.holeSpacing = 0.0f; } );
	add( "slip", runSlip, "the receiver's phase advanced by psi, not N psi", []( Perturb& p ) { p.chain.phaseNotTimesN = true; } );
	add( "arc", runArc, "the drum's lines explored along the disc's arcs", []( Perturb& p ) { p.drumAsDisc = true; } );
	add( "dwell", runDwell, "every hole the same angular width (no 1/r)", []( Perturb& p ) { p.holeWidthModel = 1; } );
	add( "bandwidth", runBandwidth, "tau = 1 / B", []( Perturb& p ) { p.chain.tauScale = 2.0 * hPi; } );
	add( "lamp", runLamp, "an instant strike", []( Perturb& p ) { p.chain.instantStrike = true; } );
	add( "lamp", runLamp, "light proportional to the current squared", []( Perturb& p ) { p.chain.lightSquare = true; } );
	add( "hunting", runHunting, "the pull not scaled by C", []( Perturb& p ) { p.chain.pullUnscaled = true; } );
	add( "hunting", runHunting, "the motor's damping of the wrong sign", []( Perturb& p ) { p.chain.dampingSign = -1.0; } );
	add( "flicker", runFlicker, "at most one scan of a band a frame", []( Perturb& p ) { p.scanModel = 1; } );
	add( "rate", runRate, "the disc advanced by 1/60 s a frame, whatever the frame", []( Perturb& p ) { p.chain.fixedFrameStep = true; } );
	add( "rate", runRate, "the lamp's strike forgotten at every frame", []( Perturb& p ) { p.chain.strikeResetEachFrame = true; } );
	add( "colour", runColour, "the lamp drawn in white", []( Perturb& p ) { p.whiteScreen = true; } );
	add( "colour", runColour, "the display scaled without its pi", []( Perturb& p ) { p.displayNoPi = true; } );
	add( "alpha", runAlpha, "the clip's alpha passed through", []( Perturb& p ) { p.alphaModel = 1; } );
	add( "alpha", runAlpha, "the clip's light not weighted by its alpha", []( Perturb& p ) { p.premultiply = false; } );
	add( "resize", runResize, "a new raster clears the exposure", []( Perturb& p ) { p.resizeResets = true; } );
	add( "resize", runResize, "a new raster restarts the wheel at the framing", []( Perturb& p ) { p.resizeWheel = true; } );
	add( "resize", runResize, "a new raster puts the lamp out", []( Perturb& p ) { p.resizeLamp = true; } );
	add( "clock", runClock, "the host's clock read in single precision", []( Perturb& p ) { p.floatClock = true; } );
	add( "glstate", runGLState, "the host's GL state not put back", []( Perturb& p ) { p.leakGLState = true; } );

	if( offlineOnly )
		cases.erase( std::remove_if( cases.begin(), cases.end(), []( const Case& c ) { return !isOffline( c.name ); } ), cases.end() );

	int unfalsifiable = 0;
	for( const Case& c : cases )
	{
		std::printf( "\n=== negative control: %s -- %s\n", c.name, c.what );
		const int before = g_failures;
		g_failures       = 0;
		c.check( c.perturb );
		const int observed = g_failures;
		g_failures         = before;
		if( observed > 0 )
			std::printf( "  ok    %s failed %d check%s, as it must\n", c.name, observed, observed == 1 ? "" : "s" );
		else
		{
			std::printf( "  FAIL  %s PASSED against a wrong model -- it cannot fail, so it is not a check\n", c.name );
			++unfalsifiable;
		}
	}
	std::printf( "\nnegative controls: %zu wrong models, %d of them undetected\n", cases.size(), unfalsifiable );
	std::printf( "\n  %s\n", unfalsifiable == 0 ? "PASS" : "FAIL" );
	return unfalsifiable == 0 ? 0 : 1;
}

int runOffline()
{
	int failed = 0;
	for( const CheckEntry& check : checks() )
		if( check.offline )
			failed |= check.run( Perturb {} );
	failed |= runNegative( true );
	std::printf( "\n  offline: the checks that need no GL context. The picture checks were NOT run --\n"
	             "  tools/verify.sh runs them against a real driver, at 1280x720 and 320x180, and again on the software renderer.\n"
	             "\n  %s\n",
	             failed == 0 ? "PASS" : "FAIL" );
	return failed == 0 ? 0 : 1;
}

//===========================================================================
// --bench: GPU time per frame from GL_TIME_ELAPSED, and the CPU's.
//===========================================================================
int runBench()
{
	struct Size
	{
		int w, h;
		const char* name;
		int warm, timed;
	};
	const Size sizes[] = { { 1280, 720, "720p", 60, 120 }, { 1920, 1080, "1080p", 60, 120 }, { 3840, 2160, "4K", 20, 30 } };
	std::printf( "\n=== bench: per frame at 60 fps, the defaults on the card: GPU time (GL_TIME_ELAPSED) and the CPU's wall time in ProcessOpenGL\n"
	             "    (the table's read-back waits for the table pass, so the CPU's includes it)\n" );
	GLuint query = 0;
	glGenQueries( 1, &query );
	for( const Size& size : sizes )
	{
		Rig rig;
		if( !rig.Init( size.w, size.h ) )
			return 1;
		if( !rig.Render( size.warm ) )
			return 1;
		std::vector< double > gpu, cpu;
		for( int f = 0; f < size.timed; ++f )
		{
			glFinish();
			const auto start = std::chrono::steady_clock::now();
			glBeginQuery( GL_TIME_ELAPSED, query );
			if( !rig.Render( 1 ) )
				return 1;
			glEndQuery( GL_TIME_ELAPSED );
			const auto end = std::chrono::steady_clock::now();
			GLuint64 ns    = 0;
			glGetQueryObjectui64v( query, GL_QUERY_RESULT, &ns );
			gpu.push_back( static_cast< double >( ns ) * 1e-6 );
			cpu.push_back( std::chrono::duration< double, std::milli >( end - start ).count() );
		}
		std::sort( gpu.begin(), gpu.end() );
		std::sort( cpu.begin(), cpu.end() );
		std::printf( "  %-6s GPU median %6.2f ms, worst %6.2f; CPU median %6.2f ms, worst %6.2f (%4.1f%% of a 60 fps frame, the larger median)\n", size.name,
		             gpu[ gpu.size() / 2 ], gpu.back(), cpu[ cpu.size() / 2 ], cpu.back(),
		             100.0 * std::max( gpu[ gpu.size() / 2 ], cpu[ cpu.size() / 2 ] ) / ( 1000.0 / 60.0 ) );
	}
	glDeleteQueries( 1, &query );
	return 0;
}
