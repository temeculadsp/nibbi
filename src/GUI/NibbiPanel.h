#pragma once
#include "NibbiLookAndFeel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace nibbi::gui
{
class ModeSwitch final : public juce::Button
{
public:
    ModeSwitch() : Button("Sample / Shift mode") { setClickingTogglesState(true); }
    void paintButton(juce::Graphics& g,bool over,bool down) override;
};
class Encoder : public juce::Slider
{
public:
    std::function<void()> onPress;
    std::function<bool(float)> onTurn;
    std::function<void()> onActivity;
    std::function<juce::String()> readoutText;
    bool isTurning() const { return turning; }
    void mouseDown(const juce::MouseEvent& e) override {
        lastY=e.position.y; turned=false; grabKeyboardFocus();
        if(!e.mods.isPopupMenu()) {turning=true;startedDragging();if(onActivity)onActivity();}
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        const auto delta=lastY-e.position.y; lastY=e.position.y;
        turned=turned || e.getDistanceFromDragStart()>2;
        turnRelative(delta);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        if(!e.mods.isPopupMenu()) {turning=false;stoppedDragging();if(onActivity)onActivity();}
        if(!turned && !e.mods.isPopupMenu() && onPress) onPress();
    }
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) override {
        juce::ignoreUnused(e);
        const float delta=(wheel.deltaY!=0?wheel.deltaY:-wheel.deltaX)*40.f;
        startedDragging(); turnRelative(delta); stoppedDragging();
    }
    void turnRelative(float delta) {
        if(delta==0) return;
        rotateThumb(delta*.035f);
        if(!(onTurn && onTurn(delta))) {
            const auto range=getNormalisableRange();
            const double next=juce::jlimit(0.,1.,range.convertTo0to1(getValue())+delta*.003);
            setValue(range.convertFrom0to1(next),juce::sendNotificationSync);
        }
        if(onActivity) onActivity();
    }
private:
    void rotateThumb(float radians) {
        if(radians==0) return;
        const float angle=float(getProperties()["thumbAngle"])+radians;
        getProperties().set("thumbAngle",std::fmod(angle,juce::MathConstants<float>::twoPi));
        repaint();
    }
    float lastY=0;
    bool turned=false,turning=false;
};
// Embed this in the instrument editor. The MIDI state must outlive the panel;
// feed it through MidiKeyboardState::processNextMidiBuffer in processBlock.
// All controls are exposed for the processor's own parameter attachments.
class NibbiPanel final : public juce::Component, private juce::Timer
{
public:
    static constexpr int designWidth = 1200, designHeight = 390;
    enum class Dial { pitch, start, end, effect, loop, input, count };

    explicit NibbiPanel (juce::MidiKeyboardState&);
    ~NibbiPanel() override;
    void paint (juce::Graphics&) override;
    static constexpr int backgroundCount=5;
    void setBackground(int index);
    juce::String backgroundName() const;
    void setLegendsVisible(bool visible) {legendsVisible_=visible;repaint();}
    bool legendsVisible() const {return legendsVisible_;}
    int getBackground() const { return backgroundIndex; }
    void resized() override;
    Encoder& dial (Dial id) { return dials[size_t (id)]; }
    std::function<bool(size_t)> onKeyDown;
    std::function<bool()> onBufferDown;
    std::function<void(size_t)> onPageLightClick;
    juce::TextButton& sampleModeLight() { return sampleModeLight_; }
    ModeSwitch& modeSwitch() { return modeSwitch_; }
    void setSamplerMode(bool shiftMode) {
        if(modeSwitch_.getToggleState()==shiftMode) return;
        modeSwitch_.setToggleState(shiftMode,juce::dontSendNotification); repaint();
    }
    void setPageLight(size_t i,int page) {
        pageLights[i].getProperties().set("page",page);
        pageLights[i].repaint();
    }
    void setSampleModeLight(bool latch) {
        if(bool(sampleModeLight_.getProperties()["latch"])==latch) return;
        sampleModeLight_.getProperties().set("latch",latch);
        sampleModeLight_.getProperties().set("badge",latch?"L":"H");
        sampleModeLight_.repaint();
    }
    void setLights(const std::array<uint32_t,35>& colours);
    void setDialLabel(Dial id, const juce::String& label) { dialLabels[size_t(id)]=label; repaint(); }
    juce::TextButton& recordButton() { return record; }
    juce::TextButton& playButton() { return play; }
    juce::TextButton& reverseButton() { return reverse; }
    juce::TextButton& octaveButton() { return octave; }
    // Call on the message thread. Changing octave releases held mouse notes.
    void setBaseNote (int midiNote);
    int getBaseNote() const noexcept { return baseNote; }
    void setMidiChannel (int channel);
    void releaseNotes();

private:
    void timerCallback() override;
    void drawFaceplate (juce::Graphics&);
    void drawLiveLights(juce::Graphics&);
    void drawKeyNumbers(juce::Graphics&);
    void showReadout(size_t);
    void updateReadout();
    juce::Label valueReadout;
    int readoutDial=-1;
    double readoutUntil=0;
    void updateKey (size_t);
    NibbiLookAndFeel look;
    int backgroundIndex=0;
    std::array<juce::Image,3> faceplateImages;
    bool legendsVisible_=false;
    juce::MidiKeyboardState& midi;
    juce::Component surface;
    std::array<Encoder, size_t (Dial::count)> dials;
    // Pitch, start, end, effect, transport, and output indicators.
    std::array<juce::TextButton,6> pageLights;
    juce::TextButton sampleModeLight_;
    ModeSwitch modeSwitch_;
    std::array<juce::String, size_t (Dial::count)> dialLabels;
    std::array<juce::TextButton, 24> keys;
    std::array<bool, 24> held {};
    std::array<bool, 24> consumed {};
    juce::TextButton record { "Record sample" }, play { "Play / pause" }, reverse { "Reverse" };
    juce::TextButton octave { "C5" };
    bool topKeyHeld=false;
    bool topKeyConsumed=false;
    std::array<juce::Colour,10> lampColours{};
    int baseNote = 48, midiChannel = 1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NibbiPanel)
};
}
