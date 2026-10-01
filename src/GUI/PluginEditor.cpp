#include "PluginEditor.h"
#if JucePlugin_Build_Standalone
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif
NibbiEditor::NibbiEditor(NibbiProcessor& p,juce::File preferencesFile):AudioProcessorEditor(p),proc_(p),preferences_(std::move(preferencesFile)) {
#if JucePlugin_Build_Standalone
 if(auto* holder=juce::StandalonePluginHolder::getInstance()) holder->getMuteInputValue()=false;
#endif
 addAndMakeVisible(surface_); surface_.addAndMakeVisible(panel_);
 surface_.setInterceptsMouseClicks(false,true);
 surface_.addAndMakeVisible(titleBar_);
 for(auto* b:{&factoryReset_,&clear_,&arm_,&background_,&legends_,&help_}) {
  titleBar_.addAndMakeVisible(*b);
 }
 help_.setComponentID("help");
 help_.setTooltip("Help — open the nibbi help page in your browser");
 help_.onClick=[this] {
  if(!juce::URL("https://temeculadsp.com/nibbi#help").launchInDefaultBrowser()) {
   status_="Open https://temeculadsp.com/nibbi#help for help.";timerCallback();
  }
 };
 legends_.setComponentID("legends");legends_.setClickingTogglesState(true);
 legends_.setTooltip("Show or hide control labels");
 legends_.onClick=[this] {panel_.setLegendsVisible(legends_.getToggleState());timerCallback();};
 panel_.setBackground(preferences_.skin(nibbi::gui::NibbiPanel::backgroundCount));
 background_.setComponentID("background");updateSkinTooltip();
 background_.onClick=[this] {
  panel_.setBackground((panel_.getBackground()+1)%nibbi::gui::NibbiPanel::backgroundCount);
  updateSkinTooltip();
  if(!preferences_.setSkin(panel_.getBackground())) {status_="Could not save skin preference.";timerCallback();}
 };
 titleBar_.addAndMakeVisible(tapeStatus_);
 for(auto* label:{&statusLabel_}) {
  surface_.addAndMakeVisible(*label); label->setInterceptsMouseClicks(false,false);
  label->setColour(juce::Label::textColourId,juce::Colour(0xffeee9dd));
  label->setColour(juce::Label::backgroundColourId,juce::Colours::black);
  label->setFont(juce::FontOptions(12.f));
 }
 panel_.recordButton().setClickingTogglesState(false);
 panel_.recordButton().onStateChange=[this] {
  const bool down=panel_.recordButton().isDown();if(down==recordDown_)return;recordDown_=down;
  if(down&&!shift_&&!proc_.shiftMenuActive.load()&&value("modeSwitch")<.5f&&!proc_.sampleRecording.load()) {
   status_=proc_.recordSample(false);
   if(status_.isNotEmpty())return;
  }
  updateShift();
  proc_.keyboardEvent({20,5,uint8_t(shift_),down?1.f:0.f});
 };
 panel_.playButton().setClickingTogglesState(false);
 panel_.playButton().onStateChange=[this] {
  const bool down=panel_.playButton().isDown();if(down==playDown_)return;playDown_=down;
  proc_.keyboardEvent({20,33,uint8_t(shift_),down?1.f:0.f});
 };
 panel_.reverseButton().setClickingTogglesState(false);
 panel_.reverseButton().onStateChange=[this] {
  const bool down=panel_.reverseButton().isDown();if(down==loopDown_)return;loopDown_=down;
  proc_.keyboardEvent({20,34,uint8_t(shift_),down?1.f:0.f});
 };
 panel_.playButton().setTooltip("Play / pause. Hold while paused to rewind.");
 panel_.reverseButton().setTooltip("Tape record / overdub. Shift + Record arms an empty tape; repeat to cancel. Hold with Play to clear.");
 panel_.modeSwitch().onClick=[this] {
  setParameter("modeSwitch",panel_.modeSwitch().getToggleState()?1.f:0.f);updateShift();
 };
 clear_.onClick=[this] { setParameter("loopSpeed",1); proc_.command(NibbiProcessor::ClearLoop); };
 clear_.setComponentID("eraseLoop");clear_.setTooltip("Erase the tape loop");
 arm_.setComponentID("armTape");factoryReset_.setComponentID("factoryReset");
 arm_.onClick=[this] { proc_.command(NibbiProcessor::ArmLoop); };
 arm_.setTooltip("Arm empty tape: recording starts on your next played note. Click again to cancel.");
 factoryReset_.setTooltip("Factory reset: restore samples and controls; clear tape, recordings and effect tails. Keep your skin preference.");
 surface_.addChildComponent(resetPrompt_);
 factoryReset_.onClick=[this] {resetPrompt_.open();};
 resetPrompt_.onConfirm=[this] {
  loader_.removeAllJobs(true,-1);
  panel_.releaseNotes();
  proc_.resetFactoryState();
  pages_.fill(0);
  recordDown_=playDown_=loopDown_=shift_=false;
  updateShift(); bindDials();
  status_="Factory state restored."; timerCallback();
 };
 keys_.addListener(this);
 panel_.sampleModeLight().onClick=[this] { setParameter("recordLatch",value("recordLatch")>.5f?0.f:1.f); };
 panel_.onPageLightClick=[this](size_t i) { press(i); };
 panel_.onKeyDown=[this](size_t i) { return keyDown(i); };
 panel_.onBufferDown=[this] { return bufferDown(); };
 bindDials();
 setResizable(true,true); setResizeLimits(840,298,1800,639);
 getConstrainer()->setFixedAspectRatio(double(nibbi::gui::NibbiPanel::designWidth)/(nibbi::gui::NibbiPanel::designHeight+nibbi::gui::TitleBar::height));
 setSize(nibbi::gui::NibbiPanel::designWidth,nibbi::gui::NibbiPanel::designHeight+nibbi::gui::TitleBar::height); timerCallback(); startTimerHz(30);
}
NibbiEditor::~NibbiEditor() {
 resetPrompt_.dismiss();
 stopTimer();loader_.removeAllJobs(true,-1);
 panel_.recordButton().onStateChange=nullptr;
 panel_.playButton().onStateChange=nullptr;
 panel_.reverseButton().onStateChange=nullptr;
 panel_.modeSwitch().onClick=nullptr;
 panel_.onKeyDown=nullptr; panel_.onBufferDown=nullptr; panel_.onPageLightClick=nullptr;
 if(playDown_) proc_.keyboardEvent({20,33,uint8_t(shift_),0});
 if(loopDown_) proc_.keyboardEvent({20,34,uint8_t(shift_),0});
 if(shift_) proc_.keyboardEvent({22,0,0,0});
 if(recordDown_ && value("modeSwitch")<.5f && value("recordLatch")<.5f) proc_.command(NibbiProcessor::StopSample);
 panel_.releaseNotes();keys_.removeListener(this);
}
void NibbiEditor::resized() {
 constexpr auto width=nibbi::gui::NibbiPanel::designWidth;
 constexpr auto barHeight=nibbi::gui::TitleBar::height;
 constexpr auto height=nibbi::gui::NibbiPanel::designHeight+barHeight;
 const float scale=juce::jmin(float(getWidth())/width,float(getHeight())/height);
 surface_.setBounds(0,0,width,height);
 surface_.setTransform(juce::AffineTransform::scale(scale).translated((getWidth()-width*scale)*.5f,(getHeight()-height*scale)*.5f));
 titleBar_.setBounds(0,0,width,barHeight);
 panel_.setBounds(0,barHeight,width,nibbi::gui::NibbiPanel::designHeight);
 constexpr int y=6,h=24;
 background_.setBounds(10,y,26,h);legends_.setBounds(44,y,26,h);
 factoryReset_.setBounds(78,y,26,h);
 help_.setBounds(width-36,y,26,h);
 arm_.setBounds(808,y,26,h);clear_.setBounds(842,y,26,h);
 tapeStatus_.setBounds(884,y,190,h);
 statusLabel_.setBounds(18,barHeight+374,1164,16);
 resetPrompt_.setBounds(0,0,width,height);
}
void NibbiEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xffe4e1d6)); }
void NibbiEditor::updateSkinTooltip() {
 background_.setTooltip("Change skin (current: "+panel_.backgroundName()+"). Cycles Black, Pink and Faceplates 1–3. Remembered for next time.");
}
void NibbiEditor::modifierKeysChanged(const juce::ModifierKeys& modifiers) {
 keyboardShift_=modifiers.isShiftDown();
 updateShift();
}
void NibbiEditor::updateShift() {
 const bool active=keyboardShift_ || (recordDown_ && value("modeSwitch")>.5f);
 if(shift_==active) return;
 shift_=active;
 proc_.keyboardEvent({22,0,0,active?1.f:0.f});
 bindDials();
}
float NibbiEditor::value(const char* id) const { return proc_.parameters.getRawParameterValue(id)->load(); }
void NibbiEditor::setParameter(const char* id,float v) {
 auto* p=proc_.parameters.getParameter(id);
 p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture();
}
void NibbiEditor::bindDials() {
 attachments_.clear();
 const char* ids[6]={pages_[0]?"gain":"sampleSpeed",pages_[1]?"attack":"start",pages_[2]?"release":"end",
  pages_[3]==0?"space":pages_[3]==1?"drive":"filter","loopSpeed",pages_[5]?"input":"output"};
 if(shift_) { if(pages_[0]) ids[0]="pan"; ids[3]=pages_[3]==0?"time":pages_[3]==1?"warble":"resonance"; ids[5]="compression"; }
 for(size_t i=0;i<6;++i) {
  auto id=static_cast<nibbi::gui::NibbiPanel::Dial>(i);
  auto& dial=panel_.dial(id);
  attachments_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.parameters,ids[i],dial));
  auto* parameter=proc_.parameters.getParameter(ids[i]);
  auto name=parameter->getName(40);
  if(i==5 && !pages_[5] && !shift_) name="VOLUME";
  if(i==0 && !pages_[0]) name=shift_?"STEPPED SPEED":"SAMPLE SPEED";
  if((i==1 || i==2) && shift_) name=pages_[i]?"ATTACK + RELEASE":"MOVE SAMPLE WINDOW";
  if(i==4) name=shift_?"STEPPED TAPE":"TAPE SPEED / SCRUB";
  panel_.setDialLabel(id,name.toUpperCase());
  dial.setTooltip(name+" — drag or scroll to turn; click the encoder or its light to change function");
  dial.setDoubleClickReturnValue(false, 0.0);
  dial.readoutText=[this,parameter,i,id=juce::String(ids[i])] {
   if(i==4 && !proc_.loopPlaying.load() && !shift_)
    return juce::String(proc_.loopPosition.load()*100.f,1)+"% position";
   const float v=parameter->convertFrom0to1(parameter->getValue());
   if(id=="sampleSpeed" || id=="loopSpeed" || id=="gain")return juce::String(v,2)+juce::String::fromUTF8(" ×");
   if(id=="attack" || id=="release")return v<1.f?juce::String(v*1000.f,0)+" ms":juce::String(v,2)+" s";
   if(id=="pan")return juce::String(v*100.f,0)+"% right";
   return juce::String(v*100.f,1)+"%";
  };
  dial.onPress=[this,i] { press(i); };
  dial.onTurn=[this,i](float delta) { return turn(i,delta); };
  if(i<6 && i!=4) panel_.setPageLight(i,pages_[i]);
 }
}
void NibbiEditor::press(size_t i) {
 static constexpr uint8_t ids[]={3,0,1,2,4,32};
 if(i>=6)return;
 proc_.keyboardEvent({20,ids[i],uint8_t(shift_),1});
 proc_.keyboardEvent({20,ids[i],uint8_t(shift_),0});
}
bool NibbiEditor::keyDown(size_t i) {
 if(!shift_)return false;
 static constexpr uint8_t ids[]={15,8,9,10,11,16,17,18,19,20,24,25,26,27,7,12,13,14,21,22,23,29,30,31};
 if(i>=24)return true;
 proc_.keyboardEvent({20,ids[i],1,1});proc_.keyboardEvent({20,ids[i],1,0});
 return true;
}
void NibbiEditor::updateMenuLights() {
 std::array<uint32_t,35> colours;
 for(size_t i=0;i<colours.size();++i)colours[i]=proc_.ledColours[i].load(std::memory_order_relaxed);
 // Keep the requested mode/input/FX selections visible outside the Shift menu.
 if(!proc_.shiftMenuActive.load()) {
  const auto bank=int(value("bank"));
  const float* bankColours[]={nibbi::control::purple,nibbi::control::orange,nibbi::control::teal,nibbi::control::dark_orange,nibbi::control::yellow_green};
  const auto* c=bankColours[juce::jlimit(0,4,bank)];
  const int mode=value("multi")>.5f?1:0;
  colours[10+mode]=nibbi::control::packLed({c[0],c[1],c[2]});
  const int source=int(value("inputSource"));
  colours[10+(source==1?2:source==0?3:4)]=nibbi::control::packLed({1.f,.36f*.7f,.62f*.7f});
  colours[10+(value("fxPre")>.5f?5:6)]=nibbi::control::packLed({1.f,.95f,.05f});
 }
 panel_.setLights(colours);
}
bool NibbiEditor::bufferDown() {
 if(!shift_)return false;
 proc_.keyboardEvent({20,28,1,1});proc_.keyboardEvent({20,28,1,0});return true;
}
bool NibbiEditor::turn(size_t i,float delta) {
 proc_.keyboardEvent({21,uint8_t(i),uint8_t(shift_),delta*.25f});return true;
}
void NibbiEditor::timerCallback() {
 // Poll as well as handling modifier events: hosts may consume key events,
 // and releasing Shift outside this editor must not leave the controls shifted.
 keyboardShift_=juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
 updateShift();
 bool pagesChanged=false;
 for(size_t i=0;i<6;++i) {const int page=proc_.encoderPage(i);pagesChanged|=pages_[i]!=page;pages_[i]=page;}
 if(pagesChanged)bindDials();
 panel_.recordButton().setToggleState(proc_.sampleRecording.load(),juce::dontSendNotification);
 panel_.recordButton().setEnabled(true);
 panel_.recordButton().setTooltip(value("modeSwitch")>.5f?"Hold for Shift; press again to confirm a pending preset":"Hold to sample the selected input; release to finish");
 panel_.setSampleModeLight(value("recordLatch")>.5f);
 if(status_=="Finishing sample..." && !proc_.sampleRecording.load()) status_="Sample recorded.";
 panel_.playButton().setToggleState(proc_.loopPlaying.load(),juce::dontSendNotification);
 panel_.reverseButton().setToggleState(proc_.loopRecording.load(),juce::dontSendNotification);
 panel_.setSamplerMode(value("modeSwitch")>.5f);
 updateMenuLights();
 const bool armed=proc_.loopArmed.load();
 arm_.setEnabled(armed || proc_.loopCanArm.load());
 arm_.setName(armed?"Cancel tape arm":"Arm tape");
 arm_.setTooltip(armed?"Cancel tape arm":"Arm empty tape: recording starts on your next played note");
 arm_.setToggleState(armed,juce::dontSendNotification);
 tapeStatus_.update(armed,proc_.loopRecording.load(),proc_.loopPlaying.load(),proc_.loopLength.load());
 const int pending=proc_.pendingAction.load();
 const juce::String menuStatus=pending==0?"Erase: select a sample, then press the sample button to confirm.":pending==1?"Copy: select the source and destination, then press the sample button to confirm.":pending==2?"Save buffer: select a destination, then press the sample button to confirm.":"";
 const auto error=proc_.factoryError();
 statusLabel_.setVisible(panel_.legendsVisible() || error.isNotEmpty() || menuStatus.isNotEmpty() || status_.isNotEmpty());
 statusLabel_.setText(error.isNotEmpty()?error:menuStatus.isNotEmpty()?menuStatus:status_.isNotEmpty()?status_:"Hold keyboard Shift for banks, samples and alternate controls. Drop an audio file to load it.",juce::dontSendNotification);
 syncingKeys_=true;
 for(size_t note=0;note<128;++note) {
  bool on=proc_.displayNotes[note].load();
  if(on!=externalNotes_[note]) { externalNotes_[note]=on; if(on) keys_.noteOn(1,int(note),.8f); else keys_.noteOff(1,int(note),0); }
 }
 syncingKeys_=false;

}
void NibbiEditor::handleNoteOn(juce::MidiKeyboardState*,int channel,int note,float velocity) { if(!syncingKeys_) proc_.keyboardEvent({1,uint8_t(note),uint8_t(channel),velocity}); }
void NibbiEditor::handleNoteOff(juce::MidiKeyboardState*,int channel,int note,float) { if(!syncingKeys_) proc_.keyboardEvent({2,uint8_t(note),uint8_t(channel)}); }
void NibbiEditor::fileDragEnter(const juce::StringArray&,int,int) {
 statusLabel_.setText("Drop an audio file to load it, then play the keys or MIDI.",juce::dontSendNotification);
}
void NibbiEditor::fileDragExit(const juce::StringArray&) {timerCallback();}
void NibbiEditor::filesDropped(const juce::StringArray& files,int,int) { if(files.size()) load(juce::File(files[0])); }
void NibbiEditor::load(const juce::File& file) {
 status_="Loading "+file.getFileName()+"...";statusLabel_.setText(status_,juce::dontSendNotification);
 auto safe=juce::Component::SafePointer<NibbiEditor>(this);auto* processor=&proc_;
 loader_.addJob([safe,processor,file] {
  const auto result=processor->loadSample(file);
  juce::MessageManager::callAsync([safe,result,file] {
   if(safe) {
    safe->status_=result.isEmpty()?"Ready: "+file.getFileNameWithoutExtension()+" — play the keys or MIDI.":result;
    safe->timerCallback();
   }
  });
 });
}
