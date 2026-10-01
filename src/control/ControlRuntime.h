#pragma once
#include "NormalPage.h"
#include "MenuPage.h"
namespace nibbi::control {
class ControlRuntime {
public:
 explicit ControlRuntime(ControlEngine& engine):engine_(engine) {
  for(size_t row=0;row<3;++row)for(size_t col=0;col<6;++col)values_[row][col]=defaults_[row][col];
  normal.attachLights(lights);menu.attachLights(lights);
  normal.Init(&surface,&engine_,&copies,rows_,defaultRows_,pages.data(),&presets);
  menu.Init(&surface,&engine_,&copies,rows_,defaultRows_,pages.data(),&presets);
 }
 void sync(const Controls& c) {
  values_[0][0]=speedPosition(c.sampleSpeed);values_[0][1]=c.start;values_[0][2]=c.end;
  values_[1][0]=std::sqrt(std::max(0.f,(c.gain-.01f)/2.f));
  values_[1][1]=std::cbrt(std::max(0.f,(c.attack-.201f)/20.f));
  values_[1][2]=std::cbrt(std::max(0.f,(c.release-.041f)/4.f));
  values_[0][3]=c.space;values_[1][3]=c.drive;values_[2][3]=c.filter;
  values_[0][4]=(c.loopSpeed+2.f)*.25f;values_[0][5]=c.output;values_[1][5]=c.input;
  normal.Configure(false,c.split,channel);menu.Configure(true,c.split);
  normal.SetSwitchState(c.modeSwitch);
  menu.Sync(c.time,c.warble,c.resonance,c.compression);
  current_=c;
 }
 void setShift(bool shift) {
  if(shift&&!heldShift_&&!menuActive_) {menu.OnFocusGained();menuActive_=true;}
  if(!shift&&heldShift_&&menuActive_) menu.OnButton(5,0,false);
  heldShift_=shift;
  menu.SetSwitchState(current_.modeSwitch||shift||menu.PendingAction()>=0);
  if(!shift&&menuActive_&&menu.IsClosable())menuActive_=false;
 }
 void button(int id,bool down,bool shift) {
  setShift(shift);
  // Desktop shortcut: Shift + tape Record arms/cancels an empty tape.
  // Consume both edges even if Shift is released before the Record button.
  if(id==34 && !down && armShortcutHeld_) {armShortcutHeld_=false;return;}
  if(id==34 && down && shift && menu.PendingAction()<0
      && (engine_.GetLooperRecordArm() || engine_.DesktopCanArmLoop())) {
   armShortcutHeld_=true;
   if(engine_.GetLooperRecordArm()) engine_.CancelDesktopLoopArm();
   else {
    engine_.LooperPlayButton(true);engine_.LooperRecordButton(true);
    engine_.LooperRecordButton(false);engine_.LooperPlayButton(false);
   }
   return;
  }
  if(id==5 && menuActive_) {menu.OnButton(uint16_t(id),down?1:0,false);return;}
  if(id==5 && !current_.modeSwitch && !shift) {normal.OnButton(uint16_t(id),down?1:0,false);return;}
  if(!(menuActive_&&menu.OnButton(uint16_t(id),down?1:0,false))) normal.OnButton(uint16_t(id),down?1:0,false);
 }
 void encoder(int id,int turns,bool shift,bool midi=false) {
  if(id<0||id>=6)return;
  if(!midi)setShift(shift);
  if(midi && (menuActive_||(id==4&&!engine_.IsLooperPlaying()))) return;
  if(!(menuActive_&&menu.OnEncoderTurned(uint16_t(id),int16_t(turns),midi?1:0)))
   normal.OnEncoderTurned(uint16_t(id),int16_t(turns),midi?1:0);
  normal.ApplyParameters();
 }
 bool tick(uint32_t now) {
  surface.now=now;
  bool changed=false;
  if(menuActive_&&!heldShift_&&menu.IsClosable()) {menuActive_=false;changed=true;}
  normal.ApplyParameters();
  return changed;
 }
 void drawLights() {
  if(menuActive_) {
   menu.DrawLights();
   // The new shortcut must keep its armed indication visible while Shift is held.
   if(engine_.GetLooperRecordArm())normal.DrawTransportLights();
  } else normal.DrawLights();
 }
 LedFrame lights;
 void stopRecording() {normal.StopVoiceRecording();}
 Controls state() const {
  auto c=current_;
  c.sampleSpeed=engine_.GetGlobalPitch()*(engine_.GetReverse()?-1.f:1.f);
  c.start=values_[0][1];c.end=values_[0][2];
  c.gain=2.f*values_[1][0]*values_[1][0]+.01f;
  c.attack=.201f+20.f*values_[1][1]*values_[1][1]*values_[1][1];
  c.release=.041f+4.f*values_[1][2]*values_[1][2]*values_[1][2];
  c.pan=engine_.GetPan();c.autoLoop=engine_.GetAutoLoop();c.sustain=engine_.GetSustainActive();
  c.space=values_[0][3];c.drive=values_[1][3];c.filter=values_[2][3];
  c.output=values_[0][5];c.input=values_[1][5];c.loopSpeed=engine_.GetLooperPitch();
  c.overdub=engine_.GetLooperDubGain();c.compression=engine_.GetFinalComp();
  c.time=menu.Time();c.warble=menu.Warble();c.resonance=menu.Resonance();
  c.multi=engine_.GetVoiceMode()==VoiceMode::CUBBI;c.bank=engine_.GetBank();
  c.slot=int(engine_.GetVoiceSlot())-1;
  const auto source=engine_.GetInputSource();c.inputSource=source==InputSource::MIC?1:source==InputSource::RESAMPLE?2:0;
  c.routing=int(engine_.GetMonitorMode());c.fxPre=engine_.DesktopFxPreTarget();
  c.jammiBank=engine_.DesktopBank(false);c.cubbiBank=engine_.DesktopBank(true);c.jammiSlot=engine_.DesktopJammiSlot();
  return c;
 }
 void openCubbi(size_t slot) {normal.OpenCubbiSlot(slot);}
 bool menuActive() const {return menuActive_;}
 int pending() const {return menu.PendingAction();}
 static float speedPosition(float speed) {
  float p=std::abs(speed);
  if(p<.5f)p=(p-.01f)*.673469f;else if(p<1.f)p=(p-.5f)*.66f+.33f;else p=(p-1.f)*.34f+.66f;
  return speed<0?(1-p)*.5f:(1+p)*.5f;
 }
 ControlSurface surface;CopyService copies;PresetStore presets;
 std::array<uint8_t,6> pages{};int channel=0;
private:
 ControlEngine& engine_;NormalPage normal;MenuPage menu;Controls current_{};
 bool armShortcutHeld_=false;
 bool heldShift_=false,menuActive_=false;
 float values_[3][6]{};
 inline static constexpr float defaults_[3][6]={{.83f,0,1,0,.75f,.5f},{.704f,0,0,0,0,.75f},{0,0,0,.5f,0,0}};
 float* rows_[3]={values_[0],values_[1],values_[2]};
 const float* defaultRows_[3]={defaults_[0],defaults_[1],defaults_[2]};
};
}
