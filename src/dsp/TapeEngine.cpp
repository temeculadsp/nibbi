#include "TapeEngine.h"

namespace nibbi {
namespace {
// ui.h::midi2key: MIDI 24..72 to the physical key identifiers used by
// DSPEngine::KeyToSlot and the firmware voice-stealing logic.
constexpr uint8_t midi2key[49]={
 44,45,46,47,48,49,50,51,52,53,54,55,
 32,33,34,35,36,37,38,39,40,41,42,43,
 15,7,8,12,9,10,13,11,14,16,21,17,18,22,19,23,
 20,24,29,25,30,26,31,27,28
};
}

TapeEngine::TapeEngine()
 : engine_(std::make_unique<control::ControlEngine>()),
   reverb_(std::make_unique<daisysp::Reverb>()),
   delay_(kMaxDelayTime),
   bufferPcm_(daisy::kMaxRamBuffSize),
   defaultLoop_(std::make_unique<LoopStorage>()),loop_(defaultLoop_.get()) {
 bufferMemory_.Init(bufferPcm_.data());
 snapshotLoop_.store(loop_);
 engine_->Init(48000.f,reverb_.get(),delay_.data(),&loop_->memory,&bufferMemory_,false,true,daisy::MonitorMode::HP);
 controller_=std::make_unique<control::ControlRuntime>(*engine_);
 setControls(controls_);
}

daisy::Engine::DesktopSampleView TapeEngine::lookup(const void* context,int mode,int bank,int slot) {
 const auto* catalog=static_cast<const Catalog*>(context);
 if(!catalog || mode<0 || mode>1 || bank<0 || bank>=5 || slot<0 || slot>=15) return {};
 const int index=catalogIndex(mode!=0,bank,slot);
 const auto* sample=catalog->samples[size_t(index)].get();
 if(!sample || sample->pcm.size()<4) return {};
 return {sample->pcm.data(),sample->pcm.size(),sample->doublePcm.empty()?nullptr:sample->doublePcm.data(),
         sample->doublePcm.size(),float(sample->sampleRate),sample->root};
}

int TapeEngine::keyForNote(int note) {
 return note>=24 && note<=72?int(midi2key[note-24]):-1;
}

void TapeEngine::installCatalog(const Catalog* catalog) {
 catalog_=catalog;
 controller_->presets.importCatalog(*catalog);
 engine_->SetDesktopSamples(catalog,&TapeEngine::lookup);
 const auto buffer=lookup(catalog,0,0,14);
 if(!capture_) engine_->SetDesktopBufferReadOnly(buffer.pcm,buffer.count);
 if(appliedMode_==0 && appliedSlot_>=0)
  engine_->SetVoiceSlot(size_t(appliedSlot_+1),true);
}

void TapeEngine::repointCatalog(const Catalog* catalog) {
 catalog_=catalog;
 controller_->presets.importCatalog(*catalog);
 engine_->SetDesktopSampleContext(catalog);
}

void TapeEngine::installLoop(LoopStorage* storage) {
 if(!storage || storage==loop_) return;
 loop_=storage;
 engine_->AttachDesktopLoop(&loop_->memory,storage->resume);
 snapshotLoop_.store(loop_,std::memory_order_release);
 snapshotEmpty_.store(engine_->DesktopLoopEmpty());
}

std::vector<int16_t> TapeEngine::copyLoop() const {
 if(snapshotEmpty_.load()) return {};
 auto* storage=snapshotLoop_.load(std::memory_order_acquire);
 const size_t count=std::min(storage->memory.length.load(),daisy::kMaxRamBuffSize);
 std::vector<int16_t> pcm(count);
 for(size_t i=0;i<count;++i) pcm[i]=storage->pcm[i];
 return pcm;
}

void TapeEngine::setControls(const Controls& c) {
 engine_->SetDesktopTime(uint32_t(processedFrames_/48));
 controller_->surface.now=uint32_t(processedFrames_/48);
 const Controls old=controls_;
 const bool first=appliedMode_<0;
 const bool selectedJammi=!c.multi && (first || appliedMode_!=0 || c.bank!=appliedBank_ || c.slot!=appliedSlot_);
 controls_=c;
 if(first || c.jammiBank!=old.jammiBank || c.cubbiBank!=old.cubbiBank || c.jammiSlot!=old.jammiSlot)
  engine_->SetDesktopBankMemory(c.jammiBank,c.cubbiBank,c.jammiSlot);
 const int mode=c.multi?1:0;
 if(mode!=appliedMode_) { engine_->SetVoiceMode(mode?daisy::VoiceMode::CUBBI:daisy::VoiceMode::JAMMI); appliedMode_=mode; appliedBank_=-1; appliedSlot_=-1; }
 if(c.bank!=appliedBank_) { engine_->SetBank(size_t(c.bank)); appliedBank_=c.bank; appliedSlot_=-1; }
 if(!c.multi && c.slot!=appliedSlot_) { engine_->SetVoiceSlot(size_t(c.slot+1),true); appliedSlot_=c.slot; }

 engine_->SetInputSource(c.inputSource==1?daisy::InputSource::MIC:
                         c.inputSource==2?daisy::InputSource::RESAMPLE:daisy::InputSource::LINE_IN);
 engine_->SetInputMonitor(c.monitor);
 engine_->SetDesktopMonitorMode(static_cast<daisy::MonitorMode>(std::clamp(c.routing,0,2)));
 engine_->SetInputGain(c.input); engine_->SetMainGain(c.output); engine_->SetFinalComp(c.compression);
 engine_->SetFxPreLooper(c.fxPre);
 engine_->SetFilter(c.filter); engine_->SetFilterResonance(c.resonance);
 engine_->SetSaturate(c.drive); engine_->SetWarble(c.warble);
 if(c.split) {
  engine_->SetReverb(std::max(0.f,(c.space-.5f)*2.f));
  engine_->SetDelayFeedback(std::max(0.f,(.5f-c.space)*2.f));
 } else { engine_->SetReverb(c.space); engine_->SetDelayFeedback(c.space); }
 engine_->SetDelayTime(c.time);

 if(selectedJammi || c.sampleSpeed!=old.sampleSpeed) {
  engine_->SetGlobalPitch(std::max(.01f,std::abs(c.sampleSpeed)));
  engine_->SetReverse(c.sampleSpeed<0.f);
 }
 if(selectedJammi || c.start!=old.start) engine_->SetStartPoint(c.start);
 if(selectedJammi || c.end!=old.end) engine_->SetEndPoint(c.end);
 const float attack=std::cbrt(std::max(0.f,(c.attack-.201f)/20.f));
 const float decay=std::cbrt(std::max(0.f,(c.release-.041f)/4.f));
 if(selectedJammi || c.attack!=old.attack) engine_->SetAttack(attack);
 if(selectedJammi || c.release!=old.release) engine_->SetDecay(decay);
 if(selectedJammi || c.autoLoop!=old.autoLoop) engine_->SetAutoLoop(c.autoLoop);
 if(selectedJammi || c.sustain!=old.sustain) engine_->SetSustainActive(c.sustain);
 if(selectedJammi || c.gain!=old.gain) engine_->SetGain(std::sqrt(std::max(0.f,(c.gain-.01f)/2.f)));
 if(selectedJammi || c.pan!=old.pan) engine_->SetPan(c.pan);
 engine_->SetLooperPitch(c.loopSpeed); engine_->SetDesktopTapeSlew(c.tapeSlew);
 engine_->SetDesktopDubGain(c.overdub);
 engine_->SetDesktopRecordLatch(c.recordLatch);
 controller_->sync(c);
}

void TapeEngine::noteOn(int note,float velocity,int channel,const Catalog& catalog) {
 (void)catalog;
 if(channel<1 || channel>16) return;
 const int key=keyForNote(note);
 if(key<0) return;
 if(controls_.multi) {
  const size_t oneBased=daisy::KeyToSlot(size_t(key));
  if(oneBased==daisy::kSlotNone || oneBased<1 || oneBased>15) return;
  controller_->openCubbi(oneBased);
  controlsChanged_=true;
 }
 engine_->request_fifo.PushBack(daisy::KeyRequest(daisy::KeyRequest::Type::START,
                                                   float(note-60),key,velocity*127.f));
 if(engine_->GetLooperRecordArm()) engine_->ToggleLooperRecord();
}

void TapeEngine::noteOff(int note,int channel) {
 if(channel<1 || channel>16) return;
 const int key=keyForNote(note);
 if(key>=0) engine_->request_fifo.PushBack(daisy::KeyRequest(daisy::KeyRequest::Type::STOP,
                                                               float(note-60),key,127.f));
}

void TapeEngine::controlEvent(int type,int id,float value,bool shifted) {
 if(type==22) {controller_->setShift(value>.5f);controlsChanged_=true;return;}
 if(type==20) controller_->button(id,value>.5f,shifted);
 else if(type==19) controller_->encoder(id,int(value),false,true);
 else if(type==21 && id>=0 && id<6) {
  turnRemainder_[size_t(id)]+=value;
  const int turns=int(turnRemainder_[size_t(id)]);
  if(!turns)return;
  turnRemainder_[size_t(id)]-=float(turns);
  controller_->encoder(id,turns,shifted);
 }
 controlsChanged_=true;
}
bool TapeEngine::takeControlState(Controls& state) {
 if(!controlsChanged_)return false;
 state=controller_->state();controls_=state;
 appliedMode_=state.multi?1:0;appliedBank_=state.bank;appliedSlot_=state.slot;
 controlsChanged_=false;return true;
}
void TapeEngine::allNotesOff() {
 engine_->request_fifo.Clear();
 engine_->ResetVoiceKeys();
}

void TapeEngine::reserveCapture(Capture* capture) {
 if(capture_) stopCapture();
 capture_=capture;
 engine_->SetDesktopCapture(capture_->sample->pcm.data(),capture_->capacity);
}
void TapeEngine::startCapture(Capture* capture) {
 reserveCapture(capture);
 engine_->StartNewRecording(0);
 capture_->status.store(1,std::memory_order_release);
}

void TapeEngine::finishCapture() {
 if(!capture_) return;
 const size_t count=std::min(engine_->DesktopBufferLength(),capture_->capacity);
 if(count>=4) engine_->SetDesktopBufferReadOnly(capture_->sample->pcm.data(),count);
 else {
  const auto previous=lookup(catalog_,0,0,14);
  engine_->SetDesktopBufferReadOnly(previous.pcm,previous.count);
 }
 capture_->length.store(count/2,std::memory_order_relaxed);
 capture_->status.store(2,std::memory_order_release);
 capture_=nullptr;
}

void TapeEngine::stopCapture() {
 if(!capture_) return;
 if(engine_->Recording()) controller_->stopRecording();
 controlsChanged_=true;
 finishCapture();
}

void TapeEngine::loopRecord() { engine_->ToggleLooperRecord(); }
void TapeEngine::loopPlay() { engine_->ToggleLooperPlaying(); }
void TapeEngine::toggleLoopArm() {
 if(loopArmed()) { engine_->CancelDesktopLoopArm(); return; }
 // Only supply the hardware chord when it can arm an empty tape. Sending
 // these edges on an existing loop would otherwise toggle its playback.
 if(!canArmLoop()) return;
 engine_->LooperPlayButton(true); engine_->LooperRecordButton(true);
 engine_->LooperRecordButton(false); engine_->LooperPlayButton(false);
}
void TapeEngine::clearLoop() { engine_->ResetDesktopLoop(); }

void TapeEngine::processBlock(const Frame* input,Frame* output) {
 engine_->SetDesktopTime(uint32_t(processedFrames_/48));
 controlsChanged_|=controller_->tick(uint32_t(processedFrames_/48));
 processedFrames_+=blockSize;
 engine_->DesktopServiceReads();
 engine_->Prepare();
 float inputs[4][blockSize]{},outputs[4][blockSize]{};
 for(size_t i=0;i<blockSize;++i) {
  inputs[0][i]=input[i].l; inputs[1][i]=input[i].r;
  inputs[2][i]=input[i].auxL; inputs[3][i]=input[i].auxR;
 }
 const float* in[4]={inputs[0],inputs[1],inputs[2],inputs[3]};
 float* out[4]={outputs[0],outputs[1],outputs[2],outputs[3]};
 engine_->Process(in,out,blockSize);
 // Display updates use the audio clock (roughly 60 Hz), never a GUI DSP timer.
 if(processedFrames_%768==24)controller_->drawLights();
 snapshotEmpty_.store(engine_->DesktopLoopEmpty());
 engine_->DesktopServiceReads();
 for(size_t i=0;i<blockSize;++i) output[i]={outputs[2][i],outputs[3][i],outputs[0][i],outputs[1][i]};
 // Capture is append-only. Publish the completed prefix for DAW autosaves;
 // the saving thread never reads frames still being written by the engine.
 if(capture_) capture_->length.store(std::min(engine_->DesktopBufferLength(),capture_->capacity)/2,std::memory_order_release);
 if(capture_ && (!engine_->Recording() || engine_->DesktopBufferLength()>=capture_->capacity)) stopCapture();
}
Frame TapeEngine::process(Frame input) {
 const auto output=outputBlock_[blockPosition_];
 inputBlock_[blockPosition_++]=input;
 if(blockPosition_==blockSize) { processBlock(inputBlock_.data(),outputBlock_.data()); blockPosition_=0; }
 return output;
}
}
