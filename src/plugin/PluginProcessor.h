#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "../dsp/HostAdapter.h"
#include <mutex>
#include "OutputStage.h"
class NibbiProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
 NibbiProcessor();
 ~NibbiProcessor() override;
 void prepareToPlay(double,int) override;
 void releaseResources() override {}
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override { return true; }
 const juce::String getName() const override { return JucePlugin_Name; }
 bool acceptsMidi() const override { return true; }
 bool producesMidi() const override { return true; }
 double getTailLengthSeconds() const override { return 30; }
 int getNumPrograms() override { return 1; }
 int getCurrentProgram() override { return 0; }
 void setCurrentProgram(int) override {}
 const juce::String getProgramName(int) override { return "TAPE"; }
 void changeProgramName(int,const juce::String&) override {}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;
 juce::AudioProcessorValueTreeState parameters;
 // Default import loads/selects the temporary chromatic buffer. Decode off the audio thread.
 juce::String loadSample(const juce::File&, int slot = -1);
 juce::String sampleName();
 void resetFactoryState();
 enum class PresetAction { erase, copy, saveBuffer };
 juce::String editPreset(PresetAction action, int source, int target);
 juce::String factoryError() const;
 int selectedSlot() const;
 juce::String recordSample(bool startImmediately=true);
 void command(unsigned bit) { commands_.fetch_or(bit,std::memory_order_release); }
 void keyboardEvent(nibbi::Event e);
 int encoderPage(size_t index) const {return encoderPages_[index].load();}
 std::array<std::atomic<uint32_t>,35> ledColours{};
 std::atomic<int> pendingAction{-1};
 std::atomic<bool> shiftMenuActive{false};
 std::atomic<bool> sampleRecording{false},loopRecording{false},loopPlaying{false};
 std::atomic<bool> loopArmed{false},loopCanArm{false};
 std::atomic<float> loopPosition{0},peak{0};
 std::atomic<int> loopLength{0};
 std::atomic<uint64_t> revision{1};
 std::array<std::atomic<bool>,128> displayNotes{};
 enum { StopSample=1, RecordLoop=2, PlayLoop=4, ClearLoop=8, Panic=16, ArmLoop=32 };
private:
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
 nibbi::Controls controls() const;
 void syncControlState(const nibbi::Controls&);
 void timerCallback() override;
 void serviceState();
 void publish(std::unique_ptr<nibbi::Catalog>);
 const nibbi::Catalog& acquireCatalog();
 std::mutex catalogMutex_;
 std::vector<std::unique_ptr<nibbi::Catalog>> catalogs_;
 std::atomic<const nibbi::Catalog*> published_{nullptr},hazard_{nullptr},candidateHazard_{nullptr};
 const nibbi::Catalog* audioCatalog_=nullptr;
 std::unique_ptr<nibbi::HostAdapter> host_;
 std::shared_ptr<nibbi::LoopStorage> loopLease_;
 std::unique_ptr<nibbi::Capture> capture_;
 std::shared_ptr<const nibbi::Sample> completedCaptureLease_;
 std::atomic<nibbi::Capture*> pendingCapture_{nullptr};
 std::atomic<unsigned> commands_{0};
 std::atomic<bool> stateDirty_{false};
 juce::AbstractFifo keys_{256};
 std::array<nibbi::Event,256> keyData_{};
 std::array<std::atomic<int>,6> encoderPages_{};
 std::array<bool,2> midiTransportDown_{};
 juce::MidiBuffer outgoing_;
 std::mutex serviceMutex_;
 juce::AudioFormatManager formats_;
 std::array<std::atomic<float>*,40> values_{};
 std::array<juce::RangedAudioParameter*,40> parameterObjects_{};
 std::array<float,40> parameterSnapshot_{};
 bool parameterSnapshotValid_=false;
 nibbi::OutputStage outputStage_;
 std::atomic<double> processingRate_{48000};
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NibbiProcessor)
};
