#pragma once

#include <atomic>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_osc/juce_osc.h>
#include <mutex>
#include <array>

struct OscMidiEvent {
  int noteNumber;
  float velocity;
  bool isNoteOn;
};


class WebPluginAudioProcessor : public juce::AudioProcessor, public juce::Timer {
public:
  WebPluginAudioProcessor();
  ~WebPluginAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  // DAWのセッション保存・復元用
  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  // 汎用オーディオロード機能 (Web側から任意のWAVをロードする用途等)
  void loadWavFile(const juce::File &file);

  void timerCallback() override;

  juce::OSCSender oscSender;
  juce::String currentOscIp{""};
  int currentOscPort{0};
  juce::String currentOscAddress{"/midi/note"};

  juce::AbstractFifo midiFifo{1024};
  std::array<OscMidiEvent, 1024> midiEventQueue{};


  // 同期・状態管理用の共有変数
  std::mutex audioMutex;
  juce::AudioBuffer<float> synthesisBuffer;
  juce::String pluginStateJson = "{}"; // Web UIの状態を保持

  std::atomic<double> currentPpqPosition{0.0};
  std::atomic<bool> isHostPlaying{false};
  std::atomic<bool> isStandalonePlaying{false};

  std::atomic<double> currentBpm{120.0};
  std::atomic<int> timeSigNumerator{4};
  std::atomic<int> timeSigDenominator{4};

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebPluginAudioProcessor)
  double hostSampleRate = 44100.0;
};