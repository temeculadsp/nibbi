#include "dsp/HostAdapter.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>
#include <atomic>
#include <cstring>
static std::atomic<bool> watch{false}; static std::atomic<int> allocations{0};
void* operator new(size_t n) {
 if(watch.load()) ++allocations;
 if(void* p=std::malloc(n?n:1)) {
  // Firmware globals start zeroed; a desktop heap allocation need not.
  if(n==sizeof(daisy::Engine)) std::memset(p,0xa5,n);
  return p;
 }
 throw std::bad_alloc();
}
void operator delete(void* p) noexcept { if(watch.load()) ++allocations; std::free(p); }
void* operator new[](size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main() { try {
 auto initial=std::make_unique<daisy::Engine>();
 require(initial->old_fx_outl==0.f && initial->old_fx_outr==0.f,"Heap engine retains firmware zero-initialized storage");
 initial.reset();
 nibbi::Catalog catalog; auto s=std::make_shared<nibbi::Sample>(); s->pcm.resize(48000*2);
 for(size_t i=0;i<s->frames();++i) { s->pcm[i*2]=int16_t(8000*std::sin(2*3.141592653589793*440*double(i)/48000)); s->pcm[i*2+1]=s->pcm[i*2]; }
 catalog.samples[0]=s;
 for(double rate:{44100.,48000.,96000.,192000.}) {
  auto host=std::make_unique<nibbi::HostAdapter>(); host->prepare(rate);
  require(host->latency()==int(std::ceil((rate==48000?0:32+32*rate/48000)+24*rate/48000)),"Actual converter delay reported");
  nibbi::Controls c; c.space=.5; c.warble=.3; host->setControls(c);
  host->event({1,60,1,.8}); double energy=0;
  watch=true;
  for(int i=0;i<int(rate*.6);++i) { if(i==int(rate*.2)) host->event({2,60}); auto x=host->process({},catalog); if(!std::isfinite(x.l)||!std::isfinite(x.r)) { watch=false; throw std::runtime_error("Non-finite audio"); } energy+=x.l*x.l+x.r*x.r; }
  watch=false;
  require(energy>1,"MIDI note produces audio"); require(allocations==0,"Processing has no allocations or deallocations");
 }
 // Original LED arithmetic follows the live native controls, including alternate pages.
 {
  auto lit=std::make_unique<nibbi::TapeEngine>();lit->installCatalog(&catalog);
  nibbi::Controls controls;controls.modeSwitch=true;controls.start=.25f;controls.end=.75f;
  controls.space=.25f;lit->setControls(controls);
  auto& panel=lit->controller();
  auto equal=[](const auto& rgb,float r,float g,float b) {
   return std::abs(rgb[0]-r)<.00001f&&std::abs(rgb[1]-g)<.00001f&&std::abs(rgb[2]-b)<.00001f;
  };
  panel.drawLights();
  require(equal(panel.lights.lamps[2],1,.8625f,.0975f),"Start LED interpolates yellow to orange");
  require(equal(panel.lights.lamps[3],1,.15f,.06f),"End LED interpolates orange to red");
  require(equal(panel.lights.lamps[4],.07f,.92f,.96f),"FX LED follows its current value");
  require(equal(panel.lights.lamps[7],0,0,0)&&equal(panel.lights.lamps[8],0,0,0),"Empty tape lamps are off");
  panel.pages[1]=1;panel.drawLights();
  require(equal(panel.lights.lamps[2],.116f,.01f,.2f),"Envelope page shows dim purple at zero attack");
  panel.setShift(true);panel.drawLights();
  panel.button(0,true,true);panel.drawLights();
  const float loopOn=lit->controllerTarget().GetAutoLoop();
  require(equal(panel.lights.lamps[2],loopOn,loopOn,loopOn),"Shift loop toggle lamp follows enabled state");
  panel.button(0,false,true);
  panel.pages[0]=1;panel.drawLights();
  require(equal(panel.lights.lamps[1],0,0,0),"Shift pan LED is dark at centre");
  controls.modeSwitch=false;lit->setControls(controls);panel.tick(2000);panel.setShift(false);panel.drawLights();
  require(equal(panel.lights.lamps[1],0,0,0)&&equal(panel.lights.lamps[4],0,0,0),"Sampling mode dims the first four knob lamps as in firmware");
 }
 // Fresh instances have independent random state and identical output.
 auto a=std::make_unique<nibbi::HostAdapter>(),b=std::make_unique<nibbi::HostAdapter>(); a->prepare(48000); b->prepare(48000);
 nibbi::Controls c; c.warble=1; c.space=.7; c.drive=.8; a->setControls(c); b->setControls(c);
 a->event({1,60,1,.8}); b->event({1,60,1,.8});
 for(int i=0;i<48000;++i) { auto x=a->process({},catalog),y=b->process({},catalog); require(x.l==y.l && x.r==y.r,"Instances do not share warble random state"); }
 // Line capture uses the line bus, firmware input gain and DC blockers.
 // Different microphone samples ensure a swapped bus cannot pass this check.
 auto recorder=std::make_unique<nibbi::HostAdapter>(); recorder->prepare(48000);
 c={}; c.monitor=true; recorder->setControls(c);
 auto capture=std::make_unique<nibbi::Capture>(); recorder->engine.startCapture(capture.get());
 for(int i=0;i<1008;++i) recorder->process({-.1f,.1f,0,0,.25f,-.125f},catalog);
 const auto* capturedStorage=capture->sample->pcm.data();
 watch=true;recorder->engine.stopCapture();watch=false;
 require(capturedStorage==capture->sample->pcm.data(),"Capture hands off the same preallocated storage");
 require(allocations==0,"Completing capture does not allocate or free");
 require(capture->status==2 && capture->length==1008,"Capture completion");
 daisysp::DcBlock left,right; left.Init(48000); right.Init(48000);
 for(int i=0;i<1008;++i) {
  require(capture->sample->pcm[2*i]==daisy::f2s16(left.Process(.25f*.75f*3.f)),"Capture left gain/filter and frame count");
  require(capture->sample->pcm[2*i+1]==daisy::f2s16(right.Process(-.125f*.75f*3.f)),"Capture right gain/filter and frame count");
 }
 // Original looper: first recording -> overdub -> playback, forward and reverse.
 auto loop=std::make_unique<nibbi::TapeEngine>(); c={}; c.monitor=true; c.routing=1; loop->setControls(c);
 // Allow the firmware's startup reset callback before the first user command.
 for(int i=0;i<24;++i)loop->process({}); loop->loopRecord();
 for(int i=0;i<24000;++i) loop->process({0,0,0,0,.2f*std::sin(float(i)*.06f),.15f*std::sin(float(i)*.07f)});
 loop->loopRecord(); for(int i=0;i<24;++i)loop->process({}); require(loop->loopRecording() && loop->loopPlaying(),"Record closes loop and enters overdub on the next callback");
 loop->loopRecord(); for(int i=0;i<24;++i)loop->process({}); require(!loop->loopRecording() && loop->loopPlaying(),"Next record leaves overdub"); require(loop->loopFrames()==24000,"Loop length");
 double loopEnergy=0; for(int i=0;i<30000;++i) { auto x=loop->process({}); loopEnergy+=x.l*x.l; } require(loopEnergy>1,"Recorded tape plays");
 c.loopSpeed=-.5f; loop->setControls(c); for(int i=0;i<24000;++i) { auto x=loop->process({}); require(std::isfinite(x.l),"Reverse varispeed finite"); }
 loop->clearLoop(); require(!loop->loopPlaying() && !loop->loopRecording(),"Clear stops tape transport");
 // Firmware erase brakes the tape and fades before resetting its RAM length.
 require(loop->loopFrames()==24000,"Clear preserves audio until the firmware fade completes");
 for(int i=0;i<96000 && loop->loopFrames();++i) {
  const auto x=loop->process({}); require(std::isfinite(x.l)&&std::isfinite(x.r),"Tape erase remains finite");
 }
 require(loop->loopFrames()==0,"Firmware tape erase completes within two seconds");
 // Native encoder turns begin from the current host setting, including reverse.
 auto controls=std::make_unique<nibbi::TapeEngine>();
 controls->installCatalog(&catalog);
 auto tape=std::make_unique<nibbi::LoopStorage>();tape->memory.length=4800;tape->resume=true;
 controls->installLoop(tape.get());
 for(float speed:{.5f,1.5f,-.5f}) {
  nibbi::Controls settings;settings.sampleSpeed=speed;settings.loopSpeed=speed;
  controls->setControls(settings);
  watch=true;controls->controlEvent(21,4,1,false);watch=false;
  nibbi::Controls result;require(controls->takeControlState(result),"Native wheel publishes its changed state");
  require(std::abs(result.loopSpeed-(speed+.012f))<1.e-6f,"Tape wheel is relative at forward and reverse speeds");
  controls->controlEvent(21,0,1,false);require(controls->takeControlState(result),"Sample encoder publishes state");
  require(result.sampleSpeed>speed && result.sampleSpeed<speed+.03f,"Sample encoder begins from the current sound");
 }
 require(allocations==0,"Native controls allocate no audio-thread memory");
 // Every normalized FX endpoint, including resonant filter and maximum drive.
 for(float filter:{0.f,.5f,1.f}) for(float resonance:{0.f,1.f}) { c.filter=filter;c.resonance=resonance;c.space=1;c.time=1;c.drive=1; loop->setControls(c); for(int i=0;i<24000;++i) { auto x=loop->process({0,0,0,0,.25f,-.25f}); require(std::isfinite(x.l)&&std::isfinite(x.r),"FX endpoints remain finite"); } }
 std::cout<<"DSP: sample-rate conversion, MIDI, instance isolation, capture, tape, endpoints and realtime checks passed\n";
 return 0;
 } catch(const std::exception& e) { watch=false; std::cerr<<e.what()<<'\n'; return 1; } }
