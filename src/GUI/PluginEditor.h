#pragma once
#include "../plugin/PluginProcessor.h"
#include "NibbiPanel.h"
#include "TitleBar.h"
#include "AppearancePreferences.h"
#include "FactoryResetPrompt.h"
// Processor adapter for the supplied faceplate. NibbiPanel owns its artwork.
class NibbiEditor final : public juce::AudioProcessorEditor, private juce::Timer,
 private juce::MidiKeyboardStateListener, public juce::FileDragAndDropTarget {
public:
 explicit NibbiEditor(NibbiProcessor&,juce::File preferencesFile=nibbi::gui::AppearancePreferences::defaultFile());
 ~NibbiEditor() override;
 void paint(juce::Graphics&) override;
 void resized() override;
 void modifierKeysChanged(const juce::ModifierKeys&) override;
 bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
 void filesDropped(const juce::StringArray&,int,int) override;
 void fileDragEnter(const juce::StringArray&,int,int) override;
 void fileDragExit(const juce::StringArray&) override;
 nibbi::gui::NibbiPanel& panel() { return panel_; }
 juce::TextButton& factoryResetButton() { return factoryReset_; }
private:
 NibbiProcessor& proc_;
 nibbi::gui::AppearancePreferences preferences_;
 juce::MidiKeyboardState keys_;
 juce::Component surface_;
 nibbi::gui::TitleBar titleBar_;
 using ToolbarButton=nibbi::gui::ToolbarButton;
 ToolbarButton legends_{"Show legends",ToolbarButton::legends};
 ToolbarButton background_{"Change skin",ToolbarButton::skin};
 ToolbarButton help_{"Help",ToolbarButton::help};
 nibbi::gui::FactoryResetPrompt resetPrompt_;
 nibbi::gui::NibbiPanel panel_{keys_};
 nibbi::gui::TapeStatus tapeStatus_;
 juce::Label statusLabel_;
 ToolbarButton factoryReset_{"Factory reset",ToolbarButton::reset};
 std::array<int,6> pages_{};
 bool shift_=false,keyboardShift_=false,recordDown_=false,playDown_=false,loopDown_=false;
 std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments_;
 ToolbarButton clear_{"Erase loop",ToolbarButton::erase},arm_{"Arm tape",ToolbarButton::arm};
 juce::TooltipWindow tooltips_{this,350};
 juce::ThreadPool loader_{1};
 juce::String status_;
 bool syncingKeys_=false;
 std::array<bool,128> externalNotes_{};
 void bindDials();
 void updateShift();
 void updateMenuLights();
 bool turn(size_t,float);
 void press(size_t);
 bool keyDown(size_t);
 bool bufferDown();
 void setParameter(const char*,float);
 float value(const char*) const;
 void timerCallback() override;
 void handleNoteOn(juce::MidiKeyboardState*,int,int,float) override;
 void handleNoteOff(juce::MidiKeyboardState*,int,int,float) override;
 void load(const juce::File&);
 void updateSkinTooltip();
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NibbiEditor)
};
