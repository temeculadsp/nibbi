#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace nibbi::gui {
class ToolbarButton final : public juce::TextButton {
public:
 enum Icon { skin, legends, reset, arm, erase, help };
 ToolbarButton(const juce::String& name,Icon icon):juce::TextButton(name),icon_(icon) {setName(name);setWantsKeyboardFocus(false);}
 void paintButton(juce::Graphics& g,bool over,bool down) override {
  const auto bounds=getLocalBounds().toFloat().reduced(1);
  if(over || down || getToggleState()) {
   g.setColour(getToggleState()?juce::Colour(0xff404c60):juce::Colour(down?0xff414b59:0xff303942));
   g.fillRoundedRectangle(bounds,5);
  }
  juce::Graphics::ScopedSaveState save(g);
  const float scale=juce::jmin(float(getWidth()),float(getHeight()))/24.f;
  g.addTransform(juce::AffineTransform::scale(scale).translated((getWidth()-24*scale)*.5f,(getHeight()-24*scale)*.5f));
  g.setColour((getToggleState() && icon_==arm?juce::Colour(0xffedaa83):juce::Colour(0xffd2e1ed))
               .withMultipliedAlpha(isEnabled()?1.f:.32f));
  juce::Path p;
  switch(icon_) {
   case help:
    g.drawEllipse(3,3,18,18,1.6f);
    p.startNewSubPath(9,9);p.cubicTo(9,5.5f,16,5.5f,15,10);
    p.cubicTo(14.5f,12,12,11.5f,12,14);draw(g,p);
    g.fillEllipse(11,16,2,2);break;
   case skin:
    p.startNewSubPath(12,4);p.cubicTo(2,3,1,18,10,20);p.cubicTo(15,22,12,15,16,15);
    p.cubicTo(23,16,22,5,12,4);p.closeSubPath();draw(g,p);
    for(auto c:{juce::Point<float>(7,10),{10,7},{15,7},{18,10}})g.fillEllipse(c.x-1,c.y-1,2,2);
    break;
   case legends:
    p.startNewSubPath(3,3);p.lineTo(12,3);p.lineTo(20,11);
    p.quadraticTo(21,12,20,13);p.lineTo(13,20);p.quadraticTo(12,21,11,20);
    p.lineTo(3,12);p.closeSubPath();draw(g,p);
    g.drawEllipse(6,6,3,3,1.6f);break;
   case reset:
    p.startNewSubPath(3,11);p.cubicTo(3,6,7,3,12,3);p.cubicTo(17,3,21,7,21,12);
    p.cubicTo(21,17,17,21,12,21);p.cubicTo(8,21,5,19,4,16);
    p.startNewSubPath(3,5);p.lineTo(3,11);p.lineTo(9,11);draw(g,p);break;
   case arm:
    for(auto c:{juce::Point<float>(4,4),{20,4},{4,20},{20,20}}) {
     const float dx=c.x<12?4.f:-4.f,dy=c.y<12?4.f:-4.f;
     p.startNewSubPath(c.x+dx,c.y);p.lineTo(c);p.lineTo(c.x,c.y+dy);
    }
    draw(g,p);g.drawEllipse(8,8,8,8,1.7f);
    if(getToggleState())g.fillEllipse(10,10,4,4);
    break;
   case erase:
    p.startNewSubPath(4,13);p.lineTo(13,4);p.lineTo(20,11);p.lineTo(12,19);p.lineTo(9,19);p.closeSubPath();
    p.startNewSubPath(8,9);p.lineTo(15,16);p.startNewSubPath(12,20);p.lineTo(21,20);draw(g,p);break;
  }
 }
private:
 static void draw(juce::Graphics& g,const juce::Path& p) {
  g.strokePath(p,juce::PathStrokeType(1.6f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
 }
 Icon icon_;
};

class TapeStatus final : public juce::Component,public juce::SettableTooltipClient {
public:
 TapeStatus() {setComponentID("tapeStatus");setName("Tape status");}
 void update(bool armed,bool recording,bool playing,int frames) {
  const juce::String state=armed?"ARMED":recording?"REC":playing?"PLAYING":"PAUSED";
  const int ticks=juce::jmax(0,frames)/4800;
  const auto duration=juce::String::formatted("%02d:%02d.%d",ticks/600,(ticks/10)%60,ticks%10);
  const auto colour=recording?juce::Colour(0xfff18c9c):armed?juce::Colour(0xffecd38e):
                     playing?juce::Colour(0xff9cd5b6):juce::Colour(0xff92969f);
  if(state==state_ && duration==duration_ && colour==colour_)return;
  state_=state;duration_=duration;colour_=colour;
  setTooltip(armed?"Tape armed: play a note to start recording":"Tape "+state.toLowerCase()+". Loop length "+duration);
  repaint();
 }
 void paint(juce::Graphics& g) override {
  const float cy=getHeight()*.5f;
  g.setColour(colour_);g.fillEllipse(0,cy-2.5f,5,5);
  g.setFont(juce::FontOptions(10.5f));g.setColour(juce::Colour(0xffb9bdc4));
  g.drawText(state_,13,0,63,getHeight(),juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xff51555b));g.drawLine(78,cy-5,78,cy+5,1);
  g.setColour(juce::Colour(0xffe4e2da));
  g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),13.f,juce::Font::plain));
  g.drawText(duration_,91,0,getWidth()-91,getHeight(),juce::Justification::centredLeft);
 }
private:
 juce::String state_,duration_;
 juce::Colour colour_;
};

class TitleBar final : public juce::Component {
public:
 static constexpr int height=36;
 TitleBar() {setName("titleBar");setComponentID("titleBar");}
 void paint(juce::Graphics& g) override {
  g.setGradientFill({juce::Colour(0xff202329),0,0,juce::Colour(0xff111315),0,float(getHeight()),false});g.fillAll();
  g.setColour(juce::Colours::black);g.drawHorizontalLine(getHeight()-1,0,float(getWidth()));
 }
};
}
