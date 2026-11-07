// FOGBANK — granular texture processor (DSP: Mutable Instruments Clouds by Emilie Gillet, MIT)
#include "../../common/KbProcessor.h"
#include "../../common/PanelUI.h"
#include "../../cores/CloudsCore.h"

using namespace juce;
using namespace kb;

static const StringArray kModes { "Granular", "Stretch", "Looping delay", "Spectral" };
static const StringArray kQualities { "16-bit stereo 1s", "16-bit mono 2s", "8-bit stereo 4s", "8-bit mono 8s" };

class FogbankProcessor : public KbProcessor {
 public:
  FogbankProcessor()
      : KbProcessor(BusesProperties().withInput("Input", AudioChannelSet::stereo(), true)
                                     .withOutput("Output", AudioChannelSet::stereo(), true), makeLayout()) {}

  static Layout makeLayout() {
    Layout l;
    l.add(fparam("position", "Position", 0.0f, 1.0f, 0.5f));
    l.add(fparam("size", "Size", 0.0f, 1.0f, 0.5f));
    l.add(fparam("pitch", "Pitch", -24.0f, 24.0f, 0.0f, -1.0f, "st"));
    l.add(bparam("pitchSnap", "Pitch Snap", true));
    l.add(fparam("density", "Density", 0.0f, 1.0f, 0.7f));
    l.add(fparam("texture", "Texture", 0.0f, 1.0f, 0.5f));
    l.add(fparam("inGain", "In Gain", -18.0f, 18.0f, 0.0f, -1.0f, "dB"));
    l.add(fparam("dryWet", "Dry/Wet", 0.0f, 1.0f, 0.5f));
    l.add(fparam("spread", "Stereo Spread", 0.0f, 1.0f, 0.5f));
    l.add(fparam("feedback", "Feedback", 0.0f, 1.0f, 0.0f));
    l.add(fparam("reverb", "Reverb", 0.0f, 1.0f, 0.3f));
    l.add(bparam("freeze", "Freeze", false));
    l.add(bparam("trig", "Trigger", false));
    l.add(cparam("clock", "Trigger Sync", HostClock::names(), 0));
    l.add(cparam("mode", "Mode", kModes, 0));
    l.add(cparam("quality", "Quality", kQualities, 0));
    l.add(fparam("outGain", "Out Gain", -18.0f, 12.0f, 0.0f, -1.0f, "dB"));
    return l;
  }

  bool isBusesLayoutSupported(const BusesLayout& l) const override {
    const auto o = l.getMainOutputChannelSet(), i = l.getMainInputChannelSet();
    return (o == AudioChannelSet::stereo() || o == AudioChannelSet::mono()) &&
           (i == AudioChannelSet::stereo() || i == AudioChannelSet::mono());
  }
  bool acceptsMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 8.0; }

  void prepareToPlay(double sr, int) override { rate = sr; core.prepare(sr); setLatencySamples(core.latency()); }

  void processBlock(AudioBuffer<float>& buf, MidiBuffer&) override {
    ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    CloudsParams p;
    p.mode = vi("mode"); p.quality = vi("quality");
    p.position = v("position"); p.size = v("size");
    p.pitch = vb("pitchSnap") ? std::round(v("pitch")) : v("pitch");
    p.density = v("density"); p.texture = v("texture");
    p.inGain = Decibels::decibelsToGain(v("inGain"));
    p.outGain = Decibels::decibelsToGain(v("outGain"));
    p.dryWet = v("dryWet"); p.spread = v("spread"); p.feedback = v("feedback"); p.reverb = v("reverb");
    p.freeze = vb("freeze");
    const bool manual = vb("trig");
    clock.update(getPlayHead(), rate);
    const int div = vi("clock");

    const float* inL = buf.getReadPointer(0);
    const float* inR = buf.getNumChannels() > 1 ? buf.getReadPointer(1) : inL;
    tmpL.assign(inL, inL + n); tmpR.assign(inR, inR + n);
    float* L = buf.getWritePointer(0);
    outR.resize((size_t) n);
    float* R = buf.getNumChannels() > 1 ? buf.getWritePointer(1) : outR.data();
    // split into short chunks so tempo-synced triggers land accurately
    const int chunk = 32;
    for (int s = 0; s < n; s += chunk) {
      const int c = std::min(chunk, n - s);
      p.trigger = manual || clock.gateAt(div, s);
      if (p.trigger && !lastTrig) trigFlash = 1.0f;
      lastTrig = p.trigger;
      core.process(p, tmpL.data() + s, tmpR.data() + s, L + s, R + s, c);
    }
  }

  AudioProcessorEditor* createEditor() override;

  CloudsCore core;
  HostClock clock;
  double rate = 48000;
  std::vector<float> tmpL, tmpR, outR;
  bool lastTrig = false;
  std::atomic<float> trigFlash { 0.0f };
};

static Theme fogTheme() {
  return { Colour(0xff2b3a44), Colour(0xff10181d), Colour(0xffdfe8ec), Colour(0xff7fd3ff), Colour(0xff51606a), Colour(0x12ffffff) };
}

AudioProcessorEditor* FogbankProcessor::createEditor() {
  std::vector<Widget> L {
    { W::Section, "", 20, 64, 560, 150, "Grain" },
    { W::Section, "", 20, 222, 560, 140, "Texture" },
    { W::Section, "", 20, 370, 300, 200, "Blend" },
    { W::Section, "", 330, 370, 250, 200, "Control" },

    { W::BigKnob, "position", 110, 135, 0, 0, "Position" },
    { W::BigKnob, "size", 300, 135, 0, 0, "Size" },
    { W::BigKnob, "pitch", 490, 135, 0, 0, "Pitch" },
    { W::Toggle, "pitchSnap", 540, 74, 34, 20, "ST" },

    { W::Knob, "inGain", 95, 290, 0, 0, "In gain" },
    { W::Knob, "density", 235, 290, 0, 0, "Density" },
    { W::Knob, "texture", 375, 290, 0, 0, "Texture" },
    { W::Knob, "outGain", 510, 290, 0, 0, "Out gain" },

    { W::Knob, "dryWet", 85, 430, 0, 0, "Dry/wet" },
    { W::Knob, "spread", 175, 430, 0, 0, "Spread" },
    { W::Knob, "feedback", 85, 515, 0, 0, "Feedback" },
    { W::Knob, "reverb", 175, 515, 0, 0, "Reverb" },

    { W::Toggle, "freeze", 350, 400, 100, 44, "FREEZE" },
    { W::Button, "trig", 460, 400, 100, 44, "TRIG" },
    { W::Choice, "clock", 460, 466, 100, 22, "Trig sync" },
    { W::Choice, "mode", 350, 466, 100, 22, "Mode" },
    { W::Choice, "quality", 350, 516, 210, 22, "Quality / buffer" },
  };
  auto* ed = new PanelEditor(*this, state, fogTheme(), "FOGBANK", "texture synthesizer - after Clouds by Emilie Gillet", 600, 610, L);
  auto* proc = this;
  ed->drawExtras = [proc](Graphics& g, PanelEditor& e) {
    // 4-LED meter (input normally, output when frozen) — as on the module
    const float m = proc->vb("freeze") ? proc->core.outMeter() : proc->core.inMeter();
    const float db = Decibels::gainToDecibels(m, -60.0f);
    const float th[4] = { -36, -18, -9, -3 };
    for (int i = 0; i < 4; ++i)
      e.drawLed(g, 245 + i * 22.0f, 392, 5.0f, db > th[i] ? 1.0f : 0.0f,
                i == 3 ? Colour(0xffff5a4a) : (i == 2 ? Colour(0xffffc94a) : e.theme().accent));
    static const char* hints[4][5] = {
      { "buffer position", "grain size", "transpose", "grain rate / random", "window / diffusion" },
      { "buffer position", "window size", "transpose", "diffusion", "LP <- -> HP" },
      { "delay time", "shifter window", "transpose", "diffusion", "LP <- -> HP" },
      { "frame position", "spectral warp", "transpose", "refresh / phase", "quantise" } };
    const int md = jlimit(0, 3, proc->vi("mode"));
    g.setColour(e.theme().accent.withAlpha(0.85f));
    g.setFont(FontOptions(9.5f * e.scale()));
    const float xs[3] = { 110, 300, 490 };
    for (int i = 0; i < 3; ++i) g.drawText(hints[md][i], e.unit(xs[i] - 60, 182, 120, 12).toNearestInt(), Justification::centred);
    g.drawText(hints[md][3], e.unit(175, 328, 120, 12).toNearestInt(), Justification::centred);
    g.drawText(hints[md][4], e.unit(315, 328, 120, 12).toNearestInt(), Justification::centred);
    e.drawLed(g, 560, 400, 4.5f, proc->trigFlash.load(), e.theme().accent);
    proc->trigFlash = proc->trigFlash.load() * 0.8f;
    g.setColour(e.theme().ink.withAlpha(0.6f));
    g.setFont(FontOptions(9.0f * e.scale(), Font::bold));
    g.drawText(proc->vb("freeze") ? "OUT" : "IN", e.unit(235, 402, 90, 12).toNearestInt(), Justification::centred);
  };
  return ed;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FogbankProcessor(); }
