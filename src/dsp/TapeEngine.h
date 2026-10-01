#pragma once
#include "Types.h"
#include "../control/ControlRuntime.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace nibbi {
// No audio algorithm lives here. This adapts the firmware Engine's RAM/SD,
// clock, control, and four-channel callback contracts to the desktop host.
class TapeEngine {
public:
 TapeEngine();
 void setControls(const Controls&);
 void controlEvent(int type,int id,float value,bool shifted);
 bool takeControlState(Controls& state);
 control::ControlEngine& controllerTarget() {return *engine_;}
 control::ControlRuntime& controller() {return *controller_;}
 const control::ControlRuntime& controller() const {return *controller_;}
 void installCatalog(const Catalog*);
 void repointCatalog(const Catalog*);
 bool hasCatalog() const { return catalog_!=nullptr; }
 void installLoop(LoopStorage*);
 void noteOn(int,float,int,const Catalog&);
 void noteOff(int,int);
 void allNotesOff();
 void reserveCapture(Capture*);
 void startCapture(Capture*);
 void stopCapture();
 void loopRecord();
 void loopPlay();
 void toggleLoopArm();
 void clearLoop();
 void loopRecordButton(bool down) { engine_->LooperRecordButton(down); }
 void loopPlayButton(bool down) { engine_->LooperPlayButton(down); }
 void scrub(float turns) { engine_->SetLooperScrub(turns); }
 static constexpr size_t blockSize=24;
 void processBlock(const Frame*,Frame*);
 Frame process(Frame);
 bool loopRecording() { return engine_->DesktopLoopRecording(); }
 bool loopPlaying() { return engine_->DesktopLoopPlaying(); }
 bool loopArmed() { return engine_->GetLooperRecordArm(); }
 bool canArmLoop() { return engine_->DesktopCanArmLoop(); }
 size_t loopFrames() const { return engine_->DesktopLoopEmpty()?0:loop_->memory.length.load()/2; }
 float loopPosition() { return engine_->GetLooperPosition(); }
 std::vector<int16_t> copyLoop() const;
 bool hasLoop(const LoopStorage* storage) const { return snapshotLoop_.load()==storage; }
private:
 static daisy::Engine::DesktopSampleView lookup(const void*,int,int,int);
 static int keyForNote(int);
 std::unique_ptr<control::ControlEngine> engine_;
 std::unique_ptr<control::ControlRuntime> controller_;
 std::unique_ptr<daisysp::Reverb> reverb_;
 std::vector<nibbi::InterpolatedDelayLine::AudioSample> delay_;
 std::vector<daisy::AtomicSample> bufferPcm_;
 daisy::RamBufferMemory bufferMemory_{};
 std::unique_ptr<LoopStorage> defaultLoop_;
 LoopStorage* loop_=nullptr;
 std::atomic<LoopStorage*> snapshotLoop_{nullptr};
 std::atomic<bool> snapshotEmpty_{true};
 const Catalog* catalog_=nullptr;
 Capture* capture_=nullptr;
 Controls controls_{};
 int appliedMode_=-1,appliedBank_=-1,appliedSlot_=-1;
 uint64_t processedFrames_=0;
 std::array<Frame,blockSize> inputBlock_{},outputBlock_{};
 size_t blockPosition_=0;
 bool controlsChanged_=false;
 std::array<float,6> turnRemainder_{};
 void finishCapture();
};
}
