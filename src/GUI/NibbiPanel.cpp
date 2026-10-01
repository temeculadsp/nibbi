#include "NibbiPanel.h"
#include "Wordmark.h"
#include "PanelSilkscreen.h"
#include "NibbiFaceplates.h"

namespace nibbi::gui
{
namespace
{
// In the faceplate reference, the enclosure spans x=86..1264; the key bed starts about
// 42 pixels inside either edge. Fit the complete control layout to that width,
// keeping square keycaps and circular knobs. The title bar is separate.
constexpr float faceplateScale=1200.f/1178.f;
const auto faceplateTransform=juce::AffineTransform::scale(faceplateScale).translated(-10,-140);
constexpr int faceplateHeight=540;
constexpr std::array<int, 24> semitones { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23,
                                        1, 3, 6, 8, 10, 13, 15, 18, 20, 22 };
constexpr std::array<int, 10> blackAfter { 0, 1, 3, 4, 5, 7, 8, 10, 11, 12 };
const std::array<juce::Colour, 10> keyAccents { ink::purple, ink::purple, ink::coral,
    ink::coral, ink::coral, ink::yellow, ink::yellow, ink::coral, ink::mint, ink::blue };
const std::array<const char*,10> menuNames { "MELO - chromatic samples",
    "HITKIT - one-shot / drum samples", "Microphone", "Aux input", "Resample",
    "FX before looper", "FX after looper", "Erase preset", "Copy preset", "Save buffer" };

void stroke (juce::Graphics& g, std::initializer_list<juce::Point<float>> points, float width = 2.2f)
{
    juce::Path path;
    bool first = true;
    for (auto point : points)
    {
        if (first) path.startNewSubPath (point); else path.lineTo (point);
        first = false;
    }
    g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

void text (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> bounds, float size,
           juce::Colour colour = ink::paper)
{
    g.setColour (colour);
    g.setFont (juce::FontOptions (size, juce::Font::bold));
    g.drawText (s, bounds, juce::Justification::centred);
}
void caption(juce::Graphics& g,const juce::String& label,juce::Rectangle<float> bounds) {
    g.setColour(juce::Colours::black); g.fillRoundedRectangle(bounds,3.f);
    text(g,label,bounds.reduced(3,0),12.f);
}
}

void ModeSwitch::paintButton(juce::Graphics& g,bool over,bool down) {
    auto plate=getLocalBounds().toFloat().reduced(6,4);
    g.setGradientFill({ink::gold.brighter(.2f),plate.getX(),plate.getY(),
                       ink::gold.darker(.25f),plate.getRight(),plate.getBottom(),false});
    g.fillRoundedRectangle(plate,2.f);
    auto slot=plate.reduced(6,5);
    g.setColour(juce::Colour(0xff080a09)); g.fillRoundedRectangle(slot,3.f);
    const float gripY=getToggleState()?slot.getBottom()-7:slot.getY()+7;
    auto grip=juce::Rectangle<float>(16,15).withCentre({slot.getCentreX(),gripY});
    g.setColour(juce::Colours::black.withAlpha(.8f));
    g.fillRoundedRectangle(grip.translated(1,2),2.f);
    g.setGradientFill({juce::Colour(0xff353937),grip.getX(),grip.getY(),
                       juce::Colour(0xff131715),grip.getX(),grip.getBottom(),false});
    g.fillRoundedRectangle(grip,2.f);
    g.setColour(juce::Colour(0xff484d49));
    g.drawRoundedRectangle(grip.reduced(.5f),2.f,.8f);
    // Molded horizontal ribs give the plastic slider its coarse thumb grip.
    for(int rib=0;rib<4;++rib) {
        const float y=grip.getY()+3.f+float(rib)*3.f;
        g.setColour(juce::Colour(0xff090c0a));
        g.drawLine(grip.getX()+3,y+1,grip.getRight()-3,y+1,1.f);
        g.setColour(juce::Colour(0xff555c56));
        g.drawLine(grip.getX()+3,y,grip.getRight()-3,y,.8f);
    }
    if(over || down || hasKeyboardFocus(false)) {
        g.setColour(ink::paper.withAlpha(.6f)); g.drawRoundedRectangle(plate,2.f,1.f);
    }
}

void NibbiPanel::setBackground(int index) {
    backgroundIndex=juce::jlimit(0,backgroundCount-1,index);
    for(auto& knob:dials) {
        knob.getProperties().set("darkInk",backgroundIndex==1);
        knob.getProperties().set("hideSilkscreen",backgroundIndex>=2);knob.repaint();
    }
    repaint();
}

juce::String NibbiPanel::backgroundName() const {
    static const char* names[]={"BLACK","PINK","FACEPLATE 1","FACEPLATE 2","FACEPLATE 3"};
    return names[backgroundIndex];
}

NibbiPanel::NibbiPanel (juce::MidiKeyboardState& state) : midi (state)
{
    faceplateImages={
        juce::ImageCache::getFromMemory(NibbiFaceplates::nibbi_bg_1_png,NibbiFaceplates::nibbi_bg_1_pngSize),
        juce::ImageCache::getFromMemory(NibbiFaceplates::nibbi_bg_2_png,NibbiFaceplates::nibbi_bg_2_pngSize),
        juce::ImageCache::getFromMemory(NibbiFaceplates::nibbi_bg_3_png,NibbiFaceplates::nibbi_bg_3_pngSize)};
    setBackground(0);
    setName ("NIBBI instrument");
    setLookAndFeel (&look);
    setOpaque (true);
    addAndMakeVisible (surface);
    surface.setInterceptsMouseClicks (false, true);
    const std::array<const char*, 6> names { "Pitch", "Sample start", "Sample end", "Effect amount",
                                           "Loop position", "Input level" };
    for (size_t i = 0; i < dials.size(); ++i)
    {
        auto& knob = dials[i];
        surface.addAndMakeVisible (knob);
        knob.setName (names[i]);
        knob.setTooltip (juce::String (names[i]) + " — drag or scroll to turn");
        knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setRange (0, 1, 0.001);
        const double initial = i == 1 ? 0.0 : i == 2 ? 1.0 : 0.5;
        knob.setValue (initial, juce::dontSendNotification);
        knob.setDoubleClickReturnValue (false, initial);
        knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.23f,
                                  juce::MathConstants<float>::pi * 2.77f, true);
        knob.setWantsKeyboardFocus (true);
        knob.setPopupDisplayEnabled (false, false, nullptr);
        knob.onActivity=[this,i] {showReadout(i);};
    }
    addChildComponent(valueReadout);
    valueReadout.setComponentID("encoderValue");
    valueReadout.setInterceptsMouseClicks(false,false);
    valueReadout.setJustificationType(juce::Justification::centred);
    valueReadout.setFont(juce::FontOptions(14.f,juce::Font::bold));
    valueReadout.setColour(juce::Label::backgroundColourId,juce::Colour(0xff101311));
    valueReadout.setColour(juce::Label::textColourId,ink::paper);
    valueReadout.setColour(juce::Label::outlineColourId,ink::gold);
    dial (Dial::loop).getProperties().set ("large", true);
    dial (Dial::loop).getProperties().set ("thumbAngle",juce::MathConstants<float>::pi*.25f);
    const std::array<juce::Colour,6> lightColours { ink::mint, ink::yellow, ink::coral,
                                                     ink::purple, ink::cyan, ink::yellow };
    for(size_t i=0;i<pageLights.size();++i)
    {
        auto& light=pageLights[i];
        surface.addAndMakeVisible(light);
        light.setName(i==4?"Reset tape speed":"Encoder page");
        light.getProperties().set("kind","led");
        light.getProperties().set("page",i==4?-1:0);
        light.setColour(juce::TextButton::buttonColourId,lightColours[i]);
        light.setTooltip(i==4?"Reset tape speed":"Click to change the encoder function");
        light.onClick=[this,i] { if(onPageLightClick) onPageLightClick(i); };
        light.setWantsKeyboardFocus(true);
    }
    surface.addAndMakeVisible(sampleModeLight_);
    surface.addAndMakeVisible(modeSwitch_);
    modeSwitch_.setTooltip("Up: hold the sample button to record. Down: hold it for Shift.");
    modeSwitch_.onStateChange=[this] { repaint(); };
    sampleModeLight_.setName("Sampling button mode");
    sampleModeLight_.getProperties().set("kind","led");
    sampleModeLight_.getProperties().set("badge","H");
    sampleModeLight_.getProperties().set("latch",false);
    sampleModeLight_.setColour(juce::TextButton::buttonColourId,ink::purple);
    sampleModeLight_.setTooltip("H: hold the sample button to record. L: click it to start and stop.");
    sampleModeLight_.setWantsKeyboardFocus(true);
    auto setup = [this] (juce::TextButton& button, const char* kind, juce::Colour colour, bool toggle)
    {
        surface.addAndMakeVisible (button);
        button.getProperties().set ("kind", kind);
        button.setColour (juce::TextButton::buttonColourId, colour);
        button.setColour (juce::TextButton::buttonOnColourId, colour.brighter (0.2f));
        button.setClickingTogglesState (toggle);
        button.setTooltip (button.getButtonText());
    };
    setup (record, "record", palette::sampleButton, true);
    setup (play, "play", palette::playButton, true);
    setup (reverse, "record", palette::recordButton, true);
    for (size_t i = 0; i < keys.size(); ++i)
    {
        const bool black = i >= 14;
        setup (keys[i], black ? "black" : "white", black ? palette::modeKey : palette::noteKey, false);
        keys[i].getProperties().set ("accent", (black ? keyAccents[i - 14] : ink::purple).toString());
        keys[i].onStateChange = [this, i] { updateKey (i); };
    }
    setup (octave, "white", palette::noteKey, false);
    octave.getProperties().set ("accent", ink::coral.toString());
    octave.onStateChange = [this] {
        bool down=octave.isDown();
        if(down==topKeyHeld) return;
        topKeyHeld=down;
        if(down) { topKeyConsumed=onBufferDown && onBufferDown(); if(!topKeyConsumed) midi.noteOn(midiChannel,baseNote+24,1.f); }
        else { if(!topKeyConsumed) midi.noteOff(midiChannel,baseNote+24,0); topKeyConsumed=false; }
    };
    setBaseNote (48);
    setSize (designWidth, designHeight);
    startTimerHz (30);
}

NibbiPanel::~NibbiPanel()
{
    stopTimer();
    for (auto& key : keys) key.onStateChange = nullptr;
    octave.onStateChange=nullptr;
    releaseNotes();
    setLookAndFeel (nullptr);
}

void NibbiPanel::releaseNotes()
{
    if(topKeyHeld) { if(!topKeyConsumed) midi.noteOff(midiChannel,baseNote+24,0); topKeyHeld=false; topKeyConsumed=false; }
    for (size_t i = 0; i < held.size(); ++i)
        if (held[i])
        {
            if(!consumed[i]) midi.noteOff (midiChannel, baseNote + semitones[i], 0);
            held[i] = false;
            consumed[i] = false;
        }
}

void NibbiPanel::setBaseNote (int note)
{
    releaseNotes();
    baseNote = juce::jlimit (0, 103, note);
    for (size_t i = 0; i < keys.size(); ++i)
    {
        const auto name = juce::MidiMessage::getMidiNoteName (baseNote + semitones[i], true, true, 4);
        keys[i].setName (name);
        keys[i].setTooltip (i<14?name:name+" — Shift: "+menuNames[i-14]+(i<16?"; press again to cycle banks":""));
    }
    octave.setTooltip(juce::MidiMessage::getMidiNoteName(baseNote+24,true,true,4));
    repaint();
}

void NibbiPanel::setMidiChannel (int channel)
{
    releaseNotes();
    midiChannel = juce::jlimit (1, 16, channel);
}

void NibbiPanel::updateKey (size_t i)
{
    const bool pressed = keys[i].isDown();
    if (pressed == held[i]) return;
    held[i] = pressed;
    if (pressed) { consumed[i]=onKeyDown && onKeyDown(i); if(!consumed[i]) midi.noteOn (midiChannel, baseNote + semitones[i], 1.f); }
    else { if(!consumed[i]) midi.noteOff (midiChannel, baseNote + semitones[i], 0); consumed[i]=false; }
}

void NibbiPanel::setLights(const std::array<uint32_t,35>& colours)
{
    auto set=[](juce::Button& button,uint32_t rgb) {
        if(button.getProperties()["ledRgb"]==juce::var(int(rgb)))return;
        button.getProperties().set("ledRgb",int(rgb));button.repaint();
    };
    bool changed=false;
    for(size_t i=0;i<10;++i) {
        const juce::Colour c(colours[i]);changed|=lampColours[i]!=c;lampColours[i]=c;
    }
    constexpr size_t lampIds[]={1,2,3,4,5,9};
    for(size_t i=0;i<6;++i)set(pageLights[i],colours[lampIds[i]]);
    set(sampleModeLight_,colours[0]);
    for(size_t i=0;i<14;++i)set(keys[i],colours[10+24-i]);
    for(size_t i=0;i<10;++i)set(keys[14+i],colours[10+i]);
    set(octave,colours[20]);
    if(changed)repaint();
}

void NibbiPanel::showReadout(size_t index) {
    readoutDial=int(index);
    readoutUntil=juce::Time::getMillisecondCounterHiRes()+1200.;
    valueReadout.setVisible(true);
    valueReadout.toFront(false);
    updateReadout();
}

void NibbiPanel::updateReadout() {
    if(readoutDial<0)return;
    auto& knob=dials[size_t(readoutDial)];
    if(!knob.isTurning() && juce::Time::getMillisecondCounterHiRes()>readoutUntil) {
        readoutDial=-1;valueReadout.setVisible(false);return;
    }
    valueReadout.setText(knob.readoutText?knob.readoutText():knob.getTextFromValue(knob.getValue()),juce::dontSendNotification);
    const auto bounds=getLocalArea(&knob,knob.getLocalBounds());
    valueReadout.setBounds(juce::jlimit(0,juce::jmax(0,getWidth()-132),bounds.getCentreX()-66),
                          juce::jmax(0,bounds.getY()-29),132,26);
}

void NibbiPanel::timerCallback()
{
    updateReadout();
    octave.setToggleState(midi.isNoteOn(midiChannel,baseNote+24),juce::dontSendNotification);
    // Polling keeps MIDI callbacks (which may occur on the audio thread) away from GUI code.
    for (size_t i = 0; i < keys.size(); ++i)
        keys[i].setToggleState (midi.isNoteOn (midiChannel, baseNote + semitones[i]), juce::dontSendNotification);
}

void NibbiPanel::resized()
{
    const float scale = juce::jmin (float (getWidth()) / designWidth, float (getHeight()) / designHeight);
    surface.setBounds (0, 0, designWidth, faceplateHeight);
    const auto viewport=juce::AffineTransform::scale(scale).translated(
        (float(getWidth())-designWidth*scale)*.5f,
        (float(getHeight())-designHeight*scale)*.5f);
    surface.setTransform(faceplateTransform.followedBy(viewport));
    dial (Dial::pitch).setBounds (225, 213, 91, 91);
    dial (Dial::start).setBounds (345, 213, 91, 91);
    dial (Dial::end).setBounds (467, 213, 91, 91);
    dial (Dial::effect).setBounds (588, 213, 91, 91);
    dial (Dial::loop).setBounds (696, 178, 163, 163);
    dial (Dial::input).setBounds (1065, 213, 91, 91);
    const std::array<int,6> lightX { 271,392,513,634,744,1110 };
    for(size_t i=0;i<pageLights.size();++i)
    {
        const int radius=i==4?13:21;
        const int cy=i==4?176:185;
        pageLights[i].setBounds(lightX[i]-radius-8,cy-radius-8,2*radius+16,2*radius+16);
    }
    sampleModeLight_.setBounds(133,156,58,58);
    modeSwitch_.setBounds(58,237,36,47);
    record.setBounds (122, 222, 78, 82);
    play.setBounds (852, 222, 76, 82);
    reverse.setBounds (928, 222, 76, 82);
    for (int i = 0; i < 14; ++i) keys[size_t (i)].setBounds (51 + i * 73, 404, 73, 74);
    for (size_t i = 0; i < blackAfter.size(); ++i)
        keys[14 + i].setBounds (87 + blackAfter[i] * 73, 331, 73, 75);
    octave.setBounds (1073, 404, 73, 74);
}

void NibbiPanel::paint (juce::Graphics& g)
{
    g.fillAll(backgroundIndex==1?juce::Colour(0xffe8a3bd):juce::Colours::black);
    juce::Graphics::ScopedSaveState save (g);
    const float scale=juce::jmin(float(getWidth())/designWidth,float(getHeight())/designHeight);
    g.addTransform(juce::AffineTransform::scale(scale).translated(
        (float(getWidth())-designWidth*scale)*.5f,
        (float(getHeight())-designHeight*scale)*.5f));
    if(backgroundIndex>=2) {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(faceplateImages[size_t(backgroundIndex-2)],{0,0,float(designWidth),float(designHeight)},
                    juce::RectanglePlacement::centred);
    }
    g.addTransform(faceplateTransform);
    if(backgroundIndex<2)drawFaceplate(g);
    drawLiveLights(g);
    drawKeyNumbers(g);
    if(!legendsVisible_)return;
    for(size_t i=0;i<dials.size();++i) {
        auto bounds=dials[i].getBounds().toFloat();
        caption(g,dialLabels[i],{bounds.getX()-8,140,bounds.getWidth()+16,18});
    }
    caption(g,modeSwitch_.getToggleState()?"SHIFT":"SAMPLE RECORD",{111,140,102,18});
    caption(g,modeSwitch_.getToggleState()?"SHIFT":"SAMPLE",{27,177,78,18});
    caption(g,"PLAY / PAUSE",{842,140,88,18});
    caption(g,"RECORD / DUB",{934,140,89,18});
}

void NibbiPanel::drawFaceplate (juce::Graphics& g)
{
    const auto inkColour=backgroundIndex==1?juce::Colours::black:ink::paper;
    const auto inverseInk=backgroundIndex==1?juce::Colour(0xffe8a3bd):juce::Colour(0xff292c28);
    // Recessed beds for the chromatic keys.
    g.setColour (juce::Colour (0xff080a09));
    for (auto r : { juce::Rectangle<float> (86, 330, 147, 77), { 305, 330, 220, 77 },
                   { 597, 330, 147, 77 }, { 816, 330, 220, 77 }, { 50, 404, 1096, 75 } })
        g.fillRect (r);

    // Cream and gold silkscreen borders.
    g.setColour (inkColour);
    stroke (g, { { 119, 210 }, { 207, 210 }, { 211, 216 }, { 211, 307 }, { 205, 312 }, { 119, 312 } }, 3);
    g.setColour (ink::gold);
    g.drawRect (118.0f, 218.0f, 87.0f, 88.0f, 4.0f);
    g.setColour (inkColour);
    // Smooth vector curves following the supplied reference's printed design.
    g.fillPath(silkscreen::pitch());g.fillPath(silkscreen::trim());
    g.fillPath(silkscreen::effect());g.fillPath(silkscreen::volume());
    g.fillPath(silkscreen::wheel());g.fillPath(silkscreen::transport());
    g.fillPath(silkscreen::transportLamps());
    constexpr float wheelX=777.5f,wheelY=259.5f;
    // The inner gold bezel is also one path: there is no vertical rectangle
    // edge beneath the wheel, and the encoder paints only its rotating face.
    constexpr float goldRadius=68.5f;
    const float goldTopAngle=std::acos((wheelY-219.f)/goldRadius);
    const float goldBottomAngle=std::acos((wheelY-306.f)/goldRadius);
    const juce::Point<float> wheelCentre(wheelX,wheelY);
    juce::Path goldBezel;
    goldBezel.startNewSubPath(wheelCentre.getPointOnCircumference(goldRadius,goldTopAngle));
    goldBezel.lineTo(1002,219);goldBezel.quadraticTo(1005,219,1005,222);
    goldBezel.lineTo(1005,303);goldBezel.quadraticTo(1005,306,1002,306);
    goldBezel.lineTo(wheelCentre.getPointOnCircumference(goldRadius,goldBottomAngle));
    goldBezel.addCentredArc(wheelX,wheelY,goldRadius,goldRadius,0,
                           goldBottomAngle,goldTopAngle+juce::MathConstants<float>::twoPi);
    goldBezel.closeSubPath();
    g.setColour(ink::gold);
    g.strokePath(goldBezel,juce::PathStrokeType(4.f,juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    g.setColour(inkColour);
    // Tape / input routing at the left of the record key.
    g.drawRoundedRectangle (67, 208, 44, 27, 3, 2);
    g.drawRoundedRectangle (67, 285, 44, 27, 3, 2);
    for (float x : { 81.0f, 90.0f, 99.0f })
    {
        g.drawEllipse (x - 8, 212, 16, 19, 1.8f);
        g.drawEllipse (x - 8, 289, 16, 19, 1.8f);
    }
    stroke (g, { { 66, 220 }, { 52, 220 }, { 48, 226 }, { 48, 246 }, { 34, 246 }, { 34, 253 },
                 { 62, 253 }, { 62, 259 }, { 40, 259 }, { 40, 265 }, { 58, 265 } });
    stroke (g, { { 66, 299 }, { 50, 299 }, { 47, 292 }, { 47, 275 }, { 57, 275 } });
    g.setColour (ink::gold);
    g.fillRect (64, 241, 24, 39);
    g.setColour (juce::Colour (0xff080a09));
    g.fillRect (70, 247, 13, 25);

    // Speed and effects pictograms beneath the small knobs.
    g.setColour (inkColour);
    g.fillRoundedRectangle (255, 298, 31, 27, 3);
    g.setColour (inverseInk);
    juce::Path speed;
    speed.addCentredArc (270, 318, 11, 11, 0, -juce::MathConstants<float>::halfPi,
                         juce::MathConstants<float>::halfPi, true);
    g.strokePath (speed, juce::PathStrokeType (1.5f));
    stroke (g, { { 259, 320 }, { 281, 320 } }, 1.5f);
    stroke (g, { { 270, 318 }, { 275, 309 } }, 1.5f);
    for (int i = 0; i < 5; ++i)
    {
        const auto p = juce::Point<float> (270, 318).getPointOnCircumference (8, -1.5f + float (i) * 0.75f);
        g.fillEllipse (p.x - 1, p.y - 1, 2, 2);
    }
    // Star-tipped wand with a rounded handle, collar and small sparkle cuts.
    g.setColour(inkColour);
    juce::Path wand;
    wand.startNewSubPath(622,324);wand.lineTo(639,312);
    g.strokePath(wand,juce::PathStrokeType(4.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    juce::Path star;
    star.addStar({644,308},5,3.6f,8.2f,-.15f);
    g.fillPath(star);
    g.setColour(backgroundIndex==1?juce::Colour(0xffe8a3bd):juce::Colours::black);
    g.drawLine(633,313,636,317,1.7f);
    g.setColour(inkColour);
    auto sparkle=[&g](float x,float y,float radius) {
        juce::Path p;p.startNewSubPath(x,y-radius);p.quadraticTo(x,y,x+radius,y);
        p.quadraticTo(x,y,x,y+radius);p.quadraticTo(x,y,x-radius,y);
        p.quadraticTo(x,y,x,y-radius);p.closeSubPath();g.fillPath(p);
    };
    sparkle(624,306,4);sparkle(653,322,3);
    g.fillEllipse(615,317,2,2);

    // Direction marks and the upper transport waveform.
    g.setColour (inkColour);
    for (float x : { 765.0f, 773.0f }) stroke (g, { { x + 4, 171 }, { x, 176 }, { x + 4, 181 } });
    for (float x : { 788.0f, 796.0f }) stroke (g, { { x, 171 }, { x + 4, 176 }, { x, 181 } });
    g.drawLine (783, 168, 783, 184, 2);
    // Microphone, cable and mascot at the right edge.
    g.setColour (ink::gold);
    g.fillEllipse (1024, 163, 43, 43);
    g.setColour (juce::Colour (0xff242823));
    for (int i = 0; i < 6; ++i)
    {
        const float x = 1033.0f + float (i) * 5;
        const float inset = (i == 0 || i == 5) ? 5.0f : 0.0f;
        g.drawLine (x, 171 + inset, x, 198 - inset, 2.4f);
    }
    g.setColour (inkColour);
    stroke (g, { { 1046, 208 }, { 1046, 226 }, { 1058, 233 }, { 1058, 253 } });
    g.drawEllipse (1049, 250, 18, 23, 2);
    g.fillRoundedRectangle (1055, 254, 5, 11, 2);
    g.drawLine (1058, 265, 1058, 269, 1);
    stroke (g, { { 1158, 269 }, { 1158, 294 }, { 1151, 294 }, { 1151, 300 }, { 1171, 300 },
                 { 1171, 307 }, { 1153, 307 }, { 1153, 314 }, { 1162, 314 }, { 1162, 342 },
                 { 1157, 348 }, { 1140, 348 }, { 1133, 342 } });
    g.fillEllipse (1150, 250, 20, 22);
    g.setColour (inverseInk);
    stroke (g, { { 1162, 253 }, { 1157, 261 }, { 1162, 261 }, { 1157, 268 } }, 2);
    NibbiLookAndFeel::mascot(g,{1069,318,70,63},inkColour,true);
    // Large body branding, under the physical controls like the supplied artwork.
    drawWordmark(g,{16,140,400,54.4f},inkColour);

 }

void NibbiPanel::drawLiveLights(juce::Graphics& g) {
    NibbiLookAndFeel::led(g,{817,176},13,lampColours[6],lampColours[6].getBrightness()>0);
    NibbiLookAndFeel::led(g,{890,173},13,lampColours[7],lampColours[7].getBrightness()>0);
    NibbiLookAndFeel::led(g,{963,173},13,lampColours[8],lampColours[8].getBrightness()>0);

}

void NibbiPanel::drawKeyNumbers(juce::Graphics& g) {
    const auto inkColour=(backgroundIndex==1 || backgroundIndex==4)?juce::Colours::black:ink::paper;
    // Key numbers are permanent panel markings, independent of the legends overlay.
    for (int i = 0; i < 14; ++i)
        text (g, juce::String (i + 1), { float (51 + i * 73), 482, 73, 24 }, 17, inkColour);
    text (g, "15", { 1073, 482, 73, 24 }, 17, inkColour);
}
}
