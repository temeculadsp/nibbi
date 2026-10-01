#include "plugin/PluginProcessor.h"
#include "plugin/FactoryBank.h"
#include "plugin/StandaloneSettings.h"
#include "GUI/PluginEditor.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <new>
#include <limits>
thread_local bool watchAudio = false;
std::atomic<int> audioAllocations{0};
void* operator new(size_t n) { if(watchAudio) ++audioAllocations; if(void* p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { if(watchAudio) ++audioAllocations; std::free(p); }
void* operator new[](size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }

void require(bool ok,const std::string& text) { if(!ok) throw std::runtime_error(text); }
// The same MIDI timeline must render identically with the editor closed,
// whether the host supplies large blocks or irregular tiny blocks.
void checkMidiPartitions(double rate) {
 auto render=[&](const std::vector<int>& sizes) {
  auto processor=std::make_unique<NibbiProcessor>();processor->prepareToPlay(rate,4096);
  std::vector<float> result(12000);
  struct Scheduled {int time;juce::MidiMessage message;};
  const Scheduled schedule[]={
   {17,juce::MidiMessage::noteOn(1,60,.7f)},
   {53,juce::MidiMessage::controllerEvent(1,20,94)},
   {147,juce::MidiMessage::controllerEvent(1,25,89)},
   {2051,juce::MidiMessage::controllerEvent(1,20,110)},
   {3071,juce::MidiMessage::controllerEvent(1,23,31)},
   {4901,juce::MidiMessage::noteOff(1,60)}};
  int position=0,index=0;
  while(position<6000) {
   const int count=std::min(sizes[size_t(index++)%sizes.size()],6000-position);
   juce::AudioBuffer<float> audio(2,count);audio.clear();juce::MidiBuffer midi;midi.ensureSize(16384);
   for(const auto& event:schedule)if(event.time>=position && event.time<position+count)midi.addEvent(event.message,event.time-position);
   watchAudio=true;processor->processBlock(audio,midi);watchAudio=false;
   for(int i=0;i<count;++i)for(int ch=0;ch<2;++ch)result[size_t(position+i)*2+size_t(ch)]=audio.getSample(ch,i);
   position+=count;
  }
  require(std::abs(processor->parameters.getRawParameterValue("output")->load()-89.f/127.f)<1.e-6f,"MIDI volume works without a GUI timer");
  require(audioAllocations==0,"MIDI control processing does not allocate");
  return result;
 };
 const auto big=render({4096}),small=render({1,23,37,257,19,512});
 for(size_t i=0;i<big.size();++i)if(big[i]!=small[i]) {
  float maximum=0;for(size_t j=i;j<big.size();++j)maximum=std::max(maximum,std::abs(big[j]-small[j]));
  throw std::runtime_error("MIDI partition mismatch rate="+std::to_string(rate)+" frame="+std::to_string(i/2)+" big="+std::to_string(big[i])+" small="+std::to_string(small[i])+" max="+std::to_string(maximum));
 }
}
void checkNativeMenus() {
 auto p=std::make_unique<NibbiProcessor>();p->prepareToPlay(48000,512);
 juce::AudioBuffer<float> block(2,512);juce::MidiBuffer midi;midi.ensureSize(16384);
 auto process=[&](int count=1) {for(int i=0;i<count;++i) {block.clear();midi.clear();watchAudio=true;p->processBlock(block,midi);watchAudio=false;}};
 auto shift=[&](bool down) {p->keyboardEvent({22,0,0,down?1.f:0.f});};
 auto button=[&](int id) {p->keyboardEvent({20,uint8_t(id),1,1});p->keyboardEvent({20,uint8_t(id),1,0});};
 process();
 const auto source=p->sampleName();
 shift(true);button(30);button(15);button(8);button(5);shift(false);process();
 require(!p->sampleRecording,"The sample button confirms the Shift copy without starting a recording");
 juce::MemoryBlock saved;p->getStateInformation(saved);process(105);
 require(p->parameters.getRawParameterValue("slot")->load()==1.f && p->sampleName()==source,"Original menu copy selects the copied destination after completion");
 const float volume=p->parameters.getRawParameterValue("output")->load();
 shift(true);process();
 midi.addEvent(juce::MidiMessage::controllerEvent(1,25,10),0);block.clear();p->processBlock(block,midi);
 require(p->parameters.getRawParameterValue("output")->load()==volume,"MIDI cannot release a held Shift menu");
 shift(false);p->keyboardEvent({21,5,0,1});process();
 require(p->parameters.getRawParameterValue("output")->load()>volume,"Releasing Shift restores the normal encoder function");
 // Switch banks in both engines, then confirm the remembered banks survive a project.
 shift(true);button(12);button(12);shift(false);process();
 require(p->parameters.getRawParameterValue("cubbiBank")->load()==1.f,"Native drum bank memory reaches host state");
 shift(true);button(7);shift(false);process();
 require(p->parameters.getRawParameterValue("bank")->load()==0.f,"Native chromatic bank memory is retained");
 require(audioAllocations==0,"Native menu actions perform no callback allocations");
}
void checkSampleImport() {
 for(double rate:{44100.,48000.,96000.}) {
  juce::TemporaryFile wav(".wav");
  {
   auto stream=wav.getFile().createOutputStream();juce::WavAudioFormat format;
   std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),rate,2,16,{},0));
   require(writer!=nullptr,"Create import fixture");stream.release();
   juce::AudioBuffer<float> source(2,int(rate));
   for(int i=0;i<source.getNumSamples();++i) {
    source.setSample(0,i,.08f*std::sin(float(2*juce::MathConstants<double>::pi*220*i/rate)));
    source.setSample(1,i,source.getSample(0,i));
   }
   require(writer->writeFromAudioSampleBuffer(source,0,source.getNumSamples()),"Write import fixture");
  }
  auto processor=std::make_unique<NibbiProcessor>();processor->prepareToPlay(48000,256);
  processor->parameters.getParameter("multi")->setValueNotifyingHost(1);
  processor->parameters.getParameter("start")->setValueNotifyingHost(.8f);
  processor->parameters.getParameter("sampleSpeed")->setValueNotifyingHost(.2f);
  require(processor->loadSample(wav.getFile()).isEmpty(),"Import audio successfully");
  require(processor->selectedSlot()==14 && processor->parameters.getRawParameterValue("multi")->load()==0,"Import selects the temporary chromatic buffer");
  require(std::abs(processor->parameters.getRawParameterValue("sampleSpeed")->load()-1.f)<1.e-5f && processor->parameters.getRawParameterValue("start")->load()==0,"Import clears previous pitch and trim");
  juce::AudioBuffer<float> output(2,256);juce::MidiBuffer midi;midi.ensureSize(4096);
  int crossings=0;float previous=0;double energy=0;
  for(int b=0;b<94;++b) {
   output.clear();midi.clear();if(b==0)midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);
   watchAudio=true;processor->processBlock(output,midi);watchAudio=false;
   for(int i=0;i<256;++i)if(b*256+i>=12000 && b*256+i<24000) {
    const float value=output.getSample(0,i);energy+=value*value;
    if(previous<0 && value>=0)++crossings;previous=value;
   }
  }
  require(energy>1,"Imported sample plays immediately from MIDI");
  require(std::abs(crossings-55)<=1,"Imported audio retains its pitch across file sample rates");
  const auto name=processor->sampleName();
  require(processor->loadSample(wav.getFile().withFileExtension("missing")).isNotEmpty() && processor->sampleName()==name,"A failed drop preserves the playable sample");
  require(audioAllocations==0,"Imported sample playback allocates no callback memory");
 }
}
void checkDawRecordingState() {
 struct Listener final : juce::AudioProcessorListener {
  int dirty=0;
  void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {}
  void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& details) override {
   if(details.nonParameterStateChanged)++dirty;
  }
 } listener;
 auto recorder=std::make_unique<NibbiProcessor>();recorder->addListener(&listener);
 recorder->prepareToPlay(48000,256);
 juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;midi.ensureSize(4096);
 auto recordBlocks=[&](int blocks) {
  for(int b=0;b<blocks;++b) {
   midi.clear();
   for(int i=0;i<256;++i)for(int ch=0;ch<2;++ch)audio.setSample(ch,i,.2f*std::sin(float(i+b*256)*.07f));
   watchAudio=true;recorder->processBlock(audio,midi);watchAudio=false;
  }
 };
 require(recorder->recordSample().isEmpty(),"Begin DAW capture");recordBlocks(8);
 juce::MemoryBlock autosave;recorder->getStateInformation(autosave);
 require(recorder->sampleRecording,"Autosave leaves recording running");
 juce::Thread::sleep(60);juce::Timer::callPendingTimersSynchronously();
 require(listener.dirty>0,"Audio changes tell the DAW that its project needs saving");
 recordBlocks(8);
 auto restored=std::make_unique<NibbiProcessor>();
 restored->setStateInformation(autosave.getData(),int(autosave.getSize()));
 juce::MemoryBlock roundtrip;restored->getStateInformation(roundtrip);
 require(autosave==roundtrip,"An in-progress capture saves an immutable PCM prefix");
 restored->parameters.getParameter("slot")->setValueNotifyingHost(1);
 require(restored->sampleName()=="Recorded sample","Autosave contains the new audio, not the previous sample");
 restored.reset();
 recorder->recordSample();audio.clear();midi.clear();recorder->processBlock(audio,midi);
 juce::MemoryBlock finished;recorder->getStateInformation(finished);
 require(finished.getSize()>autosave.getSize(),"Final state contains the rest of the recording");
 juce::TemporaryFile project(".nibbi");
 require(project.getFile().replaceWithData(finished.getData(),finished.getSize()),"Write host state to disk");
 recorder->removeListener(&listener);recorder.reset();
 juce::MemoryBlock disk;require(project.getFile().loadFileAsData(disk),"Read saved project after destroying original instance");
 restored=std::make_unique<NibbiProcessor>();restored->setStateInformation(disk.getData(),int(disk.getSize()));
 restored->prepareToPlay(44100,256);restored->getStateInformation(roundtrip);
 require(roundtrip==finished,"Recorded PCM and controls recall exactly in a fresh instance at a different host rate");
 restored->parameters.getParameter("slot")->setValueNotifyingHost(1);
 double energy=0;
 for(int b=0;b<32;++b) {
  audio.clear();midi.clear();if(b==0)midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);
  watchAudio=true;restored->processBlock(audio,midi);watchAudio=false;
  for(int i=0;i<256;++i)energy+=std::abs(audio.getSample(0,i));
 }
 require(energy>1.e-5,"Recalled recording actually plays from MIDI");
 for(bool queuedOnly:{true,false}) {
  require(restored->recordSample().isEmpty(),"Start a capture before host recall");
  if(!queuedOnly) {audio.clear();midi.clear();restored->processBlock(audio,midi);}
  restored->setStateInformation(disk.getData(),int(disk.getSize()));
  require(!restored->sampleRecording,"Host recall cancels the old capture");
  audio.clear();midi.clear();restored->processBlock(audio,midi);
  restored->getStateInformation(roundtrip);
  require(roundtrip==finished,"Old capture cannot overwrite DAW recall");
 }
 require(audioAllocations==0,"DAW capture and recall add no audio-thread allocations");
}
// AAX exposes its optional input as a mono sidechain. It must behave like
// identical left/right audio on the existing stereo auxiliary input.
void checkMonoAuxInput() {
 auto render=[](bool mono) {
  auto p=std::make_unique<NibbiProcessor>();
  auto layout=p->getBusesLayout();
  layout.inputBuses.set(1,mono?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
  layout.outputBuses.set(1,juce::AudioChannelSet::stereo());
  require(p->setBusesLayout(layout),"Mono and stereo auxiliary layouts are accepted");
  p->prepareToPlay(48000,96);
  std::vector<float> result;
  for(int block=0;block<20;++block) {
   juce::AudioBuffer<float> audio(4,96);audio.clear();juce::MidiBuffer midi;
   auto main=p->getBusBuffer(audio,true,0);
   auto aux=p->getBusBuffer(audio,true,1);
   for(int i=0;i<96;++i) {
    main.setSample(0,i,-.02f);main.setSample(1,i,.03f);
    const float sample=.1f*std::sin(float(block*96+i)*.05f);
    for(int ch=0;ch<aux.getNumChannels();++ch)aux.setSample(ch,i,sample);
   }
   p->processBlock(audio,midi);
   auto headphones=p->getBusBuffer(audio,false,1);
   for(int i=0;i<96;++i)for(int ch=0;ch<2;++ch)result.push_back(headphones.getSample(ch,i));
  }
  return result;
 };
 const auto mono=render(true),stereo=render(false);
 require(mono==stereo,"Mono sidechain reaches both native line-input channels");
 float energy=0.f;for(float sample:mono)energy+=sample*sample;
 require(energy>.001f,"Sidechain comparison contains audible monitored input");
}

int main() { try {
 juce::ScopedJuceInitialiser_GUI gui;
 if(nibbi::factoryBankError().isNotEmpty()) throw std::runtime_error(nibbi::factoryBankError().toStdString());
 for(double rate:{44100.,48000.,96000.})checkMidiPartitions(rate);
 checkNativeMenus();
 checkSampleImport();
 checkDawRecordingState();
 checkMonoAuxInput();
 auto p=std::make_unique<NibbiProcessor>();
 require(p->parameters.getRawParameterValue("output")->load()==.5f,"Startup volume leaves 6 dB of adjustment above the normal listening level");
 p->parameters.getParameter("output")->setValueNotifyingHost(1.f);
 p->prepareToPlay(48000,64);
 require(p->getLatencySamples()==24,"48 kHz reports its native callback buffering");
 juce::AudioBuffer<float> audio(2,257); juce::MidiBuffer midi;
 auto unity=std::make_unique<NibbiProcessor>();
 unity->parameters.getParameter("output")->setValueNotifyingHost(.5f);
 unity->prepareToPlay(48000,64);
 juce::AudioBuffer<float> originalAudio(2,257); juce::MidiBuffer originalMidi;

 double energy=0; float renderedPeak=0;
 for(int b=0;b<120;++b) {
  audio.clear(); midi.clear(); originalAudio.clear(); originalMidi.clear();
  if(!b) { midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),17); originalMidi=midi; }
  p->processBlock(audio,midi); unity->processBlock(originalAudio,originalMidi);
  if(!b) for(int i=0;i<17;++i) require(audio.getSample(0,i)==0,"Sample accurate MIDI onset");
  for(int i=0;i<257;++i) {
   energy+=audio.getSample(0,i)*audio.getSample(0,i);
   renderedPeak=std::max(renderedPeak,std::abs(audio.getSample(0,i)));
   for(int channel=0;channel<2;++channel) {
    require(std::abs(audio.getSample(channel,i))<=nibbi::OutputStage::ceiling+1.e-6f,"Master output stays below full scale");
    if(b>40) require(std::abs(audio.getSample(channel,i)-originalAudio.getSample(channel,i)*2.f)<1.e-3f,"Regular volume doubles the level from half to full without changing the quiet waveform: block "+std::to_string(b)+" full "+std::to_string(audio.getSample(channel,i))+" half "+std::to_string(originalAudio.getSample(channel,i)));
   }
  }
 }
 std::cout<<"Factory A1 at maximum volume: RMS "<<20*std::log10(std::sqrt(energy/(120*257)))
          <<" dBFS, peak "<<20*std::log10(renderedPeak)<<" dBFS\n";
 // Desktop ARM TAPE supplies the original Play+Record chord. Cueing is
 // note-triggered, even with incoming audio or a previously sounding voice.
 uint64_t cueFrames=120*257;
 auto cueBlock=[&] {
  watchAudio=true; unity->processBlock(originalAudio,originalMidi); watchAudio=false;cueFrames+=originalAudio.getNumSamples();
 };
 originalAudio.clear(); originalMidi.clear();
 // Shift + Record uses the same empty-tape cue and consumes its release edge.
 unity->keyboardEvent({20,34,1,1});cueBlock();
 require(unity->loopArmed && !unity->loopRecording,"Shift + Record arms empty tape");
 bool sawRed=false,sawDark=false;
 for(int block=0;block<125;++block) {
  originalAudio.clear();originalMidi.clear();cueBlock();
  const auto rgb=unity->ledColours[8].load()&0xffffffu;
  sawRed|=(rgb&0xff0000u)!=0 && (rgb&0xffffu)==0;sawDark|=rgb==0;
 }
 require(sawRed && sawDark,"Armed tape blinks red while Shift remains held");
 unity->keyboardEvent({22,0,0,0});unity->keyboardEvent({20,34,0,0});cueBlock();
 require(unity->loopArmed,"Releasing Shift before Record does not start or cancel tape");
 unity->keyboardEvent({20,34,1,1});unity->keyboardEvent({20,34,1,0});unity->keyboardEvent({22,0,0,0});cueBlock();
 require(!unity->loopArmed && !unity->loopRecording,"Shift + Record cancels armed tape");
 unity->command(NibbiProcessor::ArmLoop); cueBlock();
 require(unity->loopArmed && !unity->loopRecording && unity->loopLength==0,"Arm waits without recording: armed="+std::to_string(unity->loopArmed.load())+" recording="+std::to_string(unity->loopRecording.load())+" length="+std::to_string(unity->loopLength.load())+" can="+std::to_string(unity->loopCanArm.load()));
 for(int b=0;b<3;++b) {
  for(int i=0;i<257;++i) { originalAudio.setSample(0,i,.4f); originalAudio.setSample(1,i,-.3f); }
  originalMidi.clear(); cueBlock();
  require(unity->loopArmed && unity->loopLength==0,"Incoming audio does not trigger the firmware note cue");
 }
 unity->command(NibbiProcessor::ArmLoop); originalAudio.clear(); cueBlock();
 require(!unity->loopArmed && !unity->loopRecording && unity->loopLength==0,"Arm can be cancelled without recording");
 unity->command(NibbiProcessor::ArmLoop); originalAudio.clear(); cueBlock();
 require(unity->loopArmed,"Empty tape can be armed again");
 const auto cueStart=((cueFrames+37+23)/24)*24;
 originalAudio.clear(); originalMidi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),37); cueBlock();
 require(!unity->loopArmed && unity->loopRecording && unity->loopLength==int((cueFrames/24)*24-cueStart),"Next note starts tape on the next native callback boundary");
 unity->command(NibbiProcessor::PlayLoop); originalAudio.clear(); originalMidi.clear(); cueBlock();
 require(unity->loopPlaying && !unity->loopRecording,"Play closes the cued first recording");
 const auto closedCueLength=unity->loopLength.load();
 unity->command(NibbiProcessor::ArmLoop); originalAudio.clear(); cueBlock();
 require(!unity->loopArmed && unity->loopPlaying && unity->loopLength==closedCueLength,"Arm cannot pause or erase an existing loop");
 unity.reset();
 require(energy>.1,"Playable startup sample");
 require(renderedPeak>.5f,"Regular volume reaches a strong listening level without a second gain control");
 for(double rate:{44100.,48000.,96000.}) {
  nibbi::OutputStage stage; stage.prepare(rate);
  float left=.01f,right=-.005f; stage.processStereo(left,right,0);
  require(std::abs(left-.01f*nibbi::OutputStage::gain)<1.e-7f && std::abs(right+.005f*nibbi::OutputStage::gain)<1.e-7f,"Limiter leaves quiet stereo audio linear");
  for(int sample=0;sample<int(rate*.15);++sample) {
   left=sample<int(rate*.05)?4.f:.01f; right=-left*.5f;
   stage.processStereo(left,right,0);
   require(std::isfinite(left) && std::abs(left)<=nibbi::OutputStage::ceiling+1.e-6f,"Sustained overload and recovery cannot clip");
   require(std::abs(right+left*.5f)<1.e-7f,"Limiter preserves stereo balance");
  }
  float hpL=4.f,hpR=-2.f; stage.processStereo(hpL,hpR,1);
  require(std::abs(hpL)<=nibbi::OutputStage::ceiling+1.e-6f && std::abs(hpR)<=nibbi::OutputStage::ceiling,"Headphone output is also protected");
  stage.prepare(rate); left=.01f;right=0;stage.processStereo(left,right,0);
  require(std::abs(left-.01f*nibbi::OutputStage::gain)<1.e-7f,"Reset clears limiter gain reduction");
  for(float invalid:{std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
   stage.prepare(rate);left=invalid;right=.01f;stage.processStereo(left,right,0);
   require(left==0.f && std::abs(right-.01f*nibbi::OutputStage::gain)<1.e-7f,"Invalid or overflowing audio cannot poison the stereo limiter");
   left=.01f;right=.005f;stage.processStereo(left,right,0);
   require(std::isfinite(left) && std::isfinite(right) && left>0.f && right>0.f,"Normal audio recovers immediately after invalid input");
  }
 }
 p->parameters.getParameter("drive")->setValueNotifyingHost(.7f);
 juce::MemoryBlock state;p->getStateInformation(state);
 // Launch preferences may contain an older instrument session. Only device
 // configuration is passed to JUCE's standalone holder, on every launch.
 juce::PropertySet saved,runtime,nextLaunch;
 saved.setValue("filterState",state.toBase64Encoding());
 saved.setValue("audioSetup","<DEVICESETUP audioOutputDeviceName=\"Test output\"/>");
 saved.setValue("shouldMuteInput",false);
 saved.setValue("lastStateFile","old.nibbi"); saved.setValue("windowX",123);
 nibbi::restoreStandaloneDevices(saved,runtime);
 require(!runtime.containsKey("filterState") && !runtime.containsKey("lastStateFile"),"Standalone never restores instrument or project state");
 require(runtime.getValue("audioSetup")==saved.getValue("audioSetup") && !runtime.getBoolValue("shouldMuteInput",true),"Standalone preserves audio device settings");
 runtime.setValue("filterState",state.toBase64Encoding()); // Even a wrapper save cannot persist it.
 nibbi::saveStandaloneDevices(runtime,saved);
 require(!saved.containsKey("filterState") && !saved.containsKey("windowX"),"Quit removes legacy instrument and window state");
 nibbi::restoreStandaloneDevices(saved,nextLaunch);
 require(!nextLaunch.containsKey("filterState") && nextLaunch.containsKey("audioSetup"),"Reopening starts fresh while retaining devices");
 auto q=std::make_unique<NibbiProcessor>();q->setStateInformation(state.getData(),int(state.getSize()));
 juce::MemoryBlock roundtrip;q->getStateInformation(roundtrip);require(state==roundtrip,"Session embeds samples and parameters exactly");
 {
  juce::MemoryInputStream input(state,false);juce::MemoryBlock legacy;
  juce::MemoryOutputStream output(legacy,false);output.writeInt(input.readInt());
  output.writeString(input.readString().replace("NibbiTape","ChompTape"));
  output.writeFromInputStream(input,-1);
  q->setStateInformation(legacy.getData(),int(legacy.getSize()));
  juce::MemoryBlock migrated;q->getStateInformation(migrated);
  require(migrated==state,"Pre-rename projects migrate with identical samples and parameters");
 }
 q->setStateInformation(state.getData(),int(state.getSize()-5)); juce::MemoryBlock after;q->getStateInformation(after);require(after==roundtrip,"Truncated session rejected transactionally");
 // Invalid host state must leave the existing session untouched.
 auto requireRejectedState=[&](const void* data,int size,const char* message) {
  q->setStateInformation(data,size);
  juce::MemoryBlock unchanged;q->getStateInformation(unchanged);
  require(unchanged==roundtrip,message);
 };
 requireRejectedState(nullptr,0,"Empty host state is ignored");
 requireRejectedState(nullptr,int(state.getSize()),"Null host state is ignored even with a nonzero size");
 requireRejectedState(state.getData(),3,"An incomplete state header is ignored");
 {
  juce::MemoryInputStream input(state,false);juce::MemoryBlock invalid;
  juce::MemoryOutputStream output(invalid,false);output.writeInt(input.readInt());
  auto xml=juce::parseXML(input.readString());
  auto invalidParameters=juce::ValueTree::fromXml(*xml);
  invalidParameters.getChildWithProperty("id","output").setProperty("value",1000.f,nullptr);
  output.writeString(invalidParameters.createXml()->toString());
  output.writeFromInputStream(input,-1);
  requireRejectedState(invalid.getData(),int(invalid.getSize()),"Out-of-range parameters are rejected transactionally");
 }
 q->prepareToPlay(44100,32);require(q->getLatencySamples()==84,"44.1 kHz converter latency");
 juce::TemporaryFile appearanceFile(".settings");
 std::unique_ptr<juce::AudioProcessorEditor> editor(new NibbiEditor(*q,appearanceFile.getFile()));require(editor && editor->getWidth()==1200,"Instrument editor creates");
 auto* connected=dynamic_cast<NibbiEditor*>(editor.get()); require(connected!=nullptr,"Supplied panel adapter is installed");
 auto respondToReset=[](NibbiEditor& target,bool confirm) {
  target.factoryResetButton().onClick();
  auto* prompt=target.panel().getParentComponent()->findChildWithID("factoryResetPrompt");
  require(prompt && prompt->isVisible(),"Factory reset asks for confirmation");
  if(!confirm) {
   auto preview=target.createComponentSnapshot(target.getLocalBounds());
   auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("nibbi-reset-confirmation.png").createOutputStream();
   if(file) {file->setPosition(0);file->truncate();juce::PNGImageFormat().writeImageToStream(preview,*file);}
  }
  auto* choice=dynamic_cast<juce::TextButton*>(prompt->findChildWithID(confirm?"confirmReset":"cancelReset"));
  require(choice!=nullptr,"Reset confirmation offers Reset and Cancel");choice->onClick();
  require(!prompt->isVisible(),"Confirmation closes after either choice");
 };
 juce::MemoryBlock beforeCancel,afterCancel;q->getStateInformation(beforeCancel);
 respondToReset(*connected,false);q->getStateInformation(afterCancel);
 require(beforeCancel==afterCancel,"Cancelling factory reset preserves the instrument");
 auto* bar=connected->panel().getParentComponent()->findChildWithID("titleBar");
 require(bar && connected->factoryResetButton().getParentComponent()==bar,"Utilities belong to the title bar");
 auto* backgrounds=dynamic_cast<juce::TextButton*>(bar->findChildWithID("background"));
 require(backgrounds!=nullptr,"Title bar has a background selector");
 for(auto* child:bar->getChildren())if(auto* button=dynamic_cast<juce::Button*>(child))
  require(button->getTooltip().isNotEmpty(),"Every toolbar action has a tooltip");
 for(int i=1;i<=nibbi::gui::NibbiPanel::backgroundCount;++i) {
  backgrounds->onClick();require(connected->panel().getBackground()==i%nibbi::gui::NibbiPanel::backgroundCount,"Background button cycles black, pink and all three faceplates");
  if(i<5) {
   auto preview=editor->createComponentSnapshot(editor->getLocalBounds());
   auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(i==1?"nibbi-pink-preview.png":"nibbi-faceplate-"+juce::String(i-1)+"-preview.png").createOutputStream();
   if(file) {file->setPosition(0);file->truncate();juce::PNGImageFormat().writeImageToStream(preview,*file);}
  }
 }
 {
  backgrounds->onClick(); // Pink, saved immediately rather than only on quit.
  nibbi::gui::AppearancePreferences reopenedPreferences(appearanceFile.getFile());
  require(reopenedPreferences.skin(nibbi::gui::NibbiPanel::backgroundCount)==1,"Skin preference is persisted to disk");
  auto freshProcessor=std::make_unique<NibbiProcessor>();
  NibbiEditor reopened(*freshProcessor,appearanceFile.getFile());
  require(reopened.panel().getBackground()==1,"A new instance restores the selected skin");
  respondToReset(reopened,true);
  require(reopened.panel().getBackground()==1 && reopenedPreferences.skin(5)==1,"Factory reset preserves appearance");
  for(int i=0;i<4;++i)backgrounds->onClick(); // Restore black for the remaining previews.
 }
 require(connected->panel().getWidth()*715==connected->panel().getHeight()*2200,"Title bar preserves artwork proportions");
 auto flushControls=[&] {juce::AudioBuffer<float> block(2,96);block.clear();juce::MidiBuffer events;q->processBlock(block,events);};
 // The manual's first two Shift keys select the engine and cycle its bank.
 auto* chromatic=dynamic_cast<juce::Button*>(connected->panel().getComponentAt(116,235));
 auto* drums=dynamic_cast<juce::Button*>(connected->panel().getComponentAt(191,235));
 require(chromatic && drums,"Raised mode keys remain clickable");
 connected->modifierKeysChanged(juce::ModifierKeys(juce::ModifierKeys::shiftModifier));
 drums->setState(juce::Button::buttonDown); drums->setState(juce::Button::buttonNormal);flushControls();
 require(q->parameters.getRawParameterValue("multi")->load()==1.f,"Shift plus the second raised key selects HITKIT");
 require(q->parameters.getRawParameterValue("multi")->load()==1.f,"Selected engine survives mouse release");

 drums->setState(juce::Button::buttonDown); drums->setState(juce::Button::buttonNormal);flushControls();
 require(q->parameters.getRawParameterValue("bank")->load()==1.f,"Repeating the selected mode key advances the bank and indicator colour");
 connected->modifierKeysChanged(juce::ModifierKeys());
 require(q->parameters.getRawParameterValue("multi")->load()==1.f,"Engine selection survives releasing Shift");
 {
  auto preview=editor->createComponentSnapshot(editor->getLocalBounds());
  auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("nibbi-drums-preview.png").createOutputStream();
  if(file) { file->setPosition(0);file->truncate();juce::PNGImageFormat().writeImageToStream(preview,*file); }
 }
 connected->modifierKeysChanged(juce::ModifierKeys(juce::ModifierKeys::shiftModifier));
 chromatic->setState(juce::Button::buttonDown); chromatic->setState(juce::Button::buttonNormal);flushControls();
 require(q->parameters.getRawParameterValue("multi")->load()==0.f && q->parameters.getRawParameterValue("bank")->load()==0.f,"First mode key restores the chromatic engine's own bank");

 connected->modifierKeysChanged(juce::ModifierKeys());
 auto& modeSwitch=connected->panel().modeSwitch();
 modeSwitch.setToggleState(true,juce::dontSendNotification); modeSwitch.onClick();
 require(q->parameters.getRawParameterValue("modeSwitch")->load()==1.f,"Down switch selects Shift mode");
 modeSwitch.setToggleState(false,juce::dontSendNotification); modeSwitch.onClick();
 require(q->parameters.getRawParameterValue("modeSwitch")->load()==0.f,"Up switch selects sampling mode");
 connected->panel().dial(nibbi::gui::NibbiPanel::Dial::pitch).setValue(-.5,juce::sendNotificationSync);
 require(std::abs(q->parameters.getRawParameterValue("sampleSpeed")->load()+.5f)<1.e-5f,"Panel speed controls forward/reverse sample playback");
 q->parameters.getParameter("space")->setValueNotifyingHost(.65f);
 require(std::abs(connected->panel().dial(nibbi::gui::NibbiPanel::Dial::effect).getValue()-.65)<1.e-5,"Host automation updates supplied effect dial");
 connected->panel().dial(nibbi::gui::NibbiPanel::Dial::loop).setValue(-.5,juce::sendNotificationSync);
 require(q->parameters.getRawParameterValue("loopSpeed")->load()==-.5f,"Tape wheel selects reverse speed");
 connected->panel().dial(nibbi::gui::NibbiPanel::Dial::loop).onPress();flushControls();
 require(q->parameters.getRawParameterValue("loopSpeed")->load()==1.f,"Tape wheel press restores forward 1x");
 const auto originalOutput=q->parameters.getRawParameterValue("output")->load();
 connected->modifierKeysChanged(juce::ModifierKeys(juce::ModifierKeys::shiftModifier));
 connected->panel().dial(nibbi::gui::NibbiPanel::Dial::input).setValue(.4,juce::sendNotificationSync);
 require(std::abs(q->parameters.getRawParameterValue("compression")->load()-.4f)<1.e-5f,"Keyboard Shift + Volume controls compression");
 require(q->parameters.getRawParameterValue("output")->load()==originalOutput,"Keyboard Shift preserves normal volume");
 connected->modifierKeysChanged(juce::ModifierKeys());
 connected->panel().dial(nibbi::gui::NibbiPanel::Dial::input).setValue(.7,juce::sendNotificationSync);
 require(std::abs(q->parameters.getRawParameterValue("output")->load()-.7f)<1.e-5f,"Releasing keyboard Shift restores normal volume");
 require(std::abs(q->parameters.getRawParameterValue("compression")->load()-.4f)<1.e-5f,"Releasing keyboard Shift retains compression setting");
 // A sample capture is available to immediate session saves, without waiting for a GUI timer.
 p->recordSample();
 for(int b=0;b<4;++b) { for(int i=0;i<257;++i) {audio.setSample(0,i,.2f);audio.setSample(1,i,-.1f);} midi.clear();p->processBlock(audio,midi); }
 p->recordSample();audio.clear();p->processBlock(audio,midi);
 juce::MemoryBlock captured;p->getStateInformation(captured);
 require(p->sampleName()=="Recorded sample","Completed capture is published before saving");
 // Tape survives session restore both before the first callback and after re-preparation.
 p->parameters.getParameter("routing")->setValueNotifyingHost(.5f);
 p->command(NibbiProcessor::RecordLoop);
 for(int b=0;b<8;++b) {for(int i=0;i<257;++i) {audio.setSample(0,i,.2f*std::sin(float(i+b*257)*.05f));audio.setSample(1,i,0);}p->processBlock(audio,midi);}
 p->command(NibbiProcessor::PlayLoop);audio.clear();p->processBlock(audio,midi);
 const int recordedFrames=p->loopLength.load();
 require(std::abs(recordedFrames-8*257)<=24 && recordedFrames%24==0,"Tape length follows 24-frame callbacks");
 juce::MemoryBlock tapeState;p->getStateInformation(tapeState);
 q->setStateInformation(tapeState.getData(),int(tapeState.getSize()));
 q->prepareToPlay(48000,32);juce::MemoryBlock beforeProcess;q->getStateInformation(beforeProcess);
 require(beforeProcess==tapeState,"Preparing doesn't discard a pending restored loop");
 audio.clear();q->processBlock(audio,midi);require(q->loopLength==recordedFrames,"Session restores recorded tape");
 q->prepareToPlay(96000,512);audio.clear();q->processBlock(audio,midi);require(q->loopLength==recordedFrames,"Buffer/rate changes preserve tape");
 // Relative encoders must keep visible motion and a live readout even though
 // they intentionally bypass Slider's absolute-position mouse handling.
 for(auto id:{nibbi::gui::NibbiPanel::Dial::loop,nibbi::gui::NibbiPanel::Dial::input}) {
  auto& encoder=connected->panel().dial(id);
  const float angle=encoder.getProperties()["thumbAngle"];
  encoder.turnRelative(12.f);flushControls();encoder.onActivity();
  require(float(encoder.getProperties()["thumbAngle"])!=angle,"Every encoder visibly rotates on relative turns");
  auto* readout=dynamic_cast<juce::Label*>(connected->panel().findChildWithID("encoderValue"));
  require(readout && readout->isVisible() && readout->getText()==encoder.readoutText(),"Relative turns show the current audio control value");
 }
 auto image=editor->createComponentSnapshot(editor->getLocalBounds());require(image.isValid(),"Editor paints");
 auto output=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("nibbi-preview.png").createOutputStream();
 if(output) { output->setPosition(0); output->truncate(); juce::PNGImageFormat().writeImageToStream(image,*output); }
 // Load/restore complete catalogs while audio is running. Reclamation and
 // session serialization stay on this thread; callbacks may neither allocate
 // nor destroy catalog/sample storage.
 std::atomic<bool> running{true},finite{true};
 std::thread audioThread([&] {
  juce::AudioBuffer<float> block(2,37);juce::MidiBuffer events;events.ensureSize(4096);
  while(running.load()) { block.clear();events.clear();events.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);
   watchAudio=true;q->processBlock(block,events);watchAudio=false;
   for(int i=0;i<37;++i) if(!std::isfinite(block.getSample(0,i))) finite=false;
  }
 });
 for(int i=0;i<8;++i) { q->setStateInformation(tapeState.getData(),int(tapeState.getSize()));juce::MemoryBlock copy;q->getStateInformation(copy); }
 running=false;audioThread.join();require(finite,"Concurrent restores produce finite audio");require(audioAllocations==0,"Plugin callback performs no allocation or deallocation");
 // Factory reset must discard an active capture and old tape/effects, while
 // keeping explicit project and DAW recall available through setStateInformation.
 q->recordSample(); audio.clear(); midi.clear(); q->processBlock(audio,midi);
 require(q->sampleRecording,"Reset test has an active capture");
 respondToReset(*connected,true);
 auto fresh=std::make_unique<NibbiProcessor>();
 juce::MemoryBlock expectedFactory,resetState;
 fresh->getStateInformation(expectedFactory); q->getStateInformation(resetState);
 require(resetState==expectedFactory,"Factory reset restores samples, tape and every parameter");
 require(!q->sampleRecording && !q->loopPlaying && q->loopLength==0,"Factory reset clears transport and capture");
 audio.clear(); midi.clear(); watchAudio=true; q->processBlock(audio,midi); watchAudio=false;
 for(int i=0;i<audio.getNumSamples();++i) for(int ch=0;ch<2;++ch)
  require(audio.getSample(ch,i)==0,"Factory reset removes sounding notes and effect tails");
 q->getStateInformation(resetState); require(resetState==expectedFactory,"Old capture cannot republish after reset");
 q->recordSample(); respondToReset(*connected,true);
 q->getStateInformation(resetState); require(resetState==expectedFactory,"Factory reset discards a capture before its first callback");
 require(audioAllocations==0,"First callback after reset does not allocate");
 std::cout<<"Plugin: MIDI timing, embedded sample/session recall, corrupt-state rejection, latency and editor passed\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
