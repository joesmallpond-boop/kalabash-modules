// kalabash modules — small base for APVTS-backed processors.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace kb {

using APVTS = juce::AudioProcessorValueTreeState;
using Layout = APVTS::ParameterLayout;

inline std::unique_ptr<juce::AudioParameterFloat> fparam(const juce::String& id, const juce::String& name,
    float lo, float hi, float def, float skewCentre = -1.0f, const juce::String& unit = {}) {
  juce::NormalisableRange<float> r(lo, hi);
  if (skewCentre > lo && skewCentre < hi) r.setSkewForCentre(skewCentre);
  return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, r, def,
      juce::AudioParameterFloatAttributes().withLabel(unit));
}
inline std::unique_ptr<juce::AudioParameterChoice> cparam(const juce::String& id, const juce::String& name,
    const juce::StringArray& choices, int def) {
  return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id, 1 }, name, choices, def);
}
inline std::unique_ptr<juce::AudioParameterBool> bparam(const juce::String& id, const juce::String& name, bool def) {
  return std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 }, name, def);
}
inline std::unique_ptr<juce::AudioParameterInt> iparam(const juce::String& id, const juce::String& name, int lo, int hi, int def) {
  return std::make_unique<juce::AudioParameterInt>(juce::ParameterID { id, 1 }, name, lo, hi, def);
}

class KbProcessor : public juce::AudioProcessor {
 public:
  KbProcessor(const BusesProperties& b, Layout layout)
      : juce::AudioProcessor(b), state(*this, nullptr, "STATE", std::move(layout)) {}

  float v(const char* id) const { return state.getRawParameterValue(id)->load(); }
  int vi(const char* id) const { return (int) std::lround(state.getRawParameterValue(id)->load()); }
  bool vb(const char* id) const { return state.getRawParameterValue(id)->load() > 0.5f; }

  bool hasEditor() const override { return true; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 4.0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return "Default"; }
  void changeProgramName(int, const juce::String&) override {}
  const juce::String getName() const override { return JucePlugin_Name; }
  void releaseResources() override {}

  void getStateInformation(juce::MemoryBlock& dest) override {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
  }
  void setStateInformation(const void* data, int size) override {
    if (auto xml = getXmlFromBinary(data, size))
      if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
  }

  APVTS state;
};

}  // namespace kb
