# CHOMPI TAPE mini-guide checklist

Source: `CHOMPI_MiniGuide_TAPE_Chase+Bliss.pdf` (all 18 PDF pages, including
cover, contents, cheat sheet, and notes). Code reference: the archived TAPE
reference at commit `a73d732613da684e4de844619b690776f0f50ccf`.

These are implementation checks, **not proof of behavior parity**. Run the
automated CTest checks locally; hardware comparison remains separate.

Maintenance work completed:

- The native engine now calls `Prepare`/`Process` once per 24-frame callback at
  48 kHz. Buffering and conversion latency are reported to the host.
- MIDI encoder CCs run on the audio timeline. Closed-editor render tests compare
  44.1/48/96 kHz with large and irregular host buffers and require identical audio.
- `src/control/NormalPage.h` and `MenuPage.h` retain the original event handlers,
  with desktop clock, MIDI, preset and copy services. Original LED color/interpolation and menu/transport light logic now
  output a native RGB frame on the audio clock; board LED drivers and boot
  debounce are omitted. Audible parameter updates formerly in drawing run at
  native callback boundaries. Keyboard Shift supplies the original menu input.
- Capture writes into its preallocated sample buffer; completion publishes the
  same storage. Audio callback checks cover allocation-free control and capture.
- DAW state embeds recorded/imported PCM and tape. Autosaves include the completed
  prefix of an active sample recording; recall cancels older captures and pending
  events. Fresh-instance disk recall, playable audio at another host rate, and
  non-parameter host dirty notifications are covered by plugin checks.
- Endless encoders apply relative turns from the current sound's setting,
  including reverse tape speeds. There is no click-position jump.
- Runtime code is under `src/engine`, `src/control`, and `src/dsp/primitives`.
  Board libraries and unused firmware were removed. Unmodified sampler/looper
  and control reference files remain in the compressed test fixture; attribution
  and redistribution notices remain in `licenses`.

Remaining verification gap: differential tests cover the sampler and looper
with shared desktop storage services. Full-engine scheduling, routing, FX and
all control workflows have not been compared against an independent reference
or hardware. These changes do not establish complete hardware parity.

- [x] Seven-voice JAMMI chromatic and CUBBI one-shot playback; five separate
  banks of 14 slots in each mode; shared CHOMPI buffer and factory recordings.
- [x] CHOMPI switch up records the selected microphone, line, or resample input;
  switch down makes the pink key Shift. Hold and latch sampling are supported.
- [x] New captures replace the temporary buffer, return to JAMMI playback, and
  restore the firmware's default sample controls. Save/copy preserves buffer
  audio and its preset settings.
- [x] Endless encoder operation: speed/direction, volume/pan, start/end,
  attack/release, loop/sustain, magic-wand delay/reverb, drive, filter,
  time/warble/resonance, tape speed/scrub, master/input level, and compression.
  Encoder lights also change pages when clicked. Warble and time have no
  separate knobs.
- [x] An adapted copy of the firmware `DSPEngine.h` now runs the sample reader,
  FX/reverb, looper, input filters, gain stages, output compression, and
  master/headphone routing. The extra legacy master trim is bypassed.
- [x] Loop first recording, overdub, play/pause, rewind, scrub, speed reset,
  stepped speed, feedback adjustments, record cue, and erase. A dedicated arm button covers the record-cue chord; the other actions use
  the original transport and Shift gestures.
  The visible ARM TAPE control supplies the original record-cue chord, shows
  armed status, and supports cancellation. Plugin checks cover waiting through
  input audio, starting on the next note, and preserving an existing loop.
- [x] Shift menu selects mode/bank, microphone/line/resample source, FX
  pre/post position, and preset erase/copy/save with pink-key confirmation.
  Buffer and tape can participate in save/copy operations.
- [x] Firmware MIDI input range and channel for notes, transport CC, and
  encoder CC; outgoing on-screen key, transport, and encoder MIDI on the chosen
  channel. The CHOMPI/JAMMI switch and keyboard Shift expose the hardware gestures.
- [x] Audio import, full factory reset, separate optional Aux return and
  Headphones buses, and host/project state persistence.

Desktop and hardware differences still visible to the user:

- [ ] User-requested desktop volume differs from hardware: the regular volume
  control has a fixed +24 dB stage after host-rate conversion, followed by a
  separate stereo-linked output limiter per bus (-1 dBFS sample ceiling, immediate
  attack, 50 ms release). Startup volume is 0.5 for headroom; firmware startup is 0.84. The
  original firmware gain/compression remains inside the engine. There is no
  second gain knob; the old outputBoostDb session parameter is inactive.
- [ ] The plugin does not boot `firmware.bin`, mount a FAT32 SD card, or use the
  original `options.json`/`presets.json` as writable media. Its `.nibbi` project
  and DAW state store the equivalent desktop session data.
- [ ] Physical USB-C power, battery charge/level indication, 3.5 mm jacks,
  and direct USB/Type-A MIDI ports have no plugin equivalent. The host supplies
  audio and MIDI devices; some hosts do not expose the optional buses.
- [ ] Desktop file transactions are serviced off the audio thread; their completion
  time differs from SD media. The title bar, background selection and keyboard
  Shift are desktop controls. First-use Shift omits the board boot wait.
- [ ] Exact control parity remains unverified beyond the covered regression cases.

LED / record-cue maintenance (2026-09-30):

- Original NormalPage and MenuPage light calculations drive knob colors, VU,
  tape direction/position, recording, armed blink, and preset/key indications.
  Visual frames run every 768 native samples; publishing RGB does not call DSP
  from the GUI. Original LED-chain drivers and battery display are omitted.
- Desktop addition: Shift + yellow Record arms/cancels **empty** tape. Both
  mouse edges are consumed even if Shift is released first. Existing-tape Shift
  feedback and preset copy gestures retain their original behavior. Arming is
  note-triggered, as in the firmware, not an audio threshold trigger.
- Armed red blinking remains visible while Shift is held. Mode, input and FX
  selection lights remain visible outside Shift as previously requested.
- Black silkscreen on pink; lamp bounds now include the bezel/shadow. Native
  LED tests cover knob pages, Shift toggles, arm/cancel and blinking, plus the
  existing realtime/allocation and audio regression checks. Physical hardware
  brightness and complete independent LED-frame comparison remain unverified.

Product rename and light rendering (2026-09-30):

- Product folder, native classes/namespaces, build targets, release registration,
  app/plugin display names and new project extension use nibbi. Historical
  source names remain in attribution and immutable reference fixtures.
- Legacy project XML and device preferences have explicit read/migration paths;
  saved projects write the new name. Existing plugin type IDs are retained.
- The encoder has an 8-pixel molded bevel. Lamp rendering adds diffuse glow,
  a bright core, glass highlights and lens rims without changing firmware RGB
  values or blink timing. Round lamp bounds include 8 pixels for the glow.

## Website guide adaptation (2026-09-30)

`website/src/app/nibbi/page.tsx` replaces the introductory product copy with a
download area and a NIBBI user guide. The guide was rewritten against the local
18-page PDF and checked against the current editor, control handlers, processor,
and desktop README. Source-page coverage (printed page numbers):

- Overview, hardware and setup (1–3): plugin installation, DAW instrument tracks,
  host audio/MIDI routing and the optional standalone device setup.
- Getting started and controls (4–6): first sample/loop, encoder pages, desktop
  Shift gestures, toolbar commands and all ten raised-key menu functions.
- Sampler and sound design (7–9): hold/latch capture, shared buffer, chromatic and
  one-shot modes, five banks, speed, level/pan, trim, envelopes, loop and sustain.
- Effects, looper and routing (10–12): all three effect pages and Shift functions,
  pre/post placement, tape recording/overdub/arming/scrub/feedback, three monitor
  routes and the optional host buses.
- SD card/firmware (13): replaced with host project saving, factory reset
  and desktop plugin updates. Hardware power, battery, card formatting and cable
  diagrams have no desktop procedure and were omitted.
- Cheat sheet and support (14–15): desktop shortcut and MIDI tables, troubleshooting
  and Temecula DSP contact. Cover, blank notes and original branding were omitted.

The illustrated guide now uses the redesigned editor throughout: the hero,
navigation image, overview and control close-ups all share the current faceplate.
Captures were rendered by the actual `NibbiEditor` from the local Release build
using `createComponentSnapshot`, with normal, labeled, alternate-page, armed-tape
and reset-dialog states. This excludes the OS/JUCE window title bar while retaining
the instrument's own toolbar. Thirteen annotated figures identify controls, pages,
Shift keys and the new icon toolbar. Six original SVG plates explain mouse gestures,
sampling, MELO/HITKIT, trim/envelopes, tape recording and pre/post effects; no source
manual artwork is reused. Desktop and mobile layouts use the same instructions.

The toolbar description matches the palette, legends, reset, arm, erase and Help icons.
DAW project saving replaces the removed Open/Save toolbar workflow; the standalone
app does not automatically reopen a performance. Standard macOS and Windows
installer paths use the shared download component, including its optional email
signup. The installer binaries are supplied separately; browser checks mock the
attachment and signup responses rather than publishing or submitting real email.

Remaining verification gaps: these captures establish the shared editor appearance,
not host-specific VST3/AU input routing or optional-bus support. DAW routing
instructions remain generic, pending host-by-host walkthroughs. The pre-existing
full-engine/hardware comparison gaps above remain unchanged.

Mode naming update (2026-09-30): the product calls chromatic playback **MELO**
and one-shot playback **HITKIT**. The website guide, editor tooltips/help,
host parameter display names and factory sample labels use those names. Factory
sample references recalled from older projects receive the current display name.
Serialized parameter IDs, factory archive paths and upstream engine identifiers
remain stable so existing projects, automation and factory audio keep working.
This is a presentation change; it does not change DSP or control behavior.


### Guide/code spot check (2026-09-30)

Compared the website's instructions and illustrations with `PluginEditor.cpp`,
`PluginProcessor.cpp`, `NormalPage.h`, `MenuPage.h`, `ControlRuntime.h`,
`TapeEngine.cpp`, `DSPEngine.h` and `LooperEngine.h`. Corrected the following:

- Drag-and-drop targets the live buffer and is limited to approximately 165 seconds;
  the loader's 180-second ceiling only applies when a stored slot is targeted.
- A complete Shift + Play/Record click changes feedback by 20 percentage points.
  `MenuPage::OnButton` changes it by 10 on both edges. A temporary harness linked
  to the current plugin observed 1.0 → 0.9 → 0.8 for Play and 0.8 → 0.9 → 1.0
  for Record. This inherited behavior was documented, not changed.
- Sampling instructions now explicitly release keyboard Shift. Starting sampling
  exits active tape recording/overdub; dry input monitoring requires SAMPLE mode.
- Incoming encoder CCs are absolute 0–127 values. The current Help toolbar icon
  opens `/nibbi#help`; screenshots and the icon reference now include it.

Rebuilt the local plugin/DSP check targets and passed all three CTest suites.
Their coverage includes MIDI timing, import, preset menus, tape arming and DAW
state recall. This is a source audit plus targeted runtime verification, not a
host-by-host interactive walkthrough; the routing and full-parity gaps above remain.
