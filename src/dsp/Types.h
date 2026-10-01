#pragma once
#include "../engine/RamBuffer.h"
#include <array>
#include <memory>
#include <string>
#include <vector>
namespace nibbi {
// The host bus order differs from the four-channel hardware callback.
struct Frame { float l=0,r=0,hpL=0,hpR=0,auxL=0,auxR=0; };
struct Sample {
 std::vector<int16_t> pcm;
 std::vector<int16_t> doublePcm;
 int factoryId=-1;
 double sampleRate=48000;
 int root=60;
 bool live=false;
 size_t frames() const { return pcm.size()/2; }
};
struct LoopStorage {
 std::vector<daisy::AtomicSample> pcm;
 daisy::RamBufferMemory memory{};
 bool resume=false;
 LoopStorage():pcm(daisy::kMaxRamBuffSize) { memory.Init(pcm.data()); }
};
inline constexpr size_t kCatalogSize=150;
inline int catalogIndex(bool multi,int bank,int slot) { return slot==14?14:(multi?75:0)+bank*15+slot; }
struct PresetState {
 std::array<float,9> controls{{.83f,0.f,1.f,0.f,0.f,1.f,1.f,.704f,.5f}};
 bool valid=false;
};
struct Catalog {
 std::shared_ptr<LoopStorage> tape;
 std::array<std::shared_ptr<const Sample>,kCatalogSize> samples;
 std::array<std::string,kCatalogSize> names;
 std::array<PresetState,kCatalogSize> presets;
};
struct Capture {
 std::shared_ptr<Sample> sample=std::make_shared<Sample>();
 std::atomic<int> status{0};
 std::atomic<size_t> length{0};
 size_t capacity=0;
 int slot=14;
 bool startImmediately=true;
 explicit Capture(size_t maxSamples=daisy::kMaxRamBuffSize):capacity(maxSamples) { sample->pcm.resize(capacity); }
};
struct Controls {
 float sampleSpeed=1,start=0,end=1,attack=.201f,release=.041f,gain=2.f*.704f*.704f+.01f,pan=.5f;
 float filter=.5f,resonance=0,drive=0,warble=0,space=0,time=.5f;
 float loopSpeed=1,overdub=1,output=1,input=.75f,compression=0;
 int bank=0,slot=0,inputSource=0,routing=0;
 int jammiBank=0,cubbiBank=0,jammiSlot=0;
 bool modeSwitch=false,recordLatch=false;
 bool tapeSlew=true,multi=false,autoLoop=true,sustain=true,split=false,monitor=false,fxPre=true;
};

}
