#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Smooth centreline artwork measured against the faceplate reference. Circular arcs and
// Bezier curves retain the printed design without tracing JPEG pixel edges.
namespace nibbi::gui::silkscreen {
inline void stroke(juce::Path& ink,const juce::Path& centreline,float width=3.5f) {
 juce::Path outline;
 juce::PathStrokeType(width,juce::PathStrokeType::curved,juce::PathStrokeType::rounded)
  .createStrokedPath(outline,centreline);
 ink.addPath(outline);
}
inline void arrow(juce::Path& ink,juce::Point<float> tip,juce::Point<float> direction) {
 const auto unit=direction/direction.getDistanceFromOrigin();
 const auto base=tip-unit*6.5f;
 const juce::Point<float> wing(-unit.y*3.8f,unit.x*3.8f);
 ink.addTriangle(tip,base+wing,base-wing);
}
inline void arc(juce::Path& ink,float x,float y,float r,float start,float end,
                bool firstArrow=false,bool lastArrow=false) {
 juce::Path line;line.addCentredArc(x,y,r,r,0,start,end,true);stroke(ink,line);
 const juce::Point<float> centre(x,y);
 const float direction=end>start?1.f:-1.f;
 if(firstArrow)arrow(ink,centre.getPointOnCircumference(r,start),
                         {-direction*std::cos(start),-direction*std::sin(start)});
 if(lastArrow)arrow(ink,centre.getPointOnCircumference(r,end),
                        {direction*std::cos(end),direction*std::sin(end)});
}
inline juce::Path mapped(juce::Path p,float tx,float ty,float sx=1.f) {
 p.applyTransform(juce::AffineTransform::scale(sx,1.f).translated(tx,ty));return p;
}
inline const juce::Path& pitch() {
 static const auto p=[] {juce::Path ink;arc(ink,348,486,40.5f,-2.42f,2.42f,true,true);
  return mapped(ink,-77.5f,-227.5f);}();return p;
}
inline const juce::Path& effect() {
 static const auto p=[] {juce::Path ink;arc(ink,711,486,40.5f,-2.42f,2.42f,true,true);
  return mapped(ink,-77.5f,-227.5f);}();return p;
}
inline const juce::Path& volume() {
 static const auto p=[] {juce::Path ink,line;line.addEllipse(1147,445,82,82);stroke(ink,line);
  return mapped(ink,-77.5f,-227.5f);}();return p;
}
inline const juce::Path& trim() {
 static const auto p=[] {
  juce::Path ink,frame;
  frame.startNewSubPath(429,486);frame.cubicTo(429,463.4f,447.4f,445,470,445);
  frame.lineTo(590,445);frame.cubicTo(612.6f,445,631,463.4f,631,486);
  frame.lineTo(631,537);frame.quadraticTo(631,549,619,549);
  frame.lineTo(441,549);frame.quadraticTo(429,549,429,537);frame.closeSubPath();
  stroke(ink,frame);
  arc(ink,470,486,41,-juce::MathConstants<float>::halfPi,juce::MathConstants<float>::halfPi,false,true);
  arc(ink,590,486,41,juce::MathConstants<float>::halfPi,-juce::MathConstants<float>::halfPi,false,true);
  juce::Path wave;
  wave.startNewSubPath(454,538);wave.lineTo(480,538);
  wave.cubicTo(484,538,483,526,490,526);wave.lineTo(499,526);
  wave.quadraticTo(504,526,504,521);wave.quadraticTo(504,517,508,517);
  wave.quadraticTo(512,517,512,521);wave.lineTo(512,529);
  wave.quadraticTo(512,533,516,533);wave.quadraticTo(520,533,520,529);
  wave.lineTo(520,512);wave.quadraticTo(520,508,524,508);
  wave.quadraticTo(528,508,528,512);wave.lineTo(528,533);
  wave.quadraticTo(528,537,532,537);wave.quadraticTo(536,537,536,533);
  wave.lineTo(536,515);wave.quadraticTo(536,511,540,511);
  wave.quadraticTo(544,511,544,515);wave.lineTo(544,526);
  wave.quadraticTo(544,530,548,530);wave.quadraticTo(552,530,552,526);
  wave.lineTo(552,524);wave.quadraticTo(552,520,556,520);
  wave.quadraticTo(560,520,560,524);wave.quadraticTo(560,528,564,528);
  wave.lineTo(569,528);wave.cubicTo(575,528,576,538,581,538);wave.lineTo(600,538);
  stroke(ink,wave,2.1f);
  juce::Path base;base.startNewSubPath(451,541);base.lineTo(604,541);stroke(ink,base,1.5f);
  for(float x:{452.f,603.f}) {
   juce::Path flag;flag.startNewSubPath(x,521);flag.lineTo(x,541);stroke(ink,flag,1.6f);
   ink.addEllipse(x-1.8f,518.2f,3.6f,3.6f);
   ink.addTriangle(x,523,x+(x<500?9.f:-9.f),526,x,529);
  }
  for(float x:{496.f,565.f})for(float y:{532.f,536.f,540.f})ink.addEllipse(x-.7f,y-.7f,1.4f,1.4f);
  return mapped(ink,390.5f-470.f*122.f/120.f,-227.5f,122.f/120.f);
 }();return p;
}
inline const juce::Path& wheel() {
 static const auto p=[] {juce::Path ink;arc(ink,858,488,78,-2.55f,-.58f,true,true);
  return mapped(ink,-80.5f,-228.5f);}();return p;
}
inline const juce::Path& transport() {
 static const auto p=[] {
  juce::Path ink,line;
  line.startNewSubPath(916,438);line.lineTo(977,438);line.lineTo(979,432);
  line.quadraticTo(979.5f,430,982,430);line.lineTo(1025,430);
  line.quadraticTo(1027.5f,430,1028,432);line.lineTo(1030,438);
  line.lineTo(1082,438);line.quadraticTo(1090,438,1090,446);
  line.lineTo(1090,532);line.quadraticTo(1090,540,1082,540);line.lineTo(916,540);
  stroke(ink,line);
  juce::Path handle;handle.startNewSubPath(987,436);handle.lineTo(1020,436);stroke(ink,handle,2);
  ink.addEllipse(981.5f,434.5f,3,3);ink.addEllipse(1023.5f,434.5f,3,3);
  return mapped(ink,-78,-228);
 }();return p;
}
inline const juce::Path& transportLamps() {
 static const auto p=[] {
  juce::Path ink;
  for(float x:{965.5f,1040.f}) {
   juce::Path ring;ring.addEllipse(x-20,384,40,40);stroke(ink,ring,3.2f);
   for(int i=0;i<4;++i) {
    const float a=float(i)*juce::MathConstants<float>::halfPi;
    const juce::Point<float> centre(x,404);
    juce::Path mark;mark.startNewSubPath(centre.getPointOnCircumference(15.5f,a));
    mark.lineTo(centre.getPointOnCircumference(20,a));stroke(ink,mark,2.6f);
   }
  }
  juce::Path wave;wave.startNewSubPath(986,404);wave.lineTo(988,404);
  wave.quadraticTo(990,404,990,401);wave.quadraticTo(990,398,992,398);
  wave.quadraticTo(994,398,994,401);wave.lineTo(994,408);
  wave.quadraticTo(994,411,996,411);wave.quadraticTo(998,411,998,408);
  wave.lineTo(998,398);wave.quadraticTo(998,395,1000,395);
  wave.quadraticTo(1002,395,1002,398);wave.lineTo(1002,411);
  wave.quadraticTo(1002,414,1004,414);wave.quadraticTo(1006,414,1006,411);
  wave.lineTo(1006,402);wave.quadraticTo(1006,399,1008,399);
  wave.quadraticTo(1010,399,1010,402);wave.lineTo(1010,407);
  wave.quadraticTo(1010,410,1012,410);wave.quadraticTo(1014,410,1014,407);
  wave.quadraticTo(1014,404,1017,404);wave.lineTo(1020,404);stroke(ink,wave,2);
  return mapped(ink,890.f-965.5f*73.f/74.5f,-231,73.f/74.5f);
 }();return p;
}
}

