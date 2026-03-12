#pragma once

#include "AudioProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <vector>

class WebPluginAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::KeyListener,
                                      public juce::Timer {
public:
  WebPluginAudioProcessorEditor(WebPluginAudioProcessor &);
  ~WebPluginAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;
  void timerCallback() override;
  bool keyPressed(const juce::KeyPress &, juce::Component *) override;

private:
  struct Shortcut {
    juce::KeyPress keyPress;
    juce::String id;
  };

  WebPluginAudioProcessor &audioProcessor;
  std::unique_ptr<juce::WebBrowserComponent> webComponentPtr;
  std::unique_ptr<juce::FileChooser> chooser;

  std::vector<Shortcut> registeredShortcuts;
  juce::CriticalSection shortcutLock;

  double lastReportedTicks = -1.0;
  double lastReportedBpm = -1.0;
  int lastReportedNum = -1;
  int lastReportedDen = -1;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebPluginAudioProcessorEditor)
};