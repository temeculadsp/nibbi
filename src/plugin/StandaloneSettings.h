#pragma once
#include <juce_core/juce_core.h>

namespace nibbi {
// Only device configuration crosses app launches. The holder receives this
// separate in-memory set, so it cannot reload an old instrument session.
inline void restoreStandaloneDevices(const juce::PropertySet& saved,juce::PropertySet& runtime) {
 runtime.clear();
 for(const auto* key:{"audioSetup","shouldMuteInput"})
  if(saved.containsKey(key)) runtime.setValue(key,saved.getValue(key));
}
inline void saveStandaloneDevices(const juce::PropertySet& runtime,juce::PropertySet& saved) {
 for(const auto* key:{"filterState","lastStateFile","windowX","windowY"}) saved.removeValue(key);
 for(const auto* key:{"audioSetup","shouldMuteInput"})
  if(runtime.containsKey(key)) saved.setValue(key,runtime.getValue(key));
}
}
