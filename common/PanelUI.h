// kalabash modules — shared panel UI: look-and-feel, data-driven panel editor.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>

namespace kb {

struct Theme {
  juce::Colour panel;      // panel base colour
  juce::Colour panelEdge;
  juce::Colour ink;        // labels / lines
  juce::Colour accent;     // value arcs, LEDs
  juce::Colour knob;       // knob cap
  juce::Colour section;    // section box tint
};

class KbLook : public juce::LookAndFeel_V4 {
 public:
  explicit KbLook(const Theme& t);
  void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos,
                        float start, float end, juce::Slider&) override;
  void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                            bool highlighted, bool down) override;
  void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
  void drawComboBox(juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
  juce::Font getComboBoxFont(juce::ComboBox&) override;
  juce::Font getPopupMenuFont() override;
  void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
  juce::Font getLabelFont(juce::Label&) override;
  Theme theme;
};

enum class W { BigKnob, Knob, SmallKnob, Atten, Button, Toggle, Choice, Section, Label };

struct Widget {
  W type;
  juce::String param;     // parameter ID (empty for Section/Label)
  float x, y, w, h;       // centre x/y for knobs; top-left for boxes; in panel units
  juce::String label;
  juce::String sub;       // secondary label (e.g. alternate function)
};

// A single module-style panel bound to an APVTS.
class PanelEditor : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  PanelEditor(juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& state,
              const Theme& theme, juce::String title, juce::String subtitle,
              int width, int height, std::vector<Widget> layout);
  ~PanelEditor() override;

  void paint(juce::Graphics&) override;
  void paintOverChildren(juce::Graphics&) override;
  void resized() override;

  // hooks for per-plugin custom drawing
  std::function<void(juce::Graphics&, PanelEditor&)> drawExtras;
  std::function<void(PanelEditor&)> onTick;
  std::function<void(const juce::MouseEvent&, PanelEditor&)> onClick;
  void mouseDown(const juce::MouseEvent& e) override { if (onClick) onClick(e, *this); }

  juce::Rectangle<float> unit(float x, float y, float w, float h) const;  // panel units -> pixels
  float scale() const { return scale_; }
  const Theme& theme() const { return look_.theme; }
  juce::AudioProcessorValueTreeState& state;
  juce::Component* find(const juce::String& paramId);
  void drawLed(juce::Graphics& g, float cx, float cy, float r, float amount, juce::Colour c) const;

 private:
  void timerCallback() override { if (onTick) onTick(*this); repaint(); }
  KbLook look_;
  juce::String title_, subtitle_;
  int baseW_, baseH_;
  float scale_ = 1.0f;
  std::vector<Widget> layout_;
  struct Item {
    Widget w;
    std::unique_ptr<juce::Component> comp;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
  };
  std::vector<std::unique_ptr<Item>> items_;
  juce::TooltipWindow tooltip_ { this, 600 };
};

// Host tempo -> gate stream (used for tempo-synced triggers).
struct HostClock {
  // division index: 0 off, then 1/1,1/2,1/4,1/8,1/16,1/32,1/4T,1/8T,1/16T
  static juce::StringArray names() {
    return { "Off", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T" };
  }
  static double beatsPerTick(int idx) {
    static const double v[] = { 0, 4, 2, 1, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return v[juce::jlimit(0, 9, idx)];
  }
  // returns gate state for sample offset i within the block
  bool playing = false;
  double ppqStart = 0.0, bpm = 120.0, sr = 48000.0;
  void update(juce::AudioPlayHead* ph, double sampleRate) {
    sr = sampleRate;
    playing = false;
    if (ph == nullptr) return;
    if (auto pos = ph->getPosition()) {
      playing = pos->getIsPlaying();
      if (auto b = pos->getBpm()) bpm = *b;
      if (auto q = pos->getPpqPosition()) ppqStart = *q;
    }
  }
  bool gateAt(int idx, int sample) const {
    if (!playing || idx <= 0) return false;
    const double ppq = ppqStart + sample * bpm / 60.0 / sr;
    const double t = ppq / beatsPerTick(idx);
    return (t - std::floor(t)) < 0.25;
  }
};

}  // namespace kb
