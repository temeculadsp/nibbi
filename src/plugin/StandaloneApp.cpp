#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "StandaloneSettings.h"

namespace {
class NibbiWindow final : public juce::StandaloneFilterWindow {
public:
 using StandaloneFilterWindow::StandaloneFilterWindow;
 void closeButtonPressed() override {
  // The stock window serializes the entire instrument here.
  juce::JUCEApplicationBase::getInstance()->systemRequestedQuit();
 }
};

class NibbiStandaloneApp final : public juce::JUCEApplication {
public:
 const juce::String getApplicationName() override { return JucePlugin_Name; }
 const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
 bool moreThanOneInstanceAllowed() override { return false; }
 void anotherInstanceStarted(const juce::String&) override {
  if(window_) window_->toFront(true);
 }
 void initialise(const juce::String&) override {
  juce::PropertiesFile::Options options;
  // Keep device preferences stable when the displayed product name changes.
  options.applicationName="nibbi";
  options.filenameSuffix=".settings";
  options.osxLibrarySubFolder="Application Support";
#if JUCE_LINUX || JUCE_BSD
  options.folderName="~/.config";
#endif
  properties_.setStorageParameters(options);
  // Migrate device selection only; the instrument still opens in factory state.
  if(!properties_.getUserSettings()->containsKey("audioSetup")) {
   auto legacyOptions=options;legacyOptions.applicationName="Chomp";
   juce::PropertiesFile legacy(legacyOptions);
   juce::PropertySet devices;nibbi::restoreStandaloneDevices(legacy,devices);
   nibbi::saveStandaloneDevices(devices,*properties_.getUserSettings());
  }
  nibbi::restoreStandaloneDevices(*properties_.getUserSettings(),runtimeSettings_);
  auto holder=std::make_unique<juce::StandalonePluginHolder>(&runtimeSettings_,false);
  if(juce::Desktop::getInstance().getDisplays().displays.isEmpty()) {
   headlessHolder_=std::move(holder);
   return;
  }
  window_=std::make_unique<NibbiWindow>(getApplicationName(),
   juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
   std::move(holder));
  window_->setVisible(true);
 }
 void systemRequestedQuit() override {
  if(juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
   juce::Timer::callAfterDelay(100,[] {
    if(auto* app=juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit();
   });
  else quit();
 }
 void shutdown() override {
  // Destroying the holder writes its device setup into runtimeSettings_.
  headlessHolder_.reset(); window_.reset();
  nibbi::saveStandaloneDevices(runtimeSettings_,*properties_.getUserSettings());
  properties_.saveIfNeeded();
 }
private:
 juce::ApplicationProperties properties_;
 juce::PropertySet runtimeSettings_;
 std::unique_ptr<NibbiWindow> window_;
 std::unique_ptr<juce::StandalonePluginHolder> headlessHolder_;
};
}
JUCE_CREATE_APPLICATION_DEFINE(NibbiStandaloneApp)
