#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace nibbi::gui {
// Vector lettering based on the rounded uppercase logo in nibbi_bg_1.png.
// The smaller byline uses the same chunky, rounded hand-lettered construction.
inline juce::Path bylineLetter(juce::juce_wchar c) {
 juce::Path p;
 switch(c) {
  case 'B':
   p.startNewSubPath(10,56);p.lineTo(10,12);p.lineTo(27,12);
   p.cubicTo(47,12,47,33,25,33);p.lineTo(10,33);
   p.startNewSubPath(25,33);p.cubicTo(50,33,48,56,26,56);p.lineTo(10,56);break;
  case 'Y':
   p.startNewSubPath(8,12);p.lineTo(25,33);p.lineTo(42,12);
   p.startNewSubPath(25,33);p.lineTo(24,56);break;
  case 'T':
   p.startNewSubPath(7,12);p.lineTo(43,12);p.startNewSubPath(25,12);p.lineTo(24,56);break;
  case 'E':
   p.startNewSubPath(40,12);p.lineTo(10,12);p.lineTo(10,56);p.lineTo(41,56);
   p.startNewSubPath(11,33);p.lineTo(35,33);break;
  case 'M':
   p.startNewSubPath(8,56);p.lineTo(9,12);p.lineTo(26,35);p.lineTo(43,12);p.lineTo(44,56);break;
  case 'C':
   p.startNewSubPath(41,16);p.cubicTo(2,-3,0,71,42,52);break;
  case 'U':
   p.startNewSubPath(9,12);p.lineTo(9,40);p.cubicTo(9,65,43,65,43,40);p.lineTo(43,12);break;
  case 'L':
   p.startNewSubPath(10,12);p.lineTo(10,56);p.lineTo(40,56);break;
  case 'A':
   p.startNewSubPath(7,56);p.lineTo(25,12);p.lineTo(44,56);
   p.startNewSubPath(14,41);p.lineTo(37,41);break;
  case 'D':
   p.startNewSubPath(10,12);p.lineTo(10,56);p.cubicTo(57,63,57,5,10,12);break;
  case 'S':
   p.startNewSubPath(40,15);p.cubicTo(5,0,0,31,25,33);
   p.cubicTo(54,35,48,68,9,53);break;
  case 'P':
   p.startNewSubPath(10,56);p.lineTo(10,12);p.lineTo(26,12);
   p.cubicTo(51,12,51,36,26,36);p.lineTo(10,36);break;
 }
 juce::Path result;
 juce::PathStrokeType(16.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded)
  .createStrokedPath(result,p);
 return result;
}
inline void drawWordmark(juce::Graphics& g,juce::Rectangle<float> bounds,juce::Colour colour) {
 juce::Graphics::ScopedSaveState save(g);
 const float scale=juce::jmin(bounds.getWidth()/500.f,bounds.getHeight()/68.f);
 g.addTransform(juce::AffineTransform::scale(scale).translated(
  bounds.getX(),bounds.getY()));
 g.setColour(colour);
 juce::Path stems;
 stems.startNewSubPath(16,56);stems.lineTo(15,17);stems.quadraticTo(15,13,18,17);
 stems.lineTo(48,52);stems.lineTo(47,12);
 stems.startNewSubPath(77,12);stems.lineTo(76,57);
 stems.startNewSubPath(223,12);stems.lineTo(223,56);
 g.strokePath(stems,juce::PathStrokeType(21.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
 for(float x:{99.f,155.f}) {
  juce::Path b;b.setUsingNonZeroWinding(false);
  b.startNewSubPath(x+11,2);b.cubicTo(x+3,2,x,6,x,15);
  b.lineTo(x,56);b.cubicTo(x,65,x+5,68,x+15,68);
  b.lineTo(x+26,68);b.cubicTo(x+51,68,x+53,42,x+38,35);
  b.cubicTo(x+51,28,x+48,3,x+25,2);b.closeSubPath();
  b.addRoundedRectangle(x+19,17,7,9,3);
  b.addRoundedRectangle(x+19,43,9,10,3.5f);
  g.fillPath(b);
 }
 static const auto byline=[] {
  juce::Path path;float x=0;
  for(const auto c:juce::String("BY TEMECULA DSP")) {
   if(c==' ') {x+=24;continue;}
   path.addPath(bylineLetter(c),juce::AffineTransform::translation(x,0));x+=59;
  }
  const auto ink=path.getBounds();
  const float size=juce::jmin(270.f/ink.getWidth(),26.f/ink.getHeight());
  path.applyTransform(juce::AffineTransform::scale(size).translated(250.f-ink.getX()*size,1.5f-ink.getY()*size));
  return path;
 }();
 g.fillPath(byline);
}
}
