/**
 * Nipkow — browser demo.
 *
 * Baird's 30-line mechanical television looking at the clip (SW Nipkow, NK01,
 * an effect). A mirror drum explores the clip in 30 strips, 12½ pictures a
 * second; the signal goes through a narrow amplifier into a neon lamp that
 * strikes after its formative lag and glows over only part of its plate when
 * starved; a disc with 30 holes on a spiral turns in front of the lamp, each
 * hole tracing one arc of the picture onto a screen that holds its light for
 * Persistence; and a phonic wheel whose coils carry the lamp's current holds
 * the disc in step, and hunts. Nothing in the picture is drawn.
 *
 * ---------------------------------------------------------------------------
 * What runs here, and what does not
 * ---------------------------------------------------------------------------
 *
 * **This page does not port the plugin. It runs it.** `nipkow-core.wasm` is
 * the plugin's own C++ compiled UNMODIFIED with emscripten
 * (demo/tools/build-wasm.sh): `source/Nipkow.cpp` — the plugin class, with
 * its constructor, its clock and seconds-or-milliseconds vote, the four
 * passes and every uniform it sets — and everything it calls (Controls,
 * Apparatus, Shaders, PassBuffer, Diag), with the FFGL SDK's CFFGLPlugin, its
 * parameter bookkeeping, FFGLShader and FFGLFBO. The transmitter's table is
 * read back from the GPU every frame and the whole chain — the amplifier, the
 * lamp, the disc and the wheel — is that C++, in double, on this page's
 * thread. Its GL calls go to this page's WebGL2 context through emscripten's
 * GL library.
 *
 * What is the page's and not the plugin's:
 *
 *   - **The host.** demo/wasm/glue.cpp constructs the plugin (what
 *     EffectPlugin.cpp's registration does in a bundle), reads its parameter
 *     declarations back through the SDK's host getters — the panel below is
 *     built from them, not from a list typed here — and forwards
 *     SetFloatParameter, SetTime and ProcessOpenGL, as tools/nktest's Rig does.
 *   - **Six GL entry points** (demo/wasm/gl_shim.cpp, which says why for each):
 *     glShaderSource, for the check and the port below; glEnable, glDisable
 *     and glIsEnabled, for GL_PROGRAM_POINT_SIZE; glTexImage2D, for the SDK's
 *     R32F allocations, which WebGL2 takes only with GL_RED; glReadPixels, for
 *     the table's read-back where the browser will not read GL_RED floats.
 *   - **The port.** WebGL2 compiles GLSL ES 3.00. `shaderSource()` below
 *     refuses any text that is not byte for byte one of this page's assemblies
 *     of its copy (shaders.js, held to source/Shaders.cpp by
 *     demo/tools/check_shaders.py), applies the kit's `port()` — the version
 *     line, the ES precision defaults and the removal of `precise` — and then
 *     checks, on port()'s actual output, that those are the only changes and
 *     that `precise` came off exactly the variables shaders.js's PRECISE lists.
 *     A stage that fails either check is refused, never compiled.
 *   - **The panel's read-outs.** The number beside a slider is Controls.cpp's
 *     conversion, compiled in (glue.cpp's nk_convert); its unit and the
 *     tooltips are this page's words.
 *   - **The photosensitivity warning, the question a link asks and the line
 *     under the picture.** See "Flash safety" below.
 *
 * ---------------------------------------------------------------------------
 * Flash safety
 * ---------------------------------------------------------------------------
 *
 * The plugin flickers at its own defaults, as a Baird televisor did, and is
 * meant to (Allan, 2026-11-18: never change a model or a default for flash
 * safety; warn instead). Measured by its harness (README, Flashing): at
 * Persistence 0.1 s every lit point swings by about half its light 12 1/2
 * times a second, past WCAG's red-flash threshold over 70% of the worst
 * ten-degree window on white, 64% on Resolume's brightest demo clip, 33% on a
 * flat 25% grey; Frame Fill, a higher Lamp, a Picture Rate near 5 or 8,
 * Persistence 0.05 s or less and Sync Free make it worse; 0.2 s keeps the
 * brightest clip under, 0.6 s white, and at 1 s only Picture Rate 4 to 7
 * crosses. So:
 *
 *   - the warning is the first thing on the page, above the picture and the
 *     controls, and says that (index.html);
 *   - nothing is mounted, so nothing is drawn, until the visitor presses
 *     Start under it (xenon's gate), whatever the way in: a plain visit, a
 *     link carrying settings (Copy link writes one; the gate says what it
 *     sets, and offers the defaults instead), and `?embed=1` too. A click in
 *     an OBS source is a nuisance; an embed that starts flickering on its own
 *     is worse;
 *   - every control keeps the plugin's own range and the page starts at the
 *     plugin's own defaults. Nothing is clamped: the flicker is the plugin's,
 *     and the line under the picture says when a setting is in the range
 *     that flickers (at the defaults, it is);
 *   - the default clip is the kit's synthetic scene, mostly dusk; no clip on
 *     the menu flashes by itself, and the brightest bars are left off.
 */

import { mountDemo } from './vendor/demo.js';
import { port, GLError } from './vendor/gl.js';
import { Params } from './vendor/params.js';
import * as S from './shaders.js';
import createNipkow from './nipkow-core.js';

//===========================================================================
// FFGL.h's parameter types, by number: what GetParamType returns.
//===========================================================================

const FF_TYPE_BOOLEAN = 0;
const FF_TYPE_EVENT = 1;
const FF_TYPE_STANDARD = 10;
const FF_TYPE_OPTION = 11;
const FF_TYPE_INTEGER = 13;
const FF_TYPE_TEXT = 100;

//===========================================================================
// The GLSL: the page's checked copy, assembled as the plugin assembles it
// (ASSEMBLY is read out of AssembleVertex(), Assemble() and InitGL by
// check_shaders.py), one text per stage of each of the four passes.
//===========================================================================

const PAGE_SHADERS = Object.entries(S.ASSEMBLY).map(([name, pieces]) => ({
  name,
  stage: name.endsWith('_vertex') ? 'vertex' : 'fragment',
  text: pieces.map((p) => S[p]).join(''),
}));

/** Which stages the compiled plugin handed over, and what became of each. */
const shaderCheck = { matched: new Set(), accepted: 0, refused: 0, portRefused: [], precise: {} };

const VERSION_LINE = '#version 410 core\n';

/**
 * port()'s output for one of the page's stages, checked line by line against
 * the plugin's text: after `#version 300 es` and the precisions (exactly what
 * port() puts in front of an empty text), every line must be the plugin's,
 * but for the blank line(s) port()'s version pattern takes with the version
 * line and the word `precise ` off exactly the declarations S.PRECISE lists
 * for that stage, in order. Null if anything else differs.
 * check_shaders.py's check_port() is the same check, at verify time.
 */
function portChecked(ours) {
  const ported = port(ours.text, { stage: ours.stage });
  const head = port('', { stage: ours.stage });
  if (!ours.text.startsWith(VERSION_LINE) || !ported.startsWith(head)) return null;
  const before = ours.text.slice(VERSION_LINE.length).split('\n');
  while (before.length > 1 && before[0].trim() === '') before.shift();
  const after = ported.slice(head.length).split('\n');
  if (before.length !== after.length) return null;
  const stripped = [];
  for (let i = 0; i < before.length; i += 1) {
    if (before[i] === after[i]) continue;
    const m = /^(\s*)precise (float (\w+) = .*)$/.exec(before[i]);
    if (m && after[i] === m[1] + m[2]) {
      stripped.push(m[3]);
      continue;
    }
    return null;
  }
  const want = S.PRECISE[ours.name] ?? [];
  if (stripped.join(',') !== want.join(',')) return null;
  return { ported, stripped };
}

/**
 * Called by gl_shim.cpp's glShaderSource with the text the compiled plugin
 * handed to GL. Anything but one of this page's own assemblies, or a port
 * that changed more than it may, is refused (an empty source, so the
 * plugin's InitGL fails and says which pass) rather than compiled.
 */
function shaderSource(stage, text) {
  const ours = PAGE_SHADERS.find((s) => s.stage === stage && s.text === text);
  if (!ours) {
    shaderCheck.refused += 1;
    return '';
  }
  const checked = portChecked(ours);
  if (!checked) {
    shaderCheck.portRefused.push(ours.name);
    return '';
  }
  shaderCheck.matched.add(ours.name);
  shaderCheck.accepted += 1;
  if (checked.stripped.length) shaderCheck.precise[ours.name] = checked.stripped;
  return checked.ported;
}

//===========================================================================
// The module.
//===========================================================================

const printed = [];
let core;
try {
  core = await createNipkow({
    nipkowShaderSource: shaderSource,
    print: (text) => { printed.push(text); console.log(text); },
    printErr: (text) => { printed.push(text); console.warn(text); },
  });
} catch (error) {
  const root = document.querySelector('#demo');
  root.textContent = `The demo could not start: this browser would not run its WebAssembly (${error.message}). The plugin, its source and its downloads are at https://github.com/stoatworks-labs/nipkow.`;
  throw error;
}

const text = (pointer) => core.UTF8ToString(pointer);

//===========================================================================
// The declarations, read from the plugin's constructor through the SDK.
//===========================================================================

function declarations() {
  const h = core._nk_new();
  const list = [];
  const n = core._nk_param_count(h);
  for (let i = 0; i < n; i += 1) {
    const type = core._nk_param_type(h, i);
    const d = { index: i, name: text(core._nk_param_name(h, i)), type, group: text(core._nk_param_group(h, i)) };
    d.default = type === FF_TYPE_TEXT ? null : core._nk_param_default(h, i);
    if (type === FF_TYPE_OPTION) {
      d.elements = [];
      d.values = [];
      for (let e = 0; e < core._nk_param_element_count(h, i); e += 1) {
        d.elements.push(text(core._nk_param_element_name(h, i, e)));
        d.values.push(core._nk_param_element_value(h, i, e));
      }
    }
    if (type === FF_TYPE_INTEGER) {
      d.min = core._nk_param_range_min(h, i);
      d.max = core._nk_param_range_max(h, i);
    }
    list.push(d);
  }
  const maxInputs = core._nk_max_inputs(h);
  core._nk_delete(h);
  return { list, maxInputs };
}

const DECLARED = declarations();

//===========================================================================
// The read-outs: Controls.cpp's number, this page's unit.
//===========================================================================

const signed = (x, digits, unit) => `${x > 0 ? '+' : x < 0 ? '−' : '±'}${Math.abs(x).toFixed(digits)}${unit}`;
const UNITS = {
  'Picture Rate': (x) => `${x.toFixed(x < 10 ? 2 : 1)} a second`,
  Bandwidth: (x) => `${(x / 1000).toFixed(x < 10000 ? 2 : 1)} kHz`,
  Lamp: (x) => `${x.toFixed(1)} mA`,
  Speed: (x) => signed(x * 100, 2, '%'),
  Framing: (x) => signed(x, 3, ' line'),
  Persistence: (x) => (x < 0.1 ? `${(x * 1000).toFixed(0)} ms` : `${x.toFixed(2)} s`),
  Mix: (x) => x.toFixed(2),
};

/** Controls.cpp's conversion of a host value, or NaN where there is none. */
const convert = (d, v) => core._nk_convert(d.index, Math.fround(v));

function readout(d) {
  return (v) => {
    const x = convert(d, v);
    if (Number.isNaN(x)) return v.toFixed(2);
    return (UNITS[d.name] ?? ((y) => y.toFixed(3)))(x);
  };
}

/** Tooltips: this page's words, from the README's description of each control. */
const HINTS = {
  Lines: 'How many lines the picture is scanned in, 8 to 120 (30, Baird’s standard); the disc has as many holes. FF_TYPE_INTEGER in the plugin, a number field here, held to its declared 8–120.',
  'Picture Rate': 'Pictures a second, 2 to 30, geometrically (12½, the standard). Every point is lit once a picture. A rate of 4 to 7 flickers past WCAG’s red-flash threshold even at Persistence 1 s, and near 5 or 8 the flicker is harder: see the warning at the top.',
  Scanner: 'The transmitter. Drum: the BBC’s mirror drum, straight lines that the disc draws as arcs. Disc: a disc like the receiver’s, the same arcs at both ends, so the geometry cancels.',
  Bandwidth: 'The amplifier, one pole, 0.5 to 40 kHz, geometrically (9 kHz, the BBC’s channel). Narrower smears the picture’s edges along the lines.',
  Lamp: 'The neon’s standing current, 0 to 30 mA (10.1 mA, set from Resolume’s demo clips); the signal swings it ±10 mA. Lower, black puts the lamp out and it strikes again a formative lag later; a starved lamp glows over only part of its plate. Higher, the flicker is harder.',
  Sync: 'Locked: an ideal synchroniser. Hunting: the phonic wheel, whose coils carry the lamp’s current, holds the disc in step; a dark picture holds it weakly, so it swings and slips. Free: its coils disconnected, so the disc runs at the motor’s own speed (and on white the flicker is a little harder).',
  Speed: 'The motor’s own speed error, ±5% (+0.5%). Hunting, the wheel pulls it in as far as its pull-out (on the line under the picture); Free, the picture drifts.',
  Framing: 'Where the wheel locks, ±½ line.',
  Persistence: 'How long the screen holds its light, 20 ms to 1 s, geometrically (0.1 s, about one picture). At the default every lit point swings by about half its light once a picture, so a bright picture flickers in saturated red 12½ times a second, past WCAG’s red-flash threshold. Turned up it calms: 0.2 s or more keeps the brightest demo clip under the line, 0.6 s or more keeps white under. 0.05 s or less is worse. See the warning at the top.',
  Frame: 'Baird: the 7:3 field, the whole sector fitted to the frame’s height. Fill: the field stretched to the frame, which no televisor did, so the flicker covers the whole frame.',
  Mix: 'The televisor’s picture against the clip, in code values. 1: the picture alone, black round it, opaque. 0: the clip, untouched.',
};

//===========================================================================
// Kit parameters from the declarations. The id is the name as Arena
// addresses it (lower case, spaces removed).
//===========================================================================

const KIT_TYPES = {
  [FF_TYPE_STANDARD]: 'standard',
  [FF_TYPE_OPTION]: 'option',
  [FF_TYPE_BOOLEAN]: 'boolean',
  // A number field (below): the kit has no integer control.
  [FF_TYPE_INTEGER]: 'text',
};

/** The kit reads these from the query string itself. */
const RESERVED_QUERY = new Set(['embed', 'size', 'clip', 'bg']);
const addressOf = (name) => {
  const address = name.toLowerCase().replace(/\s+/g, '');
  return RESERVED_QUERY.has(address) ? `param-${address}` : address;
};

// The About block (a text line and its buttons) is the plugin's, declared
// last, and left off the panel as on every page in the suite.
const onPanel = (d) => d.group !== 'About' && KIT_TYPES[d.type] !== undefined;

function kitDefault(d) {
  switch (d.type) {
    case FF_TYPE_OPTION: {
      const i = d.values.findIndex((v) => v === d.default);
      return i >= 0 ? i : Math.round(d.default);
    }
    case FF_TYPE_BOOLEAN: return d.default > 0.5 ? 1 : 0;
    case FF_TYPE_INTEGER: return String(Math.round(d.default));
    default: return d.default;
  }
}

const PARAMS = DECLARED.list.filter(onPanel).map((d) => {
  const p = { id: addressOf(d.name), name: d.name, type: KIT_TYPES[d.type], group: d.group, hint: HINTS[d.name], ff: d };
  if (d.type === FF_TYPE_OPTION) p.elements = d.elements;
  if (d.type === FF_TYPE_STANDARD) p.display = readout(d);
  p.default = kitDefault(d);
  return p;
});

const byName = (name) => PARAMS.find((p) => p.name === name);

/** The value the host would send for a kit control, or null for none. */
function hostValue(d, v) {
  switch (d.type) {
    case FF_TYPE_OPTION: return Math.fround(d.values[Math.round(v)] ?? 0);
    case FF_TYPE_BOOLEAN: return v > 0.5 ? 1 : 0;
    case FF_TYPE_EVENT: return null;
    case FF_TYPE_INTEGER: {
      const n = Number.parseInt(String(v).trim(), 10);
      if (!Number.isFinite(n)) return null;
      return Math.min(d.max, Math.max(d.min, n));
    }
    default: return Math.fround(v);
  }
}

//===========================================================================
// The flickering range, as the plugin's README measures it (Flashing), for
// the line under the picture and the Start gate. The page's reading of a
// table measured one setting at a time, not a measurement made here.
//===========================================================================

/** The physical value of a control at kit value v (Controls.cpp's conversion). */
function physical(name, v) {
  const p = byName(name);
  return p ? convert(p.ff, v) : NaN;
}

/**
 * What the README's Flashing table says of these settings, as sentences: an
 * empty list only where it measured nothing crossing. `get(id)` is the kit
 * value of a control. At the plugin's defaults the list is not empty.
 */
function flashNotes(get) {
  const notes = [];
  const persistence = physical('Persistence', get('persistence'));
  const rate = physical('Picture Rate', get('picturerate'));
  const lamp = physical('Lamp', get('lamp'));
  const option = (name) => {
    const p = byName(name);
    return p ? p.elements[Math.round(get(p.id))] : '';
  };
  const P = UNITS.Persistence(persistence);
  const flickering = persistence < 0.6;
  if (persistence <= 0.05 + 1e-9) {
    notes.push(`Persistence ${P}: at 0.05 s or less the flicker is at its hardest (on white it crosses the luminance threshold too)`);
  } else if (persistence < 0.2) {
    notes.push(`Persistence ${P}: under 0.2 s a bright picture flickers past WCAG’s red-flash threshold (the default 0.1 s: 70% of the worst window on white, 64% on Resolume’s brightest demo clip, 33% on a 25% grey; WCAG fails 25%)`);
  } else if (flickering) {
    notes.push(`Persistence ${P}: under 0.6 s white still flickers past the threshold (the brightest demo clip is under from 0.2 s)`);
  }
  if (rate >= 4 - 1e-9 && rate < 8) {
    notes.push(`Picture Rate ${UNITS['Picture Rate'](rate)}: 4 to 7 a second flickers past the threshold even at Persistence 1 s (47–70% on white)${flickering && rate >= 4.5 && rate < 6 ? '; near 5 it is harder still (on white the luminance flashes too)' : ''}`);
  } else if (flickering && rate >= 8 && rate < 9) {
    notes.push(`Picture Rate ${UNITS['Picture Rate'](rate)}: near 8 the flicker is harder (on white the luminance flashes too)`);
  }
  if (flickering && lamp > 12) {
    notes.push(`Lamp ${UNITS.Lamp(lamp)}: a higher Lamp flickers harder`);
  } else if (!flickering && lamp > 20) {
    notes.push(`Lamp ${UNITS.Lamp(lamp)}: at Persistence 1 s, 30 mA on white read 15% of the window (WCAG fails 25%)`);
  }
  if (notes.length && option('Sync') === 'Free') notes.push('Sync Free: a little harder on white');
  if (notes.length && option('Frame') === 'Fill') notes.push('Frame Fill: the flicker covers the whole frame (100% of the window on white and the brightest clip)');
  return notes;
}

//===========================================================================
// The renderer: one plugin instance, driven as a host drives it.
//===========================================================================

let renderer = null;
let mounted = null;

function createRenderer(gl) {
  // The transmitter's table and the screen's three exposure buffers are R32F
  // render targets, and the table is read back as floats every frame. WebGL2
  // renders into a float texture only with this extension; the kit is not
  // asked to enable it (needFloat) so that the refusal can say why here.
  if (!gl.getExtension('EXT_color_buffer_float')) {
    throw new GLError('This browser cannot render to a 32-bit float texture (EXT_color_buffer_float is missing). The plugin’s transmitter table and its screen’s exposure are R32F render targets, and the table is read back as floats every frame to drive the lamp and the wheel; an 8-bit stand-in would quantise the signal the whole chain runs on and give a plausible wrong picture, so the demo stops here. Desktop Chrome, Edge, Firefox and Safari have it.');
  }
  // emscripten's GL library, pointed at the kit's context. Nothing is turned
  // on behind the kit's back: no extensions are enabled on its behalf.
  const handle = core.GL.registerContext(gl, { majorVersion: 2, minorVersion: 0, enableExtensionsByDefault: false });
  core.GL.makeContextCurrent(handle);
  core._nk_prepare_log();

  let instance = 0;
  let pushed = new Map();
  let clipName = 0;
  let clipTexture = null;
  let frames = 0;

  /** A kit texture under a GL name the plugin can bind, in emscripten's tables. */
  function glName(texture) {
    if (texture === clipTexture) return clipName;
    if (clipName) core.GL.textures[clipName] = null;
    clipName = core.GL.getNewId(core.GL.textures);
    core.GL.textures[clipName] = texture;
    texture.name = clipName;
    clipTexture = texture;
    return clipName;
  }

  function start(width, height) {
    instance = core._nk_new();
    pushed = new Map();
    frames = 0;
    if (!core._nk_init_gl(instance, width, height)) {
      let why;
      if (shaderCheck.refused > 0) why = 'the GLSL the compiled plugin handed to GL is not this page’s checked copy of source/Shaders.cpp, so it was not compiled';
      else if (shaderCheck.portRefused.length) why = `the kit’s port() changed ${shaderCheck.portRefused.join(', ')} beyond the version line, the precisions and the \`precise\` qualifiers shaders.js lists, so it was not compiled`;
      else why = `the plugin’s InitGL failed${printed.length ? `: ${printed.slice(-3).join(' ')}` : ''}`;
      throw new GLError(`SW Nipkow could not start: ${why}.`);
    }
  }

  function push(params) {
    for (const p of PARAMS) {
      const value = hostValue(p.ff, params.get(p.id));
      if (value === null || pushed.get(p.id) === value) continue;
      pushed.set(p.id, value);
      core._nk_set_float(instance, p.ff.index, value);
    }
  }

  return {
    render({ input, params, width, height, time }) {
      core.GL.makeContextCurrent(handle);
      if (!instance) start(width, height);
      push(params);
      const clip = glName(input.texture);
      if (!core._nk_process(instance, time, clip, input.width, input.height)) {
        throw new GLError('The plugin’s ProcessOpenGL failed.');
      }
      // For anything checking the page from outside.
      frames += 1;
      const canvas = gl.canvas;
      canvas.dataset.frames = String(frames);
      canvas.dataset.readback = core.nipkowReadback ?? '';
      canvas.dataset.samples = String(core._nk_samples(instance));
      canvas.dataset.lit = String(core._nk_lit_share(instance));
      canvas.dataset.psi = String(core._nk_psi(instance));
      canvas.dataset.clock = String(core._nk_clock_scale(instance));
      canvas.dataset.shaders = String(shaderCheck.accepted);
      canvas.dataset.precise = Object.values(shaderCheck.precise).flat().join(' ');
    },
    stats() {
      if (!instance) return null;
      return {
        samples: core._nk_samples(instance),
        lit: core._nk_lit_share(instance),
        current: core._nk_current(instance),
        pull: core._nk_pull(instance),
        pullOut: core._nk_pull_out(instance),
        psi: core._nk_psi(instance),
        psiDot: core._nk_psi_dot(instance),
        strikes: core._nk_strikes(instance),
        lampLit: core._nk_lit(instance) === 1,
        interval: core._nk_interval(instance),
        clock: core._nk_clock_scale(instance),
        readback: core.nipkowReadback ?? null,
      };
    },
    log() {
      try {
        return core.FS.readFile(text(core._nk_log_path()), { encoding: 'utf8' });
      } catch {
        return '';
      }
    },
    shaders: shaderCheck,
  };
}

//===========================================================================
// What this page does not reproduce. The kit adds its four.
//===========================================================================

const DIFFERENCES = [
  'This page runs the plugin rather than a port of it. nipkow-core.wasm is source/Nipkow.cpp — the plugin class: its constructor, its clock and seconds-or-milliseconds vote, the four passes and every uniform it sets — and everything it calls (Controls, Apparatus, Shaders, PassBuffer, Diag), with the FFGL SDK’s CFFGLPlugin, parameter bookkeeping, FFGLShader and FFGLFBO, all compiled unmodified by emscripten (demo/tools/build-wasm.sh). The transmitter’s table is read back from the GPU every frame, and the amplifier, the lamp, the disc and the phonic wheel are that C++, in double. Only the registration file is left out; the page constructs the plugin itself, as the plugin’s harness does. The panel is read back from the plugin’s own declarations through the SDK’s host getters, and the numbers beside the sliders are Controls.cpp’s conversions, compiled in; their units and every tooltip are this page’s.',
  'The GLSL is the plugin’s: demo/shaders.js is spliced from source/Shaders.cpp, with the order AssembleVertex() and Assemble() join the pieces in, and the repository’s verify script fails if a character differs. At start-up the page also requires each of the eight stages the compiled plugin hands to glShaderSource to equal its copy, and refuses to compile anything else. WebGL2 compiles GLSL ES 3.00, not the plugin’s GLSL 4.10, so the kit’s port() then makes three changes, and the page checks on its actual output that they are the only ones (as the verify script does): the version line becomes #version 300 es, the ES precision defaults (highp) go in front, and the word precise comes off the seven variables the exposure pass declares with it.',
  'Dropping precise is the one change that can move a number, and nothing here measures whether it does. ES 3.00 has no such word. In the plugin it forbids the compiler to reassociate or fuse two calculations in the exposure pass (AGENTS.md, traps 2 and 3): a hole’s radial overlap taken relative to the hole (rel, rlo, rhi), because at 350 holes out a float resolves 3e-5 and r + hr less r − hr would otherwise round the same way at every pixel; and each sample’s place along its line carried in two floats (da, db, ua, ub), because Apple’s compiler turned (u − start) + rest into (u + rest) − start and lost the remainder. A browser whose compiler does either may round the overlap to a pattern of about 3e-5 of a hole, under 1e-3 of a pixel’s light at 1920 × 1080 and below, and place a sample up to 2e-6 of a line (5e-4 of a sample) off: by that arithmetic, under one 8-bit step of the picture, but not checked. On a Mac the browser’s shader ends up with Apple’s compiler, the one that dropped the remainder before precise was added.',
  'GL is emscripten’s WebGL2 library on this page’s context, not a GL 4.1 driver, and six entry points are the page’s (demo/wasm/gl_shim.cpp). glShaderSource, for the check and the port. glEnable, glDisable and glIsEnabled, because GL_PROGRAM_POINT_SIZE — state the plugin saves and restores each frame, as a plugin in Resolume must — does not exist in WebGL2; it reads as off here, and the plugin draws no points. glTexImage2D, because the SDK allocates its R32F buffers with no data and GL_RGBA as the client format, which WebGL2 refuses (it takes GL_RED for R32F); with no data nothing about the allocation changes. glReadPixels, because the plugin reads the table back as GL_RED floats, which a browser may not offer; where it does not, the same rectangle is read as RGBA floats and the red channel copied, the same values in another layout. The line under the picture says which this browser did. All of it needs EXT_color_buffer_float (32-bit float render targets); a browser without it is refused rather than handed 8 bits.',
  'The clock is the page’s: seconds from frame deltas, capped at a tenth of a second a frame, handed to SetTime. The plugin’s own vote against the wall clock settles the unit (seconds, here within four frames); until it does the plugin runs on the wall clock, and the frame it settles on starts its intervals afresh — the plugin’s own rules. The plugin also caps a frame at 0.1 s. Restart is a backward jump and Pause a clock that stands still; in both the plugin runs the last real frame’s length, as it does in a host whose clock stops, so the disc keeps turning frame by frame: paused, a moved control draws one frame and turns the disc by one frame, and dragging a slider while paused runs the picture as fast as the drag. Step advances the clock 1/60 s.',
  'A frame here is one of the display’s refreshes (60 or 120 a second, and whenever the browser gets round to it), not Resolume’s output frame. The chain runs over each frame’s own interval in samples, so the picture rate, the lamp and the wheel do not depend on it, but which instants are drawn does, and so does the flicker a camera or a screen recording sees.',
  'WebAssembly is not the Mac’s arm64. It has no fused multiply-add, and its exp, sin and pow are musl’s, not Apple’s, so the chain’s doubles can differ from the plugin built for a Mac in the last digits. What reaches the GPU is rounded to float, as in the plugin, and then rasterised by this browser’s GPU, not the Mac’s. This page has not been compared with the plugin’s harness frame for frame.',
  'The clip is the kit’s generated one, or your own image or video. The kit hands over premultiplied colour, and the plugin treats its input as straight colour times alpha, so over a half-transparent edge the transmitter would see it darker than it is. The page’s own clips are all opaque, which is why the kit’s shape-on-transparency clip is not among them; an image of your own with transparency would show it at its edges. The brightest colour bars are left off too: the brighter the picture, the more of it flickers.',
  'Lines is FF_TYPE_INTEGER in the plugin and a number field here, held to its declared 8–120. The About block (a credit line and its buttons) is declared by the plugin and left off this panel.',
  'The photosensitivity warning at the top, the Start button nothing is drawn before, and the caution on the line under the picture are the page’s, not the plugin’s; the flicker they warn of is the plugin’s, at its own defaults, and the page keeps it. The warning and the caution read the plugin README’s Flashing table, measured per pixel by its harness on white, a grey, its card and Resolume’s demo clips, one setting at a time; they are not a measurement of this picture or of the kit’s clips, and combinations of settings were not measured.',
  'Nothing here is measured. The plugin itself has not been loaded into Resolume on any platform; oxbow, the fleet’s test host, reads its bundle and renders through it (its README’s Status). Its harness, nktest, drives the same class headlessly on a synthetic clock and reads the picture back off the pixels: the holes’ spiral to 0.008 px, the dwell’s 1/r to 2e-7, the wheel’s drift to 1e-6 of a line and its hunting to 1.2e-4, with 24 wrong models and 51 one-character mutants caught. That harness, not this page, is the reason to believe the televisor.',
];

//===========================================================================
// The page.
//===========================================================================

const query = new URLSearchParams(window.location.search);
const embed = query.has('embed') && query.get('embed') !== '0';

function start() {
  mounted = mountDemo({
    name: 'Nipkow',
    pluginId: 'NK01',
    kind: 'effect',
    tagline:
      'Baird’s 30-line mechanical television looking at your clip. A mirror drum explores it in 30 strips, 12½ pictures a second; the signal goes through a narrow amplifier into a neon lamp that strikes late and glows over only part of its plate when starved; a disc with 30 holes on a spiral turns in front of the lamp, each hole drawing one arc of the picture; and a phonic wheel whose coils carry the lamp’s current holds the disc in step, and hunts. Nothing in the picture is drawn.',
    repo: 'https://github.com/stoatworks-labs/nipkow',

    blurb:
      'It is the plugin’s own C++ — the plugin class, the lamp and the wheel in double, and its clock — compiled unmodified to WebAssembly, drawing with its own GLSL in WebGL2 on generated clips or your own. The host is this page.',

    // The plugin's float targets are asked for in createRenderer, which says
    // why when a browser has none.
    needFloat: false,
    // At Mix 1, the default, the receiver paints the whole frame: alpha 1.
    showBackdrop: false,

    // Calm first: the kit's dusk scene. All opaque, and not the 100% bars
    // (see the notes at the foot).
    sources: ['scene', 'grid', 'bars', 'ramp', 'spot', 'detail'],

    params: PARAMS,
    differences: DIFFERENCES,

    createRenderer: (gl) => {
      renderer = createRenderer(gl);
      return renderer;
    },
  });

  if (mounted && !embed) {
    buildLinesField();
    buildStatusLine();
  }
}

//---------------------------------------------------------------------------
// Lines: FF_TYPE_INTEGER, a number field in the declared range.
//---------------------------------------------------------------------------
function buildLinesField() {
  for (const p of PARAMS) {
    if (p.ff.type !== FF_TYPE_INTEGER) continue;
    const field = document.getElementById(`p-${p.id}`);
    if (!field) continue;
    field.type = 'number';
    field.min = String(p.ff.min);
    field.max = String(p.ff.max);
    field.step = '1';
    field.inputMode = 'numeric';
    field.title = p.hint ?? '';
  }
}

//---------------------------------------------------------------------------
// The line under the picture: the plugin's own state, through the accessors
// its harness reads, and the caution. It reports; it measures nothing.
//---------------------------------------------------------------------------
function buildStatusLine() {
  const stage = document.querySelector('.stage');
  if (!stage) return;
  const caution = document.createElement('p');
  caution.className = 'stage__status nk-caution';
  caution.id = 'nk-caution';
  caution.setAttribute('role', 'status');
  caution.hidden = true;
  const line = document.createElement('p');
  line.className = 'stage__status nk-status';
  line.id = 'nk-status';
  line.setAttribute('aria-live', 'off');
  const log = document.createElement('p');
  log.className = 'stage__status';
  log.id = 'nk-log';
  log.setAttribute('aria-live', 'off');
  log.style.fontFamily = 'ui-monospace, SFMono-Regular, Menlo, monospace';
  log.style.fontSize = '12px';
  log.style.whiteSpace = 'pre-wrap';
  stage.append(caution, line, log);

  const params = mounted.params;
  const sync = byName('Sync');

  // The caution follows the controls at once, not on the next tick.
  const updateCaution = () => {
    const notes = flashNotes((id) => params.get(id));
    caution.hidden = notes.length === 0;
    caution.textContent = notes.length
      ? `Flicker: ${notes.join('; ')}. These are the plugin README’s measurements, one setting at a time (Flashing); combinations were not measured, and this page does not measure its own picture. Persistence turned up calms it; Pause stops the picture.`
      : '';
  };
  params.addEventListener('change', updateCaution);
  updateCaution();

  let lastLog = '';
  setInterval(() => {
    const s = renderer?.stats();
    if (!s) return;
    const unit = s.clock === 1 ? 'seconds' : s.clock === 0.001 ? 'milliseconds' : 'not settled yet';
    const mode = sync ? sync.elements[Math.round(params.get(sync.id))] : '';
    // ψ is the disc's angle less the transmitter's, in revolutions: a whole
    // revolution is a whole picture slipped, and frames the same.
    const whole = Math.round(s.psi);
    const off = s.psi - whole;
    const lineCount = hostValue(byName('Lines').ff, params.get('lines')) ?? 30;
    const disc = `disc ${off >= 0 ? '+' : '−'}${Math.abs(off * lineCount).toFixed(2)} lines off the transmitter${whole ? `, ${Math.abs(whole)} whole picture${Math.abs(whole) === 1 ? '' : 's'} slipped` : ''}, moving ${s.psiDot >= 0 ? '+' : '−'}${Math.abs(s.psiDot * lineCount).toFixed(2)} lines/s`;
    const wheel = mode === 'Hunting'
      ? ` · the wheel: pull ${s.pull.toFixed(1)} mA², holding a speed error up to ${(100 * s.pullOut).toFixed(2)}%; ${disc}`
      : mode === 'Free'
        ? ` · the wheel’s coils disconnected: ${disc}`
        : ' · Sync Locked: an ideal synchroniser';
    line.textContent = `Lamp ${s.lampLit ? 'lit' : 'out'}, ${s.current.toFixed(2)} mA; lit over ${(100 * s.lit).toFixed(1)}% of the picture’s samples; ${s.strikes} strikes`
      + wheel
      + ` · ${s.samples} samples in the last frame, over ${(1000 * s.interval).toFixed(1)} ms`
      + ` · host clock read in ${unit}`
      + ` · table read back as ${s.readback ?? '…'}`;
    const textLog = renderer.log();
    if (textLog === lastLog) return;
    lastLog = textLog;
    const lines = textLog.trim().split('\n').filter(Boolean).map((l) => l.replace(/^\S+ (INFO |WARN |ERROR) nipkow: /, '$1 '));
    log.textContent = lines.length ? `The plugin’s own log (Diag.cpp, in this page’s memory):\n${lines.slice(-2).join('\n')}` : '';
  }, 250);
}

//---------------------------------------------------------------------------
// Start: nothing is mounted, so nothing is drawn, until the visitor has
// pressed Start under the warning. Every way in, embeds included. A link
// that carries settings away from the defaults says what it sets, and can be
// started at the defaults instead.
//---------------------------------------------------------------------------
function linkSettings() {
  const probe = new Params(PARAMS);
  probe.fromQuery(query);
  const away = [...probe.toQuery().keys()];
  return { probe, away };
}

function describeSetting(probe, id) {
  const p = PARAMS.find((x) => x.id === id);
  const v = probe.get(id);
  if (p.type === 'option') return `${p.name} ${p.elements[Math.round(v)]}`;
  if (p.type === 'text') return `${p.name} ${hostValue(p.ff, v) ?? v}`;
  return `${p.name} ${p.display ? p.display(v) : v}`;
}

function gate() {
  const section = document.getElementById('photosensitivity');
  const words = document.getElementById('nk-gate-text');
  const startButton = document.getElementById('nk-start');
  const defaultsButton = document.getElementById('nk-start-defaults');
  const note = document.getElementById('nk-start-note');
  // Without the warning's own button there is no Start, and nothing starts.
  if (!section || !startButton) return;

  const { probe, away } = linkSettings();
  if (away.length && words && defaultsButton) {
    const notes = flashNotes((id) => probe.get(id));
    words.textContent = `This link sets ${away.map((id) => describeSetting(probe, id)).join(', ')}.`
      + (notes.length ? ` With them: ${notes.join('; ')}.` : ' With them nothing the README measured crosses, but combinations were not measured.');
    words.hidden = false;
    startButton.textContent = 'I have read this. Start with the link’s settings';
    defaultsButton.hidden = false;
  }
  startButton.hidden = false;

  const go = (keep) => {
    startButton.disabled = true;
    if (defaultsButton) defaultsButton.disabled = true;
    if (!keep) {
      for (const id of away) query.delete(id);
      const url = new URL(window.location.href);
      url.search = query.toString();
      window.history.replaceState(null, '', url);
    }
    start();
    startButton.remove();
    defaultsButton?.remove();
    if (words) words.hidden = true;
    section.classList.add('is-started');
    if (note) note.textContent = 'Started. Pause, under the picture, stops it at once; Persistence turned up calms it.';
  };
  startButton.addEventListener('click', () => go(true), { once: true });
  defaultsButton?.addEventListener('click', () => go(false), { once: true });
}

gate();

export { renderer, mounted, PARAMS, DECLARED, shaderCheck };
