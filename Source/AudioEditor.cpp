#include "AudioEditor.h"
#if !JUCE_DEBUG
#include "BinaryData.h"
#endif

WebPluginAudioProcessorEditor::WebPluginAudioProcessorEditor(
    WebPluginAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
  auto options =
      juce::WebBrowserComponent::Options().withNativeIntegrationEnabled(true);

  options =
      options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
          .withWinWebView2Options(
              juce::WebBrowserComponent::Options::WinWebView2{}
                  .withUserDataFolder(juce::File::getSpecialLocation(
                      juce::File::SpecialLocationType::tempDirectory)))
          .withAppleWkWebViewOptions(
              juce::WebBrowserComponent::Options::AppleWkWebView{}
                  .withAllowAccessToEnclosingDirectory(true));
#if !JUCE_DEBUG
  auto getMimeType = [](const juce::String &url) -> juce::String {
    if (url.endsWithIgnoreCase(".html"))
      return "text/html";
    if (url.endsWithIgnoreCase(".js"))
      return "text/javascript";
    if (url.endsWithIgnoreCase(".css"))
      return "text/css";
    if (url.endsWithIgnoreCase(".svg"))
      return "image/svg+xml";
    return "application/octet-stream";
  };

  options = options.withResourceProvider(
      [getMimeType, &p](const juce::String &url)
          -> std::optional<juce::WebBrowserComponent::Resource> {
        // ルートへのアクセスは index.html とする
        juce::String reqUrl = url == "/" ? "index.html" : url;
        if (reqUrl.startsWith("/"))
          reqUrl = reqUrl.substring(1);
        // その他、外部リソース読み込み処理
        // if (reqUrl.startsWith(...))
        juce::String fileName = reqUrl.fromLastOccurrenceOf("/", false, false);
        if (fileName.isEmpty())
          fileName = reqUrl;

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
          if (juce::String(BinaryData::originalFilenames[i]) == fileName) {
            int dataSize = 0;
            if (const char *data = BinaryData::getNamedResource(
                    BinaryData::namedResourceList[i], dataSize)) {
              const auto *byteData = reinterpret_cast<const std::byte *>(data);
              std::vector<std::byte> vecData(byteData, byteData + dataSize);

              return juce::WebBrowserComponent::Resource{std::move(vecData),
                                                         getMimeType(reqUrl)};
            }
          }
        }
        return std::nullopt;
      });
#endif

  // 1. Web UIからのステート受信（随時）
  options = options.withNativeFunction(
      "updatePluginState",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0) {
          audioProcessor.pluginStateJson = args[0].toString();
          completion("State Updated");
        } else {
          completion(juce::var::undefined());
        }
      });

  // 2. ステートのファイル保存
  // (DAWの外に個別のプリセットとして保存したい場合など)
  options = options.withNativeFunction(
      "savePluginState",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0) {
          juce::String payload = args[0].toString();
          chooser = std::make_unique<juce::FileChooser>(
              "Save State",
              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                  .getChildFile("state.json"),
              "*.json");
          chooser->launchAsync(
              juce::FileBrowserComponent::saveMode |
                  juce::FileBrowserComponent::canSelectFiles,
              [completion, payload](const juce::FileChooser &fc) {
                auto file = fc.getResult();
                if (file != juce::File{}) {
                  file.replaceWithText(payload);
                  completion(true);
                } else {
                  completion(false);
                }
              });
        } else {
          completion(false);
        }
      });

  // 3. ステートのファイル読み込み
  options = options.withNativeFunction(
      "loadPluginState",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        chooser = std::make_unique<juce::FileChooser>("Load State",
                                                      juce::File{}, "*.json");
        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                             [this, completion](const juce::FileChooser &fc) {
                               auto file = fc.getResult();
                               if (file.existsAsFile()) {
                                 juce::String content = file.loadFileAsString();
                                 audioProcessor.pluginStateJson =
                                     content; // C++側のステートも更新
                                 completion(content);
                               } else {
                                 completion(juce::var::undefined());
                               }
                             });
      });

  // 4. 再生コントロール
  options = options.withNativeFunction(
      "togglePlayback",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (!audioProcessor.isHostPlaying.load()) {
          bool currentlyPlaying = audioProcessor.isStandalonePlaying.load();
          if (!currentlyPlaying && args.size() > 0) {
            double startTicks = static_cast<double>(args[0]);
            double newPpq = startTicks / 480.0;
            audioProcessor.currentPpqPosition.store(newPpq);
          }
          audioProcessor.isStandalonePlaying.store(!currentlyPlaying);
          completion("Toggled");
        } else {
          completion("Host is playing");
        }
      });

  // 5. ショートカットの登録
  options = options.withNativeFunction(
      "registerShortcuts",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0 && args[0].isArray()) {
          auto *shortcutsArray = args[0].getArray();
          const juce::ScopedLock sl(shortcutLock);
          registeredShortcuts.clear();
          if (shortcutsArray != nullptr) {
            for (auto &s : *shortcutsArray) {
              juce::String id = s.getProperty("id", "").toString();
              juce::String desc = s.getProperty("desc", "").toString();
              if (id.isNotEmpty() && desc.isNotEmpty()) {
                registeredShortcuts.push_back(
                    {juce::KeyPress::createFromDescription(desc), id});
              }
            }
          }
          completion("Registered");
        }
      });

  webComponentPtr = std::make_unique<juce::WebBrowserComponent>(options);
  addKeyListener(this);
  setWantsKeyboardFocus(true);

  setSize(800, 600);
  addAndMakeVisible(webComponentPtr.get());

#if JUCE_DEBUG
  // 開発時: Viteなどのローカル開発サーバーを使用
  webComponentPtr->goToURL("http://localhost:5173/");
#else
  // Release時: withResourceProviderで設定したローカルアセットを読み込む
  webComponentPtr->goToURL(
      juce::WebBrowserComponent::getResourceProviderRoot());
#endif
  startTimerHz(30);
}

WebPluginAudioProcessorEditor::~WebPluginAudioProcessorEditor() {
  removeKeyListener(this);
  stopTimer();
}

void WebPluginAudioProcessorEditor::timerCallback() {
  if (webComponentPtr == nullptr)
    return;

  bool isPlaying = audioProcessor.isHostPlaying.load() ||
                   audioProcessor.isStandalonePlaying.load();
  double ppq = audioProcessor.currentPpqPosition.load();
  double currentTicks = ppq * 480.0;

  if (isPlaying || currentTicks != lastReportedTicks) {
    lastReportedTicks = currentTicks;
    juce::String jsCode =
        "if (typeof window.updatePlayheadPosition === 'function') { "
        "  window.updatePlayheadPosition(" +
        juce::String(currentTicks) +
        ");"
        "}";
    webComponentPtr->evaluateJavascript(jsCode);
  }

  double bpm = audioProcessor.currentBpm.load();
  int num = audioProcessor.timeSigNumerator.load();
  int den = audioProcessor.timeSigDenominator.load();

  if (bpm != lastReportedBpm || num != lastReportedNum ||
      den != lastReportedDen) {
    lastReportedBpm = bpm;
    lastReportedNum = num;
    lastReportedDen = den;
    juce::String jsCode;
    jsCode << "if (typeof window.updateHostTempo === 'function') { "
           << "  window.updateHostTempo(" << bpm << ", " << num << ", " << den
           << ");"
           << "}";
    webComponentPtr->evaluateJavascript(jsCode);
  }
}

bool WebPluginAudioProcessorEditor::keyPressed(const juce::KeyPress &key,
                                               juce::Component *) {
  const juce::ScopedLock sl(shortcutLock);
  for (const auto &shortcut : registeredShortcuts) {
    if (shortcut.keyPress == key) {
      juce::String js;
      js << "if (typeof window.handleJuceShortcut === 'function') { "
         << "  window.handleJuceShortcut('" << shortcut.id << "'); "
         << "}";
      webComponentPtr->evaluateJavascript(js);
      return true;
    }
  }
  return false;
}

void WebPluginAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void WebPluginAudioProcessorEditor::resized() {
  webComponentPtr->setBounds(getLocalBounds());
  webComponentPtr->grabKeyboardFocus();
}