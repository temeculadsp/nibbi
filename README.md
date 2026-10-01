# Nibbi

Source snapshot of the Temecula DSP Nibbi sampler instrument: seven-voice
sampling, MELO and HITKIT banks, sound shaping, stereo effects, and tape looping.

[Download Nibbi and read the illustrated guide](https://temeculadsp.com/nibbi).

This repository contains the instrument source, factory samples, faceplate
artwork, and test sources with reference fixtures. Build projects, dependency setup,
signing configuration, packaging tools, and release scripts are not included.
The website and other Temecula DSP products are not part of this repository.

## Source

- `src/GUI`: the instrument interface and artwork.
- `src/control`: control and menu behavior.
- `src/engine` and `src/dsp`: audio engine and desktop adapters.
- `src/plugin`: plugin processor and application integration.
- `src/resources`: bundled factory samples and instrument settings.
- `src/tests`: reference fixtures and test source.

## Credits and licenses

Nibbi is an independent desktop port of the CHOMPI TAPE engine. Original source
attributions and third-party notices are preserved in `licenses/` and the source
files. CHOMPI and related marks belong to their respective owners; this is not
an official CHOMPI Club release. JUCE is a separate dependency under its own
license and is not included here.
