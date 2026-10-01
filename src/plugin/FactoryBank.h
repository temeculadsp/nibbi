#pragma once
#include "../dsp/TapeEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace nibbi {
// Shared immutable PCM. Loading happens during instance construction, never
// in processBlock or in response to a MIDI note.
std::shared_ptr<const Catalog> factoryBank();
juce::String factoryBankError();
}
