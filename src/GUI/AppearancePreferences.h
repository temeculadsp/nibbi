#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace nibbi::gui {
// User appearance is shared by AU, VST3 and standalone, outside instrument state.
class AppearancePreferences {
public:
 static juce::PropertiesFile::Options options() {
  static juce::InterProcessLock lock("com.temeculadsp.nibbi.appearance");
  juce::PropertiesFile::Options o;
  o.applicationName="nibbi-appearance";o.filenameSuffix="settings";
  o.folderName="Temecula DSP";o.osxLibrarySubFolder="Application Support";
  o.storageFormat=juce::PropertiesFile::storeAsXML;o.millisecondsBeforeSaving=-1;
  o.processLock=&lock;
  return o;
 }
 static juce::File defaultFile() {return options().getDefaultFile();}
 explicit AppearancePreferences(juce::File file=defaultFile()):file_(std::move(file)) {}
 int skin(int count) const {
  juce::PropertiesFile settings(file_,options());
  return juce::jlimit(0,count-1,settings.getIntValue("skin",0));
 }
 bool setSkin(int index) {
  juce::PropertiesFile settings(file_,options());
  settings.setValue("skin",index);return settings.saveIfNeeded();
 }
private:
 juce::File file_;
};
}
