#include "PluginProcessor.h"
#include "FactoryBank.h"
#include "../GUI/PluginEditor.h"
namespace {
// Keep serialized parameter IDs stable for existing DAW sessions and automation.
constexpr const char* ids[] = {"pitch","start","end","attack","release","gain","pan","filter","resonance","drive","warble","space","time","loopSpeed","overdub","output","input","bank","slot","multi","reverse","autoLoop","sustain","split","monitor","fxPre","compression","sampleSpeed","recordLatch","inputSource","tapeSlew","masterGain","modeSwitch","routing","midiInChannel","midiOutChannel","jammiBank","cubbiBank","jammiSlot","outputBoostDb"};
}

juce::AudioProcessorValueTreeState::ParameterLayout NibbiProcessor::layout() {
 juce::AudioProcessorValueTreeState::ParameterLayout l;
 auto f=[&](const char* id,const char* name,float min,float max,float initial,float skew=1.f) { l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>(min,max,0,skew),initial)); };
 auto b=[&](const char* id,const char* name,bool initial) { l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id,1},name,initial)); };
 f("pitch","Legacy transpose (inactive)",-24,24,0); f("start","Sample start",0,.9999f,0); f("end","Sample end",.0001f,1,1);
 f("attack","Attack",.001f,20.201f,.201f,.3f); f("release","Release",.001f,4.041f,.041f,.4f);
 f("gain","Sample level",0,2.01f,2.f*.704f*.704f+.01f); f("pan","Pan",0,1,.5f);
 f("filter","Filter",0,1,.5f); f("resonance","Resonance",0,1,0); f("drive","Saturation",0,1,0);
 f("warble","Warble",0,1,0); f("space","Space",0,1,0); f("time","Time",0,1,.5f);
 f("loopSpeed","Tape speed",-2,2,1); f("overdub","Overdub feedback",0,1,1);
 f("output","Volume",0,1,.5f); f("input","Input",0,2,.75f);
 l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"bank",1},"Bank",0,4,0));
 l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"slot",1},"Sample",0,14,0));
 b("multi","HITKIT mode (off: MELO)",false); b("reverse","Legacy reverse (inactive)",false); b("autoLoop","Sample loop",true);
 b("sustain","Sustain",true); b("split","Split delay/reverb",false); b("monitor","Legacy monitor (inactive)",false); b("fxPre","Effects before tape",true);
 f("compression","Compression",0,1,0);
 juce::NormalisableRange<float> speed(-2.f,2.f,
  [](float,float,float position) {
   float v=(position-.5f)*2.f, inv=v<0?-1.f:1.f;
   if(std::abs(v)<.33f) return v*1.484848f+.01f*inv;
   if(std::abs(v)<.66f) return (v-.33f*inv)*1.515151f+.5f*inv;
   return (v-.66f*inv)*2.941176f+inv;
  },
  [](float,float,float value) {
   float p=std::abs(value);
   if(p<.5f) p=(p-.01f)*.673469f;
   else if(p<1.f) p=(p-.5f)*.66f+.33f;
   else p=(p-1.f)*.34f+.66f;
   return value<0?(1-p)*.5f:p*.5f+.5f;
  });
 l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"sampleSpeed",1},"Sample speed",speed,1.f));
 b("recordLatch","Latch sample recording",false);
 l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"inputSource",1},"Sampling source",juce::StringArray{"Line input","Microphone (mono)","Resample"},0));
 b("tapeSlew","Tape speed inertia",true);
 f("masterGain","Legacy master trim (inactive)",1,3,2);
 b("modeSwitch","Mode switch down (Shift)",false);
 l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"routing",1},"Routing position",juce::StringArray{"Dry headphone monitor","Wet monitor","External FX send / return"},0));
 l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"midiInChannel",1},"MIDI input channel",1,16,1));
 l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"midiOutChannel",1},"MIDI output channel",1,16,1));
 l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"jammiBank",1},"MELO bank",0,4,0));
 l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"cubbiBank",1},"HITKIT bank",0,4,0));
 l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"jammiSlot",1},"MELO preset",0,14,0));
 l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"outputBoostDb",1},
  "Legacy output boost (inactive)",juce::NormalisableRange<float>(0,24,.1f),12));
 return l;
}
NibbiProcessor::NibbiProcessor() : AudioProcessor(BusesProperties().withInput("Recording input",juce::AudioChannelSet::stereo(),true).withInput("Aux return",juce::AudioChannelSet::stereo(),false).withOutput("Master",juce::AudioChannelSet::stereo(),true).withOutput("Headphones",juce::AudioChannelSet::stereo(),false)),parameters(*this,nullptr,"NibbiTape",layout()) {
 for(size_t i=0;i<std::size(ids);++i) {values_[i]=parameters.getRawParameterValue(ids[i]);parameterObjects_[i]=parameters.getParameter(ids[i]);}
 outgoing_.ensureSize(16384);
 formats_.registerBasicFormats();
 auto catalog=std::make_unique<nibbi::Catalog>(*nibbi::factoryBank());
 publish(std::move(catalog)); stateDirty_.store(false); startTimerHz(20);
}
NibbiProcessor::~NibbiProcessor() { stopTimer(); }
void NibbiProcessor::publish(std::unique_ptr<nibbi::Catalog> catalog) {
 stateDirty_.store(true,std::memory_order_release);
 const auto* next=catalog.get(); catalogs_.push_back(std::move(catalog)); published_.store(next); revision.fetch_add(1);
 // Keep the loop snapshot alive across an audio-thread catalog handoff.
 if(host_) for(const auto& p:catalogs_) if(p->tape && host_->engine.hasLoop(p->tape.get())) { loopLease_=p->tape; break; }
 const auto* used=hazard_.load();
 const auto* candidate=candidateHazard_.load();
 catalogs_.erase(std::remove_if(catalogs_.begin(),catalogs_.end(),[&](auto& p) { return p.get()!=used && p.get()!=candidate && p.get()!=next; }),catalogs_.end());
}
const nibbi::Catalog& NibbiProcessor::acquireCatalog() {
 const nibbi::Catalog* next;
 do {
  const auto candidateRevision=revision.load();
  next=published_.load();
  candidateHazard_.store(next);
  if(next!=published_.load() || candidateRevision!=revision.load()) continue;
  if(next!=audioCatalog_) {
   bool samplesChanged=audioCatalog_==nullptr;
   if(audioCatalog_) for(size_t i=0;i<nibbi::kCatalogSize;++i)
    if(audioCatalog_->samples[i]!=next->samples[i]) { samplesChanged=true; break; }
   // Preset edits publish a new catalog while a note is held. Those edits do
   // not replace sample storage, so the firmware reader can keep sounding.
   if(samplesChanged) {
    host_->engine.allNotesOff();
    host_->engine.installCatalog(next);
   } else host_->engine.repointCatalog(next);
   audioCatalog_=next;
   host_->engine.installLoop(next->tape.get());
  }
  hazard_.store(next);
  candidateHazard_.store(nullptr);
 } while(next!=published_.load());
 return *next;
}
void NibbiProcessor::prepareToPlay(double rate,int) {
 std::lock_guard<std::mutex> serviceLock(serviceMutex_);
 const juce::ScopedLock callbackLock(getCallbackLock());
 std::lock_guard<std::mutex> lock(catalogMutex_);
 processingRate_.store(rate);
 if(host_) {
  host_->engine.stopCapture();
  const auto* current=published_.load();
  // Preserve running tape, but don't replace a session waiting to be applied.
  if(!current->tape || host_->engine.hasLoop(current->tape.get())) {
   auto pcm=host_->engine.copyLoop();
   auto tape=std::make_shared<nibbi::LoopStorage>(); tape->resume=host_->engine.loopPlaying();
   for(size_t i=0;i<pcm.size();++i) tape->pcm[i]=pcm[i];
   tape->memory.length.store(pcm.size());
   auto catalog=std::make_unique<nibbi::Catalog>(*current); catalog->tape=std::move(tape); publish(std::move(catalog));
  }
 }
 parameterSnapshotValid_=false;
 host_=std::make_unique<nibbi::HostAdapter>(); host_->prepare(rate);
 audioCatalog_=nullptr; hazard_.store(nullptr); candidateHazard_.store(nullptr); setLatencySamples(host_->latency());
 outputStage_.prepare(rate);
 loopArmed.store(false); loopCanArm.store(false);
}
bool NibbiProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
 auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();
 auto aux=l.getChannelSet(true,1);
 auto headphones=l.getChannelSet(false,1);
 return out==juce::AudioChannelSet::stereo() && (aux.isDisabled() || aux==juce::AudioChannelSet::stereo()) && (headphones.isDisabled() || headphones==juce::AudioChannelSet::stereo()) && (in.isDisabled() || in==juce::AudioChannelSet::mono() || in==juce::AudioChannelSet::stereo());
}
nibbi::Controls NibbiProcessor::controls() const {
 nibbi::Controls c; auto v=[&](size_t i) { return values_[i]->load(std::memory_order_relaxed); };
 c.inputSource=int(v(29)); c.tapeSlew=v(30)>.5f; c.sampleSpeed=v(27); c.start=v(1); c.end=std::max(c.start+.0001f,v(2)); c.attack=std::max(.201f,v(3)); c.release=std::max(.041f,v(4)); c.gain=std::max(.01f,v(5)); c.pan=v(6);
 c.filter=v(7); c.resonance=v(8); c.drive=v(9); c.warble=v(10); c.space=v(11); c.time=v(12);
 c.loopSpeed=v(13); c.overdub=v(14); c.output=v(15); c.input=std::clamp(v(16),0.f,1.f); c.bank=int(v(17)); c.slot=int(v(18));
 c.jammiBank=int(v(36));c.cubbiBank=int(v(37));c.jammiSlot=int(v(38));
 c.modeSwitch=v(32)>.5f;c.recordLatch=v(28)>.5f;
 c.multi=v(19)>.5f; c.autoLoop=v(21)>.5f; c.sustain=v(22)>.5f; c.split=v(23)>.5f; c.monitor=v(32)<.5f; c.fxPre=v(25)>.5f; c.compression=v(26); c.routing=int(v(33));
 return c;
}
void NibbiProcessor::keyboardEvent(nibbi::Event e) {
 int a,n,b,m; keys_.prepareToWrite(1,a,n,b,m); if(n) { keyData_[size_t(a)]=e; keys_.finishedWrite(1); }
}
void NibbiProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) {
 const juce::ScopedTryLock callbackLock(getCallbackLock());
 if(!callbackLock.isLocked()) { buffer.clear(); midi.clear(); return; }
 juce::ScopedNoDenormals denormals;
 if(!host_) { buffer.clear(); return; }
 const auto& catalog=acquireCatalog();
 bool hostChanged=!parameterSnapshotValid_;
 for(size_t i=0;i<values_.size();++i) {
  const float value=values_[i]->load();hostChanged|=value!=parameterSnapshot_[i];parameterSnapshot_[i]=value;
 }
 // Do not echo host-normalized floats back into the native encoder state on
 // every host block: that would make rounding depend on host buffer size.
 if(hostChanged) {host_->setControls(controls());parameterSnapshotValid_=true;}
 host_->engine.controller().channel=juce::jlimit(0,15,int(values_[35]->load())-1);
 if(auto* capture=pendingCapture_.exchange(nullptr)) { nibbi::Event event{uint8_t(capture->startImmediately?14:16)};event.capture=capture;host_->event(event); }
 unsigned commands=commands_.exchange(0);
 if(commands&StopSample) host_->event({15});
 if(commands&RecordLoop) host_->event({10}); if(commands&PlayLoop) host_->event({11});
 if(commands&ArmLoop) host_->event({12});
 if(commands&ClearLoop) host_->event({13}); if(commands&Panic) { host_->event({4}); for(auto& n:displayNotes) n.store(false); }
 outgoing_.clear();
 auto& outgoing=outgoing_;
 const int outChannel=juce::jlimit(1,16,int(values_[35]->load(std::memory_order_relaxed)));
 auto dispatchKey=[&](const nibbi::Event& e) {
  host_->event(e);
  if(e.type==1) outgoing.addEvent(juce::MidiMessage::noteOn(outChannel,int(e.note),juce::uint8(juce::jlimit(1,127,juce::roundToInt(e.value*127.f)))),0);
  else if(e.type==2) outgoing.addEvent(juce::MidiMessage::noteOff(outChannel,int(e.note)),0);
  else if(e.type==6 || e.type==7 || e.type==9) {
   const int cc=e.type==6?27:e.type==7?26:int(e.note);
   outgoing.addEvent(juce::MidiMessage::controllerEvent(outChannel,cc,juce::jlimit(0,127,juce::roundToInt(e.value*127.f))),0);
  }
 };
 int a,n,b,m; keys_.prepareToRead(256,a,n,b,m);
 for(int i=0;i<n;++i) dispatchKey(keyData_[size_t(a+i)]); for(int i=0;i<m;++i) dispatchKey(keyData_[size_t(b+i)]); keys_.finishedRead(n+m);
 auto it=midi.cbegin(); float maximum=0;
 const int inChannel=juce::jlimit(1,16,int(values_[34]->load(std::memory_order_relaxed)));
 auto inputBus=getBusBuffer(buffer,true,0);
 auto auxBus=getBusBuffer(buffer,true,1);
 auto masterBus=getBusBuffer(buffer,false,0);
 auto headphoneBus=getBusBuffer(buffer,false,1);
 for(int i=0;i<buffer.getNumSamples();++i) {
  while(it!=midi.cend() && (*it).samplePosition<=i) {
   const auto metadata=*it; const auto* d=metadata.data; int size=metadata.numBytes;
   if(size>=1 && (size<3 || (d[1]<128 && d[2]<128))) {
    uint8_t status=d[0]&0xf0,ch=uint8_t((d[0]&15)+1);
    if(ch==inChannel) {
     if(size>=3 && status==0x90 && d[2] && d[1]>=24 && d[1]<=72) {
      host_->event({1,d[1],ch,float(d[2]+1)/127}); displayNotes[d[1]].store(true);
     }
     else if(size>=3 && (status==0x80 || status==0x90) && d[1]>=24 && d[1]<=72) { host_->event({2,d[1],ch}); displayNotes[d[1]].store(false); }
     else if(size>=3 && status==0xb0 && d[1]>=20 && d[1]<=25) {
      const int knob=int(d[1])-20;
      if(knob!=4 || host_->engine.loopPlaying()) {
       host_->event({19,uint8_t(knob),0,float(d[2])});
      }
     }
     else if(size>=3 && status==0xb0 && (d[1]==26 || d[1]==27)) {
      const int button=int(d[1])-26;
      bool next=midiTransportDown_[size_t(button)];
      if(d[2]>84) next=true; else if(d[2]<42) next=false;
      if(next!=midiTransportDown_[size_t(button)]) {
       midiTransportDown_[size_t(button)]=next;
       host_->event({uint8_t(button==0?7:6),0,ch,next?1.f:0.f});
      }
     }
    }
   }
   ++it;
  }
  nibbi::Frame input;
  if(inputBus.getNumChannels()>0) {
   input.l=inputBus.getSample(0,i);
   input.r=inputBus.getSample(inputBus.getNumChannels()>1?1:0,i);
  }
  input.auxL=auxBus.getNumChannels()>0?auxBus.getSample(0,i):input.l;
  input.auxR=auxBus.getNumChannels()>1?auxBus.getSample(1,i):input.r;
  auto f=host_->process(input,catalog);
  nibbi::Controls updated;
  if(host_->takeControlState(updated)) syncControlState(updated);
  nibbi::control::ControlSurface::Midi sent;
  while(host_->engine.controller().surface.midi.pop(sent)) {
   const int channel=juce::jlimit(1,16,sent.channel+1), value=juce::jlimit(0,127,sent.value);
   if(sent.type==0)outgoing.addEvent(juce::MidiMessage::controllerEvent(channel,sent.note,value),i);
   else if(sent.type==1)outgoing.addEvent(juce::MidiMessage::noteOn(channel,sent.note,juce::uint8(value)),i);
   else outgoing.addEvent(juce::MidiMessage::noteOff(channel,sent.note),i);
  }
  // Desktop listening level, after the firmware's recording, feedback and FX.
  outputStage_.processStereo(f.l,f.r,0);
  outputStage_.processStereo(f.hpL,f.hpR,1);
  masterBus.setSample(0,i,f.l); masterBus.setSample(1,i,f.r);
  if(headphoneBus.getNumChannels()==2) {
   headphoneBus.setSample(0,i,f.hpL); headphoneBus.setSample(1,i,f.hpR);
  }
  maximum=std::max({maximum,std::abs(f.l),std::abs(f.r)});
 }
 midi.clear(); midi.addEvents(outgoing,0,buffer.getNumSamples(),0);
 const auto& lights=host_->engine.controller().lights;
 for(size_t i=0;i<10;++i)ledColours[i].store(nibbi::control::packLed(lights.lamps[i]),std::memory_order_relaxed);
 for(size_t i=0;i<25;++i)ledColours[10+i].store(nibbi::control::packLed(lights.keys[i]),std::memory_order_relaxed);
 const bool recording=host_->engine.loopRecording(),playing=host_->engine.loopPlaying();
 if(recording || sampleRecording.load() || recording!=loopRecording.load()
    || playing!=loopPlaying.load() || int(host_->engine.loopFrames())!=loopLength.load())
  stateDirty_.store(true,std::memory_order_release);
 peak.store(maximum); loopRecording.store(recording); loopPlaying.store(playing);
 loopArmed.store(host_->engine.loopArmed()); loopCanArm.store(host_->engine.canArmLoop());
 loopPosition.store(host_->engine.loopPosition()); loopLength.store(int(host_->engine.loopFrames()));
}
int NibbiProcessor::selectedSlot() const { return nibbi::catalogIndex(values_[19]->load()>.5f,int(values_[17]->load()),int(values_[18]->load())); }
juce::String NibbiProcessor::factoryError() const { return nibbi::factoryBankError(); }
juce::String NibbiProcessor::editPreset(PresetAction action, int source, int target) {
 const bool tapeTarget=target==-2;
 if((target<0 && !tapeTarget) || target>=int(nibbi::kCatalogSize) ||
    (target>=0 && target%15==14 && (target!=14 || action!=PresetAction::copy)) ||
    (tapeTarget && action==PresetAction::erase))
  return "Choose a valid preset or tape destination.";
 std::lock_guard<std::mutex> lock(catalogMutex_);
 auto catalog=std::make_unique<nibbi::Catalog>(*published_.load());
 if(action==PresetAction::saveBuffer) source=14;
 if(action!=PresetAction::erase) {
  std::shared_ptr<const nibbi::Sample> sample;
  nibbi::PresetState settings;
  if(source==-2) {
   std::vector<int16_t> loop;
   if(catalog->tape && (!host_ || !host_->engine.hasLoop(catalog->tape.get()))) {
    const auto count=catalog->tape->memory.length.load(); loop.resize(count);
    for(size_t i=0;i<count;++i) loop[i]=catalog->tape->pcm[i];
   } else if(host_) loop=host_->engine.copyLoop();
   if(loop.size()<4) return "The tape loop is empty.";
   auto copied=std::make_shared<nibbi::Sample>(); copied->pcm=std::move(loop); copied->live=target==14;
   sample=std::move(copied); settings={};
  } else {
   if(source<0 || source>=int(nibbi::kCatalogSize) || !catalog->samples[size_t(source)])
    return "The source sample is empty.";
   sample=catalog->samples[size_t(source)];
   settings=catalog->presets[size_t(source)];
  }
  if(tapeTarget) {
   if(sample->sampleRate!=48000) return "Tape copy requires a 48 kHz sample.";
   if(sample->pcm.size()>daisy::kMaxRamBuffSize) return "The sample is too long for the tape loop.";
   auto tape=std::make_shared<nibbi::LoopStorage>();
   for(size_t i=0;i<sample->pcm.size();++i) tape->pcm[i]=sample->pcm[i];
   tape->memory.length.store(sample->pcm.size()); tape->resume=false;
   catalog->tape=std::move(tape);
  } else {
   if(sample->live!=(target==14)) {
    auto copied=std::make_shared<nibbi::Sample>(*sample);
    copied->live=target==14; copied->factoryId=-1;
    sample=std::move(copied);
   }
   catalog->samples[size_t(target)]=sample;
   catalog->names[size_t(target)]=source==-2?"Copied tape loop":catalog->names[size_t(source)];
   catalog->presets[size_t(target)]=settings;
  }
 } else {
  catalog->samples[size_t(target)].reset(); catalog->names[size_t(target)].clear(); catalog->presets[size_t(target)]={};
 }
 publish(std::move(catalog));
 return action==PresetAction::erase?"Preset erased.":tapeTarget?"Tape loop copied.":"Preset saved.";
}
juce::String NibbiProcessor::loadSample(const juce::File& file, int targetSlot) {
 const bool selectImported=targetSlot<0;
 if(selectImported) targetSlot=14; // A drop arms the shared temporary sample, like recording.
 if(targetSlot<0 || targetSlot>=int(nibbi::kCatalogSize)) return "Invalid sample slot.";
 std::unique_ptr<juce::AudioFormatReader> reader(formats_.createReaderFor(file));
 if(!reader || reader->lengthInSamples<2 || reader->sampleRate<8000 || reader->sampleRate>384000) return "Choose a valid audio file.";
 if(reader->lengthInSamples>int64_t(reader->sampleRate*180)) return "Samples can be up to three minutes long.";
 const bool buffer=targetSlot==14;
 const double outputRate=buffer?48000.:reader->sampleRate;
 const auto frames=size_t(std::llround(double(reader->lengthInSamples)*outputRate/reader->sampleRate));
 if(frames<2) return "This audio file is too short to load.";
 if(buffer && frames>daisy::kMaxRamBuffSize/2) return "This sample is too long for the temporary buffer (about 165 seconds).";
 juce::AudioBuffer<float> data(2,int(reader->lengthInSamples));
 if(!reader->read(&data,0,data.getNumSamples(),0,true,true)) return "The audio file could not be read.";
 auto sample=std::make_shared<nibbi::Sample>();sample->sampleRate=outputRate;sample->live=buffer;sample->pcm.resize(frames*2);
 auto pcm=[](float x) {return int16_t(daisy::f2s16(std::isfinite(x)?x:0));};
 if(outputRate==reader->sampleRate) {
  for(size_t i=0;i<frames;++i) for(int ch=0;ch<2;++ch)sample->pcm[i*2+size_t(ch)]=pcm(data.getSample(ch,int(i)));
 } else {
  // The native RAM player is fixed at 48 kHz. Convert on the loader thread,
  // compensating the filter delay so imported duration and pitch stay intact.
  nibbi::RateFilter converter;converter.prepare(reader->sampleRate,outputRate);
  int64_t source=0;
  for(size_t i=0;i<frames;++i) {
   const double time=double(i)*reader->sampleRate/outputRate+32.;
   while(source<=int64_t(std::floor(time))) {
    nibbi::Frame input;
    if(source<data.getNumSamples()) {input.l=data.getSample(0,int(source));input.r=data.getSample(1,int(source));}
    converter.push(input);++source;
   }
   const auto converted=converter.at(time);
   sample->pcm[i*2]=pcm(converted.l);sample->pcm[i*2+1]=pcm(converted.r);
  }
 }
 const juce::ScopedLock callbackLock(getCallbackLock());
 std::lock_guard<std::mutex> lock(catalogMutex_);
 if(buffer && capture_) return "Finish sampling before loading an audio file.";
 auto catalog=std::make_unique<nibbi::Catalog>(*published_.load());const auto slot=size_t(targetSlot);
 size_t total=sample->pcm.size();
 for(size_t i=0;i<nibbi::kCatalogSize;++i) if(i!=slot && catalog->samples[i]) total+=catalog->samples[i]->pcm.size()+catalog->samples[i]->doublePcm.size();
 if(total>128*1024*1024) return "The sample bank is full (256 MB). Replace an existing sample to free space.";
 catalog->samples[slot]=sample;catalog->names[slot]=file.getFileNameWithoutExtension().toStdString();
 catalog->presets[slot]={};catalog->presets[slot].valid=true;
 if(selectImported) {
  for(const auto* id:{"sampleSpeed","pitch","start","end","attack","release","gain","pan","reverse","autoLoop","sustain"}) {
   auto* parameter=parameters.getParameter(id);
   parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->getDefaultValue());parameter->endChangeGesture();
  }
  auto select=[&](const char* id,float value) {
   auto* parameter=parameters.getParameter(id);
   parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(value));parameter->endChangeGesture();
  };
  select("multi",0);select("bank",values_[36]->load());select("slot",14);select("jammiSlot",14);
  parameterSnapshotValid_=false;
 }
 publish(std::move(catalog));
 return {};
}
juce::String NibbiProcessor::sampleName() { std::lock_guard<std::mutex> lock(catalogMutex_); auto& n=published_.load()->names[size_t(selectedSlot())]; return n.empty()?"Drop a sample here":juce::String(n); }
void NibbiProcessor::resetFactoryState() {
 std::lock_guard<std::mutex> serviceLock(serviceMutex_);
 // Prepare all large buffers off the audio thread, including fresh FX state.
 auto replacement=std::make_unique<nibbi::HostAdapter>();
 replacement->prepare(processingRate_.load());
 auto factory=std::make_unique<nibbi::Catalog>(*nibbi::factoryBank());
 factory->tape=std::make_shared<nibbi::LoopStorage>();
 {
  const juce::ScopedLock callbackLock(getCallbackLock());
  std::lock_guard<std::mutex> lock(catalogMutex_);
  // The old engine is retired on this calling thread. A capture cannot publish
  // after reset, and old tape/FX tails cannot leak into the fresh instrument.
  host_.swap(replacement);parameterSnapshotValid_=false;
  pendingCapture_.store(nullptr); capture_.reset();completedCaptureLease_.reset();
  commands_.store(0); keys_.reset();
  pendingAction.store(-1);shiftMenuActive.store(false);
  midiTransportDown_.fill(false);
  for(auto& page:encoderPages_) page.store(0);
  for(auto& note:displayNotes) note.store(false);
  for(const auto* id:ids) {
   auto* parameter=parameters.getParameter(id);
   parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->getDefaultValue()); parameter->endChangeGesture();
  }
  audioCatalog_=nullptr; hazard_.store(nullptr); candidateHazard_.store(nullptr);
  publish(std::move(factory)); loopLease_.reset();
  outputStage_.prepare(processingRate_.load());
  sampleRecording.store(false); loopRecording.store(false); loopPlaying.store(false);
  loopArmed.store(false); loopCanArm.store(false); loopLength.store(0); loopPosition.store(0); peak.store(0);
 }
}
juce::String NibbiProcessor::recordSample(bool startImmediately) {
 std::lock_guard<std::mutex> lock(catalogMutex_);
 if(sampleRecording.load()) { command(StopSample); return {}; }
 if(capture_) return "Finishing the previous recording.";
 size_t available=128*1024*1024; int slot=14; // The shared live buffer, as on the instrument.
 const auto* catalog=published_.load();
 for(size_t i=0;i<nibbi::kCatalogSize;++i) if(int(i)!=slot && catalog->samples[i]) available-=std::min(available,catalog->samples[i]->pcm.size()+catalog->samples[i]->doublePcm.size());
 available=std::min(available,daisy::kMaxRamBuffSize); available-=available%2;
 if(available<4) return "The sample bank is full. Replace an existing sample.";
 capture_=std::make_unique<nibbi::Capture>(available); capture_->slot=slot;capture_->startImmediately=startImmediately;
 sampleRecording.store(true); pendingCapture_.store(capture_.get());
 return {};
}
void NibbiProcessor::syncControlState(const nibbi::Controls& c) {
 const std::pair<size_t,float> updated[]={
  {27,c.sampleSpeed},{1,c.start},{2,c.end},{3,c.attack},{4,c.release},{5,c.gain},{6,c.pan},
  {21,float(c.autoLoop)},{22,float(c.sustain)},{11,c.space},{9,c.drive},{7,c.filter},
  {15,c.output},{16,c.input},{13,c.loopSpeed},{14,c.overdub},{26,c.compression},{12,c.time},
  {10,c.warble},{8,c.resonance},{19,float(c.multi)},{17,float(c.bank)},{18,float(c.slot)},
  {29,float(c.inputSource)},{33,float(c.routing)},{25,float(c.fxPre)},
  {36,float(c.jammiBank)},{37,float(c.cubbiBank)},{38,float(c.jammiSlot)}};
 for(const auto& value:updated) {
  auto* p=parameterObjects_[value.first];
  const float bounded=juce::jlimit(p->getNormalisableRange().start,p->getNormalisableRange().end,value.second);
  if(std::abs(values_[value.first]->load()-bounded)>1.e-6f)
   p->setValueNotifyingHost(p->convertTo0to1(bounded));
  parameterSnapshot_[value.first]=values_[value.first]->load();
 }
 for(size_t i=0;i<6;++i)encoderPages_[i].store(host_->engine.controller().pages[i]);
 pendingAction.store(host_->engine.controller().pending());
 shiftMenuActive.store(host_->engine.controller().menuActive());
}
void NibbiProcessor::serviceState() {
 std::lock_guard<std::mutex> serviceLock(serviceMutex_);
 if(host_) {
  auto& ui=host_->engine.controller();
  {
   std::lock_guard<std::mutex> lock(catalogMutex_);
   const auto* current=published_.load();std::unique_ptr<nibbi::Catalog> changed;
   // The audio-owned preset values are atomic; file/copy work stays here.
   for(size_t i=0;i<nibbi::kCatalogSize;++i) {
    if(!ui.presets.consumeDirty(i))continue;
    auto preset=ui.presets.snapshot(i);
    if(preset.controls!=current->presets[i].controls || preset.valid!=current->presets[i].valid) {
     if(!changed)changed=std::make_unique<nibbi::Catalog>(*current);
     changed->presets[i]=preset;
    }
   }
   if(changed)publish(std::move(changed));
  }
  nibbi::control::CopyService::CopyRequest request;
  if(!ui.copies.req_fifo.IsEmpty())ui.copies.busy.store(true);
  while(ui.copies.req_fifo.pop(request)) {
   auto index=[](size_t slot,size_t bank,daisy::VoiceMode mode) {return slot==16?-2:nibbi::catalogIndex(mode==daisy::VoiceMode::CUBBI,int(bank),int(slot)-1);};
   editPreset(PresetAction::copy,index(request.src,request.src_bank,request.src_mode),index(request.dest,request.dest_bank,request.dest_mode));
  }
  ui.copies.busy.store(false);
  nibbi::control::ControlEngine::Erase erase;
  auto& target=host_->engine.controllerTarget();
  if(!target.erases.IsEmpty())target.eraseBusy.store(true);
  while(target.erases.pop(erase)) {
   editPreset(PresetAction::erase,-1,nibbi::catalogIndex(erase.mode!=0,erase.bank,erase.slot-1));
  }
  target.eraseBusy.store(false);
 }
 {
  std::lock_guard<std::mutex> lock(catalogMutex_);
  const auto* acknowledged=hazard_.load();
  if(completedCaptureLease_ && acknowledged && acknowledged->samples[14]==completedCaptureLease_)
   completedCaptureLease_.reset();
  if(capture_ && capture_->status.load(std::memory_order_acquire)==2) {
   auto length=capture_->length.load();
   if(length>=2) {
    capture_->sample->pcm.resize(length*2);
    capture_->sample->live=true;
    // Keep newly attached PCM alive until audio acknowledges a catalog that
    // owns it, even if a second UI edit replaces the pending catalog first.
    completedCaptureLease_=capture_->sample;
    auto catalog=std::make_unique<nibbi::Catalog>(*published_.load());
    catalog->samples[size_t(capture_->slot)]=capture_->sample; catalog->names[size_t(capture_->slot)]="Recorded sample";
    // PresetManager keeps the shared buffer's stored controls across captures.
    publish(std::move(catalog));
   }
   capture_.reset(); sampleRecording.store(false);
  }
 }
}
void NibbiProcessor::timerCallback() {
 serviceState();
 // Notify outside service/catalog locks: a host may synchronously request state.
 if(stateDirty_.exchange(false,std::memory_order_acq_rel))
  updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}

void NibbiProcessor::getStateInformation(juce::MemoryBlock& out) {
 serviceState(); // Finish capture without notifying/re-entering the host's save call.
 std::lock_guard<std::mutex> serviceLock(serviceMutex_);
 juce::MemoryOutputStream stream(out,false); stream.writeInt(0x43485035);
 auto state=parameters.copyState(); state.setProperty("factoryBankVersion",1,nullptr);
 auto xml=state.createXml(); stream.writeString(xml->toString());
 std::lock_guard<std::mutex> lock(catalogMutex_);
 auto catalog=*published_.load();
 if(capture_) {
  const size_t frames=capture_->length.load(std::memory_order_acquire);
  if(frames>=2) {
   auto sample=std::make_shared<nibbi::Sample>();sample->live=true;
   sample->pcm.assign(capture_->sample->pcm.begin(),capture_->sample->pcm.begin()+frames*2);
   catalog.samples[size_t(capture_->slot)]=std::move(sample);
   catalog.names[size_t(capture_->slot)]="Recorded sample";
  }
 }
 for(size_t slot=0;slot<nibbi::kCatalogSize;++slot) {
  auto sample=catalog.samples[slot]; stream.writeString(catalog.names[slot]);
  size_t earlier=0;
  while(earlier<slot && catalog.samples[earlier]!=sample) ++earlier;
  if(sample && earlier<slot) { stream.writeInt(-2); stream.writeInt(int(earlier)); continue; }
  stream.writeInt(sample?sample->factoryId:-1);
  if(sample && sample->factoryId>=0) continue;
  stream.writeDouble(sample?sample->sampleRate:48000); stream.writeInt(sample?sample->root:60);
  stream.writeBool(sample && sample->live);
  stream.writeInt(sample?int(sample->pcm.size()):0);
  if(sample) for(auto x:sample->pcm) stream.writeShort(x);
 }
 std::vector<int16_t> loop;
 bool playing=loopPlaying.load();
 if(catalog.tape && (!host_ || !host_->engine.hasLoop(catalog.tape.get()))) { auto count=catalog.tape->memory.length.load(); loop.resize(count); for(size_t i=0;i<count;++i) loop[i]=catalog.tape->pcm[i]; playing=catalog.tape->resume; }
 else if(host_) loop=host_->engine.copyLoop();
 stream.writeInt(int(loop.size())); stream.writeBool(playing);
 for(auto x:loop) stream.writeShort(x);
 for(const auto& preset:catalog.presets) {
  stream.writeBool(preset.valid);
  for(float value:preset.controls) stream.writeFloat(value);
 }
}
void NibbiProcessor::setStateInformation(const void* data,int size) {
 if(size<4 || size>512*1024*1024) return;
 juce::MemoryInputStream stream(data,size_t(size),false);
 const int version=stream.readInt();
 if(version!=0x43485032 && version!=0x43485033 && version!=0x43485034 && version!=0x43485035) return;
 auto xml=juce::parseXML(stream.readString());
 if(!xml) return;
 // Read projects saved before the product rename; always write NibbiTape.
 if(xml->hasTagName("ChompTape"))xml->setTagName("NibbiTape");
 if(!xml->hasTagName("NibbiTape")) return;
 auto factory=nibbi::factoryBank();
 auto catalog=std::make_unique<nibbi::Catalog>();
 size_t totalSamples=0;
 const size_t slots=version>=0x43485034?nibbi::kCatalogSize:75;
 for(size_t slot=0;slot<slots;++slot) {
  catalog->names[slot]=stream.readString().toStdString();
  if(version>=0x43485034) {
   if(stream.getNumBytesRemaining()<4) return;
   const int id=stream.readInt();
   if(id==-2) {
    if(stream.getNumBytesRemaining()<4) return;
    const int earlier=stream.readInt();
    if(earlier<0 || size_t(earlier)>=slot || !catalog->samples[size_t(earlier)]) return;
    catalog->samples[slot]=catalog->samples[size_t(earlier)];
    if(const int factoryId=catalog->samples[slot]->factoryId; factoryId>=0)
     catalog->names[slot]=factory->names[size_t(factoryId)];
    continue;
   }
   if(id>=0) {
    if(id>=int(nibbi::kCatalogSize) || !factory->samples[size_t(id)] || factory->samples[size_t(id)]->factoryId!=id) return;
    catalog->samples[slot]=factory->samples[size_t(id)];
    catalog->names[slot]=factory->names[size_t(id)];
    totalSamples+=catalog->samples[slot]->pcm.size()+catalog->samples[slot]->doublePcm.size();
    if(totalSamples>128*1024*1024) return;
    continue;
   }
   if(id!=-1) return;
  }
  if(stream.getNumBytesRemaining()<(version==0x43485032?16:17)) return;
  double rate=stream.readDouble(); int root=stream.readInt(); bool live=version!=0x43485032?stream.readBool():false; int count=stream.readInt();
  if(!std::isfinite(rate) || rate<8000 || rate>384000 || root<0 || root>127 || count<0 || count%2 || count>rate*360 || int64_t(count)*2>stream.getNumBytesRemaining()) return;
  totalSamples+=size_t(count); if(totalSamples>128*1024*1024) return;
  if(count) { auto sample=std::make_shared<nibbi::Sample>(); sample->sampleRate=rate; sample->root=root; sample->live=live; sample->pcm.resize(size_t(count)); for(auto& x:sample->pcm) x=stream.readShort(); catalog->samples[slot]=std::move(sample); }
 }
 if(version<0x43485034) {
  // Earlier desktop sessions used the same files in both modes.
  for(size_t i=0;i<75;++i) { catalog->samples[i+75]=catalog->samples[i]; catalog->names[i+75]=catalog->names[i]; }
 }
 if(stream.getNumBytesRemaining()<5) return;
 int loopCount=stream.readInt(); bool resume=stream.readBool();
 constexpr int64_t presetBytes=int64_t(nibbi::kCatalogSize)*(1+9*4);
 if(loopCount<0 || loopCount%2 || size_t(loopCount)>daisy::kMaxRamBuffSize || int64_t(loopCount)*2+(version==0x43485035?presetBytes:0)!=stream.getNumBytesRemaining()) return;
 catalog->tape=std::make_shared<nibbi::LoopStorage>(); catalog->tape->resume=resume;
 for(int i=0;i<loopCount;++i) catalog->tape->pcm[size_t(i)]=stream.readShort();
 catalog->tape->memory.length.store(size_t(loopCount));
 if(version==0x43485035) for(auto& preset:catalog->presets) {
  preset.valid=stream.readBool();
  for(auto& control:preset.controls) {
   control=stream.readFloat();
   if(!std::isfinite(control) || control<0.f || control>1.f) return;
  }
 }
 if(stream.getNumBytesRemaining()!=0) return;
 auto restored=juce::ValueTree::fromXml(*xml);
 // Early standalone prototypes saved missing parameter values as empty XML
 // attributes. Those must restore defaults, not silently become zero.
 for(const auto* id:ids) {
  auto child=restored.getChildWithProperty("id",id);
  auto* parameter=parameters.getParameter(id);
  if(!child.isValid()) {
   child=juce::ValueTree("PARAM"); child.setProperty("id",id,nullptr); restored.appendChild(child,nullptr);
  }
  if(!child.hasProperty("value")) child.setProperty("value",parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);
 }
 for(auto child:restored) {
  const auto id=child["id"].toString(); auto* parameter=parameters.getParameter(id);
  float value=float(child["value"]);
  if(!parameter || !std::isfinite(value) || value<parameter->getNormalisableRange().start || value>parameter->getNormalisableRange().end) return;
 }
 bool hasFactorySample=false;
 for(const auto& sample:catalog->samples) if(sample && sample->factoryId>=0) { hasFactorySample=true; break; }
 const bool prototypeState=!restored.hasProperty("factoryBankVersion");
 if(wrapperType==wrapperType_Standalone && (prototypeState || !hasFactorySample) && nibbi::factoryBankError().isEmpty()) {
  // Also repair standalone states saved while a failed factory load left every
  // factory slot empty. Preserve any samples the user imported or recorded.
  int firstFactory=-1;
  for(size_t slot=0;slot<nibbi::kCatalogSize;++slot) {
   const auto& sample=factory->samples[slot];
   if(!catalog->samples[slot] && sample) {
    const size_t count=sample->pcm.size()+sample->doublePcm.size();
    if(totalSamples+count>128*1024*1024) continue;
    catalog->samples[slot]=sample; catalog->names[slot]=factory->names[slot]; totalSamples+=count;
    if(firstFactory<0 && slot<75 && slot%15<14 && sample->factoryId>=0) firstFactory=int(slot);
   }
  }
  if(firstFactory>=0 && prototypeState) {
   for(const auto* id:ids) {
    auto* parameter=parameters.getParameter(id);
    restored.getChildWithProperty("id",id).setProperty("value",parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);
   }
   restored.getChildWithProperty("id","bank").setProperty("value",firstFactory/15,nullptr);
   restored.getChildWithProperty("id","slot").setProperty("value",firstFactory%15,nullptr);
   catalog->tape->resume=false;
  } else if(firstFactory>=0) {
   const int bank=int(float(restored.getChildWithProperty("id","bank")["value"]));
   const int slot=int(float(restored.getChildWithProperty("id","slot")["value"]));
   const bool multi=float(restored.getChildWithProperty("id","multi")["value"])>.5f;
   if(!catalog->samples[size_t(nibbi::catalogIndex(multi,bank,slot))]) {
    restored.getChildWithProperty("id","bank").setProperty("value",firstFactory/15,nullptr);
    restored.getChildWithProperty("id","slot").setProperty("value",firstFactory%15,nullptr);
    restored.getChildWithProperty("id","multi").setProperty("value",false,nullptr);
   }
  }
 }
 restored.setProperty("factoryBankVersion",1,nullptr);
 {
  std::lock_guard<std::mutex> serviceLock(serviceMutex_);
  // A host can restore while recording, or before the queued capture starts.
  // Retire the old adapter (including queued raw capture pointers) together
  // with its capture so it cannot later publish over the recalled session.
  std::unique_ptr<nibbi::HostAdapter> replacement;
  if(host_) { replacement=std::make_unique<nibbi::HostAdapter>();replacement->prepare(processingRate_.load()); }
  const juce::ScopedLock callbackLock(getCallbackLock());
  std::lock_guard<std::mutex> lock(catalogMutex_);
  host_.swap(replacement);
  pendingCapture_.store(nullptr);commands_.store(0);keys_.reset();
  capture_.reset();completedCaptureLease_.reset();loopLease_.reset();
  audioCatalog_=nullptr;hazard_.store(nullptr);candidateHazard_.store(nullptr);
  parameterSnapshotValid_=false;midiTransportDown_.fill(false);
  for(auto& note:displayNotes)note.store(false);
  sampleRecording.store(false);loopRecording.store(false);loopArmed.store(false);
  loopPlaying.store(catalog->tape->resume);loopLength.store(int(catalog->tape->memory.length.load()/2));
  loopPosition.store(0);peak.store(0);outputStage_.prepare(processingRate_.load());
  parameters.replaceState(restored);publish(std::move(catalog));
  stateDirty_.store(false);
 }
}
juce::AudioProcessorEditor* NibbiProcessor::createEditor() { return new NibbiEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NibbiProcessor(); }
