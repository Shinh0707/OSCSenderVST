#include "AudioProcessor.h"
#include "AudioEditor.h"

WebPluginAudioProcessor::WebPluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
#endif
{
  startTimerHz(60); // Check for MIDI events frequently
}

WebPluginAudioProcessor::~WebPluginAudioProcessor() {
  stopTimer();
}

const juce::String WebPluginAudioProcessor::getName() const {
  return JucePlugin_Name;
}
bool WebPluginAudioProcessor::acceptsMidi() const { return true; }
bool WebPluginAudioProcessor::producesMidi() const { return true; }
bool WebPluginAudioProcessor::isMidiEffect() const { return false; }
double WebPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int WebPluginAudioProcessor::getNumPrograms() { return 1; }
int WebPluginAudioProcessor::getCurrentProgram() { return 0; }
void WebPluginAudioProcessor::setCurrentProgram(int index) {}
const juce::String WebPluginAudioProcessor::getProgramName(int index) {
  return {};
}
void WebPluginAudioProcessor::changeProgramName(int index,
                                                const juce::String &newName) {}

void WebPluginAudioProcessor::prepareToPlay(double sampleRate,
                                            int samplesPerBlock) {
  hostSampleRate = sampleRate;
}

void WebPluginAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool WebPluginAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;
#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif
  return true;
}
#endif

// 汎用WAVロード処理（必要に応じて使用）
void WebPluginAudioProcessor::loadWavFile(const juce::File &file) {
  juce::AudioFormatManager formatManager;
  formatManager.registerBasicFormats();
  std::unique_ptr<juce::AudioFormatReader> reader(
      formatManager.createReaderFor(file));

  if (reader != nullptr) {
    double fileSampleRate = reader->sampleRate;
    int numChannels = (int)reader->numChannels;
    int lengthInSamples = (int)reader->lengthInSamples;
    juce::AudioBuffer<float> newBuffer;

    if (std::abs(fileSampleRate - hostSampleRate) < 1.0 ||
        hostSampleRate == 0.0) {
      newBuffer.setSize(numChannels, lengthInSamples);
      reader->read(&newBuffer, 0, lengthInSamples, 0, true, true);
    } else {
      juce::AudioBuffer<float> tempBuffer(numChannels, lengthInSamples);
      reader->read(&tempBuffer, 0, lengthInSamples, 0, true, true);
      double ratio = fileSampleRate / hostSampleRate;
      int newLength = static_cast<int>(lengthInSamples / ratio);
      newBuffer.setSize(numChannels, newLength);
      for (int ch = 0; ch < numChannels; ++ch) {
        juce::LagrangeInterpolator interpolator;
        interpolator.process(ratio, tempBuffer.getReadPointer(ch),
                             newBuffer.getWritePointer(ch), newLength);
      }
    }
    std::lock_guard<std::mutex> lock(audioMutex);
    synthesisBuffer = std::move(newBuffer);
  }
}

void WebPluginAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                           juce::MidiBuffer &midiMessages) {
  buffer.clear();

  // Read MIDI notes and pass to message thread for OSC
  for (const auto metadata : midiMessages) {
    auto msg = metadata.getMessage();
    if (msg.isNoteOn() || msg.isNoteOff()) {
      int start1, size1, start2, size2;
      midiFifo.prepareToWrite(1, start1, size1, start2, size2);
      if (size1 > 0) {
        midiEventQueue[static_cast<std::size_t>(start1)] = {
            msg.getNoteNumber(), msg.getFloatVelocity(), msg.isNoteOn()};
        midiFifo.finishedWrite(1);
      }
    }
  }

  bool hostPlaying = false;
  if (auto *playHead = getPlayHead()) {
    if (auto positionInfo = playHead->getPosition()) {
      hostPlaying = positionInfo->getIsPlaying();
      isHostPlaying.store(hostPlaying);
      if (hostPlaying)
        isStandalonePlaying.store(false);

      if (auto ppq = positionInfo->getPpqPosition()) {
        if (hostPlaying)
          currentPpqPosition.store(*ppq);
      }
      if (auto bpm = positionInfo->getBpm())
        currentBpm.store(*bpm);
      if (auto timeSig = positionInfo->getTimeSignature()) {
        timeSigNumerator.store(timeSig->numerator);
        timeSigDenominator.store(timeSig->denominator);
      }
    }
  }

  if (!hostPlaying && isStandalonePlaying.load()) {
    double currentPos = currentPpqPosition.load();
    double currentBpmVal = currentBpm.load() > 0.0 ? currentBpm.load() : 120.0;
    double timeInSeconds = (double)buffer.getNumSamples() / getSampleRate();
    double ppqAdvance = timeInSeconds * (currentBpmVal / 60.0);
    currentPpqPosition.store(currentPos + ppqAdvance);
  }

  std::lock_guard<std::mutex> lock(audioMutex);
  if (synthesisBuffer.getNumSamples() == 0)
    return;

  bool isPlayingNow = hostPlaying || isStandalonePlaying.load();
  if (isPlayingNow) {
    int64_t currentSamplePosition = 0;
    if (hostPlaying) {
      if (auto *playHead = getPlayHead()) {
        if (auto positionInfo = playHead->getPosition()) {
          if (auto timeInSamplesOpt = positionInfo->getTimeInSamples()) {
            currentSamplePosition = *timeInSamplesOpt;
          }
        }
      }
    } else {
      double ppq = currentPpqPosition.load();
      double bpm = currentBpm.load() > 0.0 ? currentBpm.load() : 120.0;
      currentSamplePosition =
          static_cast<int64_t>((ppq / (bpm / 60.0)) * getSampleRate());
    }

    int numSamples = buffer.getNumSamples();
    int synthSamples = synthesisBuffer.getNumSamples();

    if (currentSamplePosition < synthSamples) {
      int bufferOffset = 0;
      int readOffset = (int)currentSamplePosition;
      int toCopy = numSamples;

      if (readOffset < 0) {
        bufferOffset = -readOffset;
        toCopy += readOffset;
        readOffset = 0;
      }

      if (toCopy > 0) {
        toCopy = std::min(toCopy, synthSamples - readOffset);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
          int readCh = std::min(ch, synthesisBuffer.getNumChannels() - 1);
          buffer.copyFrom(ch, bufferOffset, synthesisBuffer, readCh, readOffset,
                          toCopy);
        }
      }
    } else if (!hostPlaying) {
      isStandalonePlaying.store(false);
    }
  }
}

bool WebPluginAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *WebPluginAudioProcessor::createEditor() {
  return new WebPluginAudioProcessorEditor(*this);
}

// DAWのプロジェクト保存時に呼ばれる
void WebPluginAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
  juce::MemoryOutputStream stream(destData, true);
  stream.writeString(pluginStateJson);
}

// DAWのプロジェクト読み込み時に呼ばれる
void WebPluginAudioProcessor::setStateInformation(const void *data,
                                                  int sizeInBytes) {
  juce::String stateString =
      juce::String::createStringFromData(data, sizeInBytes);
  if (stateString.isNotEmpty()) {
    pluginStateJson = stateString;
  }
}

void WebPluginAudioProcessor::timerCallback() {
  juce::var parsed = juce::JSON::parse(pluginStateJson);
  if (parsed.isObject()) {
    juce::String ip = parsed.getProperty("ip", "127.0.0.1");
    int port = parsed.getProperty("port", 9000);
    juce::String addr = parsed.getProperty("oscAddress", "/midi/note");

    if (ip != currentOscIp || port != currentOscPort) {
      currentOscIp = ip;
      currentOscPort = port;
      currentOscAddress = addr;
      if (ip.isNotEmpty() && port > 0) {
        oscSender.connect(ip, port);
      }
    } else if (addr != currentOscAddress) {
      currentOscAddress = addr;
    }
  }

  int start1, size1, start2, size2;
  midiFifo.prepareToRead(1024, start1, size1, start2, size2);

  if (size1 > 0) {
    juce::OSCBundle bundle;

    auto addEvents = [&](int start, int size) {
      for (int i = 0; i < size; ++i) {
        auto &ev = midiEventQueue[static_cast<std::size_t>(start + i)];

        juce::String noteAddr =
            currentOscAddress + "/note" + juce::String(ev.noteNumber);
        juce::String velAddr =
            currentOscAddress + "/note_vel" + juce::String(ev.noteNumber);

        try {
          juce::OSCMessage noteMsg{juce::OSCAddressPattern(noteAddr)};
          noteMsg.addInt32((ev.isNoteOn && ev.velocity > 0.0f) ? 1 : 0);
          bundle.addElement(noteMsg);

          juce::OSCMessage velMsg{juce::OSCAddressPattern(velAddr)};
          velMsg.addFloat32(ev.velocity);
          bundle.addElement(velMsg);
        } catch (const juce::OSCFormatError &) {
          // ignore bad address formats
        }
      }
    };

    addEvents(start1, size1);
    if (size2 > 0) {
      addEvents(start2, size2);
    }

    if (!bundle.isEmpty() && currentOscIp.isNotEmpty() && currentOscPort > 0) {
      oscSender.send(bundle);
    }

    midiFifo.finishedRead(size1 + size2);
  }
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new WebPluginAudioProcessor();
}