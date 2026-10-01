#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace nibbi::gui {
class FactoryResetPrompt final : public juce::Component {
public:
 std::function<void()> onConfirm;
 FactoryResetPrompt() {
  setComponentID("factoryResetPrompt");setName("Reset to factory?");
  setAlwaysOnTop(true);
  addAndMakeVisible(cancel_);addAndMakeVisible(reset_);
  cancel_.setComponentID("cancelReset");reset_.setComponentID("confirmReset");
  cancel_.onClick=[this] {dismiss();};
  reset_.onClick=[this] {dismiss();if(onConfirm)onConfirm();};
  reset_.setColour(juce::TextButton::buttonColourId,juce::Colour(0xffbcd9cd));
  cancel_.setColour(juce::TextButton::buttonColourId,juce::Colour(0xffb9b1d1));
  for(auto* button:{&reset_,&cancel_}) {
   button->setColour(juce::TextButton::textColourOffId,juce::Colour(0xff303044));
   button->setColour(juce::TextButton::textColourOnId,juce::Colour(0xff303044));
  }
  setVisible(false);
 }
 void open() {setVisible(true);toFront(false);enterModalState(false);if(isShowing())cancel_.grabKeyboardFocus();}
 void dismiss() {exitModalState(0);setVisible(false);}
 bool keyPressed(const juce::KeyPress& key) override {
  if(key==juce::KeyPress::escapeKey) {dismiss();return true;}return false;
 }
 void resized() override {
  card_=getLocalBounds().withSizeKeepingCentre(430,184);
  cancel_.setBounds(card_.getRight()-222,card_.getBottom()-48,94,28);
  reset_.setBounds(card_.getRight()-116,card_.getBottom()-48,94,28);
 }
 void paint(juce::Graphics& g) override {
  g.fillAll(juce::Colours::black.withAlpha(.55f));
  g.setColour(juce::Colour(0xff25282e));g.fillRoundedRectangle(card_.toFloat(),10);
  g.setColour(juce::Colour(0xff595c65));g.drawRoundedRectangle(card_.toFloat().reduced(.5f),10,1);
  g.setColour(juce::Colour(0xfff0eade));g.setFont(juce::FontOptions(21.f,juce::Font::bold));
  g.drawText("Reset to factory?",card_.reduced(24).removeFromTop(30),juce::Justification::centredLeft);
  g.setFont(juce::FontOptions(14.f));g.setColour(juce::Colour(0xffd1d0cb));
  g.drawFittedText("Are you sure? This clears your recordings and tape loop, and restores all factory samples and controls.",
                  card_.getX()+24,card_.getY()+62,card_.getWidth()-48,48,juce::Justification::topLeft,3);
 }
private:
 juce::TextButton cancel_{"Cancel"},reset_{"Reset"};
 juce::Rectangle<int> card_;
};
}
