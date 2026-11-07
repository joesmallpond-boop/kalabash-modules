// ABACUS — granular texture synthesizer (DSP: Mutable Instruments Beads by Emilie Gillet, MIT)
#include "../../common/KbProcessor.h"
#include "../../common/PanelUI.h"
#include "../../cores/BeadsCore.h"

using namespace juce;
using namespace kb;

static const StringArray kQuality { "Digital 48k", "Cloudy 32k", "Clean tape 24k", "Dirty tape 24k" };

class AbacusProcessor : public KbProcessor {
 public:
  AbacusProcessor()
      : KbProcessor(BusesProperties().withInput("Input", AudioChannelSet::stereo(), true)
                                     .withOutput("Output", AudioChannelSet::stereo(), true), makeLayout()) {}

  static Layout makeLayout() {
    Layout l;
    l.add(fparam("time", "Time", 0.0f, 1.0f, 0.3f));
    l.add(fparam("size", "Size", 0.0f, 1.0f, 0.5f));
    l.add(fparam("shape", "Shape", 0.0f, 1.0f, 0.5f));
    l.add(fparam("pitch", "Pitch", 0.0f, 1.0f, 0.5f));
    l.add(fparam("timeAmt", "Time Random", -1.0f, 1.0f, 0.0f));
    l.add(fparam("sizeAmt", "Size Random", -1.0f, 1.0f, 0.0f));
    l.add(fparam("shapeAmt", "Shape Random", -1.0f, 1.0f, 0.0f));
    l.add(fparam("pitchAmt", "Pitch Random", -1.0f, 1.0f, 0.0f));
    l.add(fparam("density", "Density", 0.0f, 1.0f, 0.7f));
    l.add(fparam("feedback", "Feedback", 0.0f, 1.0f, 0.0f));
    l.add(fparam("dryWet", "Dry/Wet", 0.0f, 1.0f, 0.5f));
    l.add(fparam("reverb", "Reverb", 0.0f, 1.0f, 0.2f));
    l.add(cparam("quality", "Quality", kQuality, 0));
    l.add(bparam("mono", "Mono Buffer", false));
    l.add(bparam("freeze", "Freeze", false));
    l.add(bparam("seed", "Seed", false));
    l.add(bparam("latch", "Seed Latch", true));
    l.add(cparam("clock", "Seed Clock", HostClock::names(), 0));
    l.add(bparam("synth", "Synth Mode", false));
    l.add(fparam("inGain", "In Gain", -18.0f, 18.0f, 0.0f, -1.0f, "dB"));
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
    BeadsParams p;
    p.time = v("time"); p.size = v("size"); p.shape = v("shape"); p.pitchKnob = v("pitch");
    p.timeAmt = v("timeAmt"); p.sizeAmt = v("sizeAmt"); p.shapeAmt = v("shapeAmt"); p.pitchAmt = v("pitchAmt");
    p.density = v("density");
    p.feedback = v("feedback"); p.dryWet = v("dryWet"); p.reverb = v("reverb");
    p.quality = vi("quality"); p.mono = vb("mono"); p.freeze = vb("freeze");
    p.seedButton = vb("seed"); p.latched = vb("latch"); p.synth = vb("synth");
    p.inGain = Decibels::decibelsToGain(v("inGain"));
    p.outGain = Decibels::decibelsToGain(v("outGain"));
    clock.update(getPlayHead(), rate);
    const int div = vi("clock");
    p.seedPatched = div > 0;

    const float* inL = buf.getReadPointer(0);
    const float* inR = buf.getNumChannels() > 1 ? buf.getReadPointer(1) : inL;
    tmpL.assign(inL, inL + n); tmpR.assign(inR, inR + n);
    float* L = buf.getWritePointer(0);
    outR.resize((size_t) n);
    float* R = buf.getNumChannels() > 1 ? buf.getWritePointer(1) : outR.data();
    const int chunk = 16;
    for (int s = 0; s < n; s += chunk) {
      const int c = std::min(chunk, n - s);
      p.seedGate = clock.gateAt(div, s);
      core.process(p, tmpL.data() + s, tmpR.data() + s, L + s, R + s, c);
    }
    if (core.latencyChanged()) setLatencySamples(core.latency());
    if (core.seedActivity()) seedFlash = 1.0f;
  }

  AudioProcessorEditor* createEditor() override;

  BeadsCore core;
  HostClock clock;
  double rate = 48000;
  std::vector<float> tmpL, tmpR, outR;
  std::atomic<float> seedFlash { 0.0f };
};

static Theme abacusTheme() {
  return { Colour(0xff2a2433), Colour(0xff0f0c13), Colour(0xffeee6f2), Colour(0xffd96bff), Colour(0xff504a5a), Colour(0x12ffffff) };
}

AudioProcessorEditor* AbacusProcessor::createEditor() {
  // The four grain parameters sit in a row, each with its attenurandomizer below,
  // DENSITY above them, mix controls and the SEED / FREEZE buttons at the bottom.
  std::vector<Widget> L {
    { W::Section, "", 20, 64, 600, 120, "Grain generation" },
    { W::Section, "", 20, 192, 600, 210, "Grain" },
    { W::Section, "", 20, 410, 330, 150, "Mix" },
    { W::Section, "", 360, 410, 260, 150, "Buffer" },

    { W::BigKnob, "density", 320, 128, 0, 0, "Density" },
    { W::Button, "seed", 60, 98, 90, 50, "SEED" },
    { W::Toggle, "latch", 60, 152, 90, 22, "LATCH" },
    { W::Choice, "clock", 440, 112, 110, 24, "Seed clock" },
    { W::Toggle, "synth", 440, 148, 110, 24, "SYNTH" },

    { W::BigKnob, "time", 95, 260, 0, 0, "Time" },
    { W::BigKnob, "size", 245, 260, 0, 0, "Size" },
    { W::BigKnob, "shape", 395, 260, 0, 0, "Shape" },
    { W::BigKnob, "pitch", 545, 260, 0, 0, "Pitch" },
    { W::SmallKnob, "timeAmt", 95, 352, 0, 0, "" },
    { W::SmallKnob, "sizeAmt", 245, 352, 0, 0, "" },
    { W::SmallKnob, "shapeAmt", 395, 352, 0, 0, "" },
    { W::SmallKnob, "pitchAmt", 545, 352, 0, 0, "" },

    { W::Knob, "feedback", 75, 475, 0, 0, "Feedback" },
    { W::Knob, "dryWet", 160, 475, 0, 0, "Dry/wet" },
    { W::Knob, "reverb", 245, 475, 0, 0, "Reverb" },
    { W::SmallKnob, "inGain", 305, 445, 0, 0, "In" },
    { W::SmallKnob, "outGain", 305, 515, 0, 0, "Out" },

    { W::Toggle, "freeze", 380, 440, 100, 44, "FREEZE" },
    { W::Toggle, "mono", 490, 440, 110, 22, "MONO 2x" },
    { W::Choice, "quality", 380, 512, 220, 24, "Quality" },
  };
  auto* ed = new PanelEditor(*this, state, abacusTheme(), "ABACUS", "granular texture synthesizer - after Beads by Emilie Gillet", 640, 600, L);
  auto* proc = this;
  ed->drawExtras = [proc](Graphics& g, PanelEditor& e) {
    // seed LED, freeze LED, quality LED row, buffer reserve
    e.drawLed(g, 160, 104, 5.0f, proc->seedFlash.load(), e.theme().accent);
    proc->seedFlash = proc->seedFlash.load() * 0.85f;
    e.drawLed(g, 470, 448, 5.0f, proc->vb("freeze") ? 1.0f : 0.0f, Colour(0xff6bd4ff));
    const int q = proc->vi("quality");
    for (int i = 0; i < 4; ++i) e.drawLed(g, 500 + i * 18.0f, 478, 4.0f, i == q ? 1.0f : 0.0f, e.theme().accent);
    // attenurandomizer legend
    g.setColour(e.theme().ink.withAlpha(0.5f));
    g.setFont(FontOptions(9.0f * e.scale()));
    for (float x : { 95.0f, 245.0f, 395.0f, 545.0f })
      g.drawText("peaky  <  >  smooth", e.unit(x - 50, 372, 100, 12).toNearestInt(), Justification::centred);
    // pitch readout (the knob snaps to musical intervals)
    g.setColour(e.theme().accent);
    g.setFont(FontOptions(10.0f * e.scale(), Font::bold));
    const float st = BeadsCore::pitchKnobToSemitones(proc->v("pitch"));
    g.drawText(String(st >= 0 ? "+" : "") + String(st, 1) + " st", e.unit(495, 210, 100, 12).toNearestInt(), Justification::centred);
    g.setColour(e.theme().ink.withAlpha(0.6f));
    g.setFont(FontOptions(9.0f * e.scale()));
    g.drawText(proc->v("size") > 0.98f ? "delay mode" : "", e.unit(195, 210, 100, 12).toNearestInt(), Justification::centred);
  };
  return ed;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AbacusProcessor(); }
