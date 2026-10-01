# Nibbi

The Temecula DSP Nibbi sampler instrument: seven-voice
sampling, MELO and HITKIT banks, sound shaping, stereo effects, and tape looping.

[Download Nibbi and read the illustrated guide](https://temeculadsp.com/nibbi).

This repository contains the instrument source, factory samples, faceplate
artwork, and test sources with reference fixtures.

## Source

- `src/GUI`: the instrument interface and artwork.
- `src/control`: control and menu behavior.
- `src/engine` and `src/dsp`: audio engine and desktop adapters.
- `src/plugin`: plugin processor and application integration.
- `src/resources`: bundled factory samples and instrument settings.
- `src/tests`: reference fixtures and test source.

## Compatibility and verification

Some internal identifiers and factory archive names retain their upstream names
for saved-session and automation compatibility. The interface uses MELO and HITKIT.

Tests cover desktop DSP, MIDI timing, sample import, session recall, and selected
interface interactions. Upstream comparisons cover sampler and looper scenarios;
they do not establish complete engine or hardware parity. Host-specific audio
routing still needs verification across individual DAWs.

## Credits and licenses

Nibbi is an independent desktop port of the CHOMPI TAPE engine. Original source
attributions and third-party notices are preserved in `licenses/` and the source
files. CHOMPI and related marks belong to their respective owners; this is not
an official CHOMPI Club release. JUCE is a separate dependency under its own
license.
