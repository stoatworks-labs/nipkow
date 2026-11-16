#pragma once

#include <string>

/**
    The passes, in the order a frame runs them. Every one is a full-raster
    triangle into a buffer of its own; nothing is blended.

    1. **table** -- the transmitter: N rows of M samples, each the clip's
       linear luminance x alpha averaged over the square aperture at the
       sample's place on the drum's straight line (or the disc's arc). Read
       back to the CPU, which runs the amplifier, the lamp and the wheel
       (Apparatus.h) and uploads the frame's samples.
    2. **exposure** -- the screen: per pixel, the light that reached it
       through the receiving disc's holes over the frame, from every scan of
       its band within the frame, the hole's window and the pixel's footprint
       along the arc integrated in closed form; added to the last frame's
       exposure decayed by e^( -T / Persistence ). Ping-ponged (three buffers,
       so a resize can resample the exposure into a fresh one).
    3. **resample** -- on a change of raster only: the exposure carried over,
       each new pixel taking the share of every old pixel it overlaps.
    4. **composite** -- to the host: the exposure x the display's scale, in
       the neon's colour; Mix to the clip in code values; alpha
       mix( a, 1, Mix ).

    No time reaches a shader: the samples carry the receiver's phase relative
    to the frame, reduced in double on the CPU. tools/glslc.sh's ASSEMBLED
    table mirrors Assemble().
*/
namespace nipkow::shaders
{

extern const char* const kVersion;
extern const char* const kQuadVertex;
extern const char* const kCommon;
extern const char* const kTableFragment;
extern const char* const kExposureFragment;
extern const char* const kResampleFragment;
extern const char* const kCompositeFragment;

/// The samples texture's width: sample i is at ( i & 4095, i >> 12 ).
constexpr int kSampleRowShift = 12;
constexpr int kSampleRow      = 1 << kSampleRowShift;

enum class Pass
{
	Table,
	Exposure,
	Resample,
	Composite
};
std::string AssembleVertex( Pass pass );
std::string Assemble( Pass pass );

} // namespace nipkow::shaders
