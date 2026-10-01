#pragma once
#include "TapeEngine.h"
#include <array>
namespace nibbi {
// Causal, windowed-sinc sample-rate conversion. No callback-sized waiting FIFO.
class RateFilter {
public:
 static constexpr int taps=64, phases=512;
 void prepare(double inputRate,double outputRate) {
  history_.fill({}); count_=0;
  double cutoff=.90*std::min(1.,outputRate/inputRate);
  for(int p=0;p<=phases;++p) {
   double sum=0;
   for(int k=0;k<taps;++k) {
    double x=double(k)+double(p)/phases-32.;
    double w=std::abs(x)<32 ? .42+.5*std::cos(3.141592653589793*x/32)+.08*std::cos(2*3.141592653589793*x/32):0;
    double v=std::abs(x)<1.e-12?cutoff:std::sin(3.141592653589793*cutoff*x)/(3.141592653589793*x);
    table_[size_t(p)][size_t(k)]=float(v*w); sum+=v*w;
   }
   for(auto& v:table_[size_t(p)]) v/=float(sum);
  }
 }
 void push(Frame f) { history_[size_t(count_++)%128]=f; }
 Frame at(double time) const {
  auto whole=int64_t(std::floor(time));
  double phase=(time-double(whole))*phases;
  int p=std::clamp(int(phase),0,phases-1); float fraction=float(phase-p);
  Frame sum;
  for(int k=0;k<taps;++k) {
   auto index=whole-k;
   if(index<0 || index>=count_ || count_-index>128) continue;
   float c=table_[size_t(p)][size_t(k)]+fraction*(table_[size_t(p+1)][size_t(k)]-table_[size_t(p)][size_t(k)]);
   const auto f=history_[size_t(index)%128]; sum.l+=c*f.l; sum.r+=c*f.r;
   sum.hpL+=c*f.hpL; sum.hpR+=c*f.hpR; sum.auxL+=c*f.auxL; sum.auxR+=c*f.auxR;
  }
  return sum;
 }
private:
 std::array<Frame,128> history_{};
 int64_t count_=0;
 std::array<std::array<float,taps>,phases+1> table_{};
};
struct Event { uint8_t type=0,note=0,channel=1; float value=0; int64_t time=0; Capture* capture=nullptr; };
class HostAdapter {
public:
 TapeEngine engine;
 void prepare(double rate) {
  rate_=rate; direct_=std::abs(rate-48000)<.01; hostFrame_=nativeFrame_=0; read_=write_=0;
  input_.prepare(rate,48000); output_.prepare(48000,rate);
  const double conversion=direct_?0.:32.+32.*rate/48000.;
  latency_=int(std::ceil(conversion+24.*rate/48000.));
  extra_=double(latency_)-(conversion+24.*rate/48000.);
  position_=0; inputBlock_.fill({});outputBlock_.fill({});
 }
 int latency() const { return latency_; }
 void setControls(const Controls& controls) { pendingControls_=controls; }
 bool takeControlState(Controls& state) {if(!controlChanged_)return false;state=pendingControls_;controlChanged_=false;return true;}
 void event(Event e) {
  e.time=hostFrame_+(direct_?0:32);
  auto next=(write_+1)%events_.size();
  if(next==read_) { engine.allNotesOff(); read_=write_=0; return; }
  events_[write_]=e; write_=next;
 }
 Frame process(Frame in,const Catalog& catalog) {
  if(!engine.hasCatalog()) engine.installCatalog(&catalog);
  if(direct_) { auto out=processNative(in,double(hostFrame_),catalog); ++hostFrame_; return out; }
  input_.push(in);
  while(double(nativeFrame_)*rate_/48000.<=double(hostFrame_)+1.e-9) {
   double t=double(nativeFrame_)*rate_/48000.;
   output_.push(processNative(input_.at(t),t,catalog)); ++nativeFrame_;
  }
  auto f=output_.at((double(hostFrame_)-extra_)*48000./rate_); ++hostFrame_; return f;
 }
private:
 Frame processNative(Frame input,double time,const Catalog& catalog) {
  if(position_==0) { blockStart_=time; blockControls_=pendingControls_; }
  const auto output=outputBlock_[position_];
  inputBlock_[position_++]=input;
  if(position_==24) {
   engine.setControls(blockControls_);
   dispatch(blockStart_,catalog);
   engine.processBlock(inputBlock_.data(),outputBlock_.data());
   Controls updated;
   if(engine.takeControlState(updated)) {pendingControls_=updated;controlChanged_=true;}
   position_=0;
  }
  return output;
 }
 void dispatch(double time,const Catalog& catalog) {
  while(read_!=write_ && double(events_[read_].time)<=time+1.e-9) {
   auto e=events_[read_]; read_=(read_+1)%events_.size();
   if(e.type==1) engine.noteOn(e.note,e.value,e.channel,catalog);
   if(e.type==2) engine.noteOff(e.note,e.channel);
   if(e.type==4) engine.allNotesOff();
   if(e.type==6) engine.loopRecordButton(e.value>.5f);
   if(e.type==7) engine.loopPlayButton(e.value>.5f);
   if(e.type==8) engine.scrub(e.value);
   if(e.type==10) engine.loopRecord();
   if(e.type==11) engine.loopPlay();
   if(e.type==12) engine.toggleLoopArm();
   if(e.type==13) engine.clearLoop();
   if(e.type==14) engine.startCapture(e.capture);
   if(e.type==15) engine.stopCapture();
   if(e.type==16) engine.reserveCapture(e.capture);
   if(e.type>=19&&e.type<=22) engine.controlEvent(e.type,e.note,e.value,e.channel!=0);
  }
 }
 std::array<Frame,24> inputBlock_{},outputBlock_{};
 Controls pendingControls_{},blockControls_{};
 size_t position_=0; double blockStart_=0;bool controlChanged_=false;
 RateFilter input_,output_;
 std::array<Event,4096> events_{};
 size_t read_=0,write_=0;
 double rate_=48000,extra_=0;
 int latency_=0;
 bool direct_=true;
 int64_t hostFrame_=0,nativeFrame_=0;
};
}
