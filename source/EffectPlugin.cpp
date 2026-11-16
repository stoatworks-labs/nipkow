#include "Nipkow.h"

/**
    The registration. `nipkow_core` is an OBJECT library so this file-scope
    constructor, which nothing references by name, is linked into the bundle
    (an archive would let the linker drop it, and the bundle would load and
    report no plugins).
*/
static CFFGLPluginInfo PluginInfo(
	PluginFactory< nipkow::NipkowPlugin >,// Create method
	"NK01",                               // Plugin unique ID of maximum length 4
	"SW Nipkow",                          // Plugin name
	2,                                    // API major version number
	1,                                    // API minor version number
	0,                                    // Plugin major version number
	1,                                    // Plugin minor version number
	FF_EFFECT,                            // Plugin type
	"Baird's 30-line mechanical television looking at the clip. A mirror drum explores it in 30 vertical strips, 12 1/2 "
	"pictures a second; the signal goes through a narrow amplifier into a neon lamp, and a spinning Nipkow disc draws it "
	"back one hole at a time: bowed lines, a picture taller and dimmer at its outer edge, smear up the lines, the lamp's "
	"orange glow and flicker, and a phonic wheel that holds the picture in step and hunts.",
	"Nipkow FFGL effect"// About
);

extern "C" const char* NipkowBuildStamp()
{
	return "nipkow " NIPKOW_VERSION ", built " __DATE__ " " __TIME__;
}
