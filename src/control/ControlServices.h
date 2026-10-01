#pragma once
#include "ControlMaps.h"
#include "../engine/DSPEngine.h"
#include "../dsp/Types.h"
#include <atomic>
namespace nibbi::control {
template<class T,size_t Size> class Queue {
public:
 bool PushBack(const T& item) {
  const auto w=write_.load(std::memory_order_relaxed),next=(w+1)%Size;
  if(next==read_.load(std::memory_order_acquire)) return false;
  data_[w]=item;write_.store(next,std::memory_order_release);return true;
 }
 bool pop(T& item) {
  const auto r=read_.load(std::memory_order_relaxed);
  if(r==write_.load(std::memory_order_acquire))return false;
  item=data_[r];read_.store((r+1)%Size,std::memory_order_release);return true;
 }
 bool IsEmpty() const {return read_.load()==write_.load();}
private:
 std::array<T,Size> data_{};std::atomic<size_t> read_{0},write_{0};
};
struct ControlSurface : ControlIds {
 struct Midi {int type=0,channel=0,note=0,value=0;};
 uint32_t now=0;Queue<Midi,256> midi;
 uint32_t Now() const {return now;}
 void SendCC(int ch,int cc,int value) {if(cc)midi.PushBack({0,ch,cc,value});}
 void SendNoteOn(int ch,int note,int value) {midi.PushBack({1,ch,note,value});}
 void SendNoteOff(int ch,int note,int value) {midi.PushBack({2,ch,note,value});}
};
struct CopyService {
 struct CopyRequest {
  enum RamDir {NONE,FROM,TO};
  size_t src=0,src_bank=0;VoiceMode src_mode=VoiceMode::JAMMI;
  size_t dest=0,dest_bank=0;VoiceMode dest_mode=VoiceMode::JAMMI;
  bool set=false;RamDir is_nibbi=NONE,is_looper=NONE;
  CopyRequest()=default;
  CopyRequest(size_t s,size_t b,VoiceMode m,size_t d,size_t db,VoiceMode dm,bool st,RamDir c,RamDir l)
   :src(s),src_bank(b),src_mode(m),dest(d),dest_bank(db),dest_mode(dm),set(st),is_nibbi(c),is_looper(l){}
 };
 Queue<CopyRequest,32> req_fifo;
 std::atomic<bool> busy{false};
 bool IsCopying() const {return busy.load() || !req_fifo.IsEmpty();}
};
class PresetStore {
public:
 PresetStore() {for(size_t i=0;i<kCatalogSize;++i) {valid_[i]=false;for(size_t j=0;j<9;++j)values_[i][j]=PresetState{}.controls[j];}}
 bool consumeDirty(size_t i) {return dirty_[i].exchange(false);}
 static int index(int m,int b,int s) {return s>=1&&s<=15&&b>=0&&b<5&&m>=0&&m<2?catalogIndex(m!=0,b,s-1):-1;}
 bool IsValid(int m,int b,int s) const {const int i=index(m,b,s);return i>=0&&valid_[size_t(i)].load();}
 float GetValue(int m,int b,int s,int field) const {const int i=index(m,b,s);return i>=0?values_[size_t(i)][size_t(field)].load():PresetState{}.controls[size_t(field)];}
 void SetValue(float v,int m,int b,int s,int field) {const int i=index(m,b,s);if(i>=0) {values_[size_t(i)][size_t(field)].store(v);valid_[size_t(i)]=true;dirty_[size_t(i)]=true;}}
 void Invalidate(int m,int b,int s) {const int i=index(m,b,s);if(i>=0) {valid_[size_t(i)]=false;dirty_[size_t(i)]=true;}}
 // Bounded native preset copies; the catalog service performs the PCM copy.
 void Save(int mode,int bank,int slot) {Copy(0,0,15,mode,bank,slot);}
 void Copy(int mode,int bank,int slot,int destMode,int destBank,int destSlot) {
  const int src=index(mode,bank,slot),dest=index(destMode,destBank,destSlot);
  if(src<0||dest<0)return;
  for(size_t j=0;j<9;++j)values_[size_t(dest)][j]=values_[size_t(src)][j].load();
  valid_[size_t(dest)]=valid_[size_t(src)].load();dirty_[size_t(dest)]=true;
 }
 void importCatalog(const Catalog& c) {
  for(size_t i=0;i<kCatalogSize;++i) {
   const bool newSample=!loaded_ || sampleIdentity_[i]!=c.samples[i].get();
   if(newSample||last_[i].valid!=c.presets[i].valid)valid_[i]=c.presets[i].valid;
   for(size_t j=0;j<9;++j)if(newSample||last_[i].controls[j]!=c.presets[i].controls[j])values_[i][j]=c.presets[i].controls[j];
   if(newSample)dirty_[i]=false;
   sampleIdentity_[i]=c.samples[i].get();last_[i]=c.presets[i];
  }loaded_=true;
 }
 PresetState snapshot(size_t i) const {PresetState p;p.valid=valid_[i].load();for(size_t j=0;j<9;++j)p.controls[j]=values_[i][j].load();return p;}
private:
 std::array<std::array<std::atomic<float>,9>,kCatalogSize> values_{};
 std::array<std::atomic<bool>,kCatalogSize> valid_{},dirty_{};
 std::array<const Sample*,kCatalogSize> sampleIdentity_{};
 std::array<PresetState,kCatalogSize> last_{};std::atomic<bool> loaded_{false};
};
class ControlEngine : public daisy::Engine {
public:
 struct Erase {int slot=0,bank=0,mode=0;};
 Queue<Erase,32> erases;std::atomic<bool> eraseBusy{false};
 void EraseStart(size_t slot,size_t bank,VoiceMode mode) {erases.PushBack({int(slot),int(bank),int(mode)});}
 bool IsErasing() {return eraseBusy.load()||!erases.IsEmpty();}
 // A catalog handoff attaches copied tape storage; no SD file needs opening.
 void LooperOpenFile() {}
};
}
