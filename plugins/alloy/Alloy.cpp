// ALLOY — modal synthesis voice (DSP: Mutable Instruments Elements by Emilie Gillet, MIT)
#include "../../common/KbProcessor.h"
#include "../../common/PanelUI.h"
#include "../../cores/ElementsCore.h"
#include <set>

using namespace juce;
using namespace kb;

class AlloyProcessor : public KbProcessor {
 public:
  AlloyProcessor()
      : KbProcessor(BusesProperties()
                        .withOutput("Output", AudioChannelSet::stereo(), true)
                        .withInput("Input", AudioChannelSet::stereo(), false)
                        .withInput("Sidechain", AudioChannelSet::stereo(), true),
                    makeLayout()) {}

  static Layout makeLayout() {
    Layout l;
    auto u = [&](const char* id, const char* name, float def) { l.add(fparam(id, name, 0.0f, 1.0f, def)); };
    auto b = [&](const char* id, const char* name) { l.add(fparam(id, name, -1.0f, 1.0f, 0.0f)); };
    u("contour", "Contour", 0.5f);
    u("bow", "Bow Level", 0.0f); u("blow", "Blow Level", 0.0f); u("strike", "Strike Level", 0.8f);
    u("flow", "Flow", 0.5f); u("mallet", "Mallet", 0.5f);
    u("bowTimbre", "Bow Timbre", 0.5f); u("blowTimbre", "Blow Timbre", 0.5f); u("strikeTimbre", "Strike Timbre", 0.5f);
    l.add(fparam("coarse", "Coarse", -30.0f, 30.0f, 0.0f, -1.0f, "st"));
    l.add(fparam("fine", "Fine", -50.0f, 50.0f, 0.0f, -1.0f, "ct"));
    l.add(bparam("tuned", "Tuned", true));
    l.add(fparam("a4", "A4 Reference", 415.0f, 466.0f, 440.0f, -1.0f, "Hz"));
    b("fm", "FM Amount");
    u("geometry", "Geometry", 0.5f); u("brightness", "Brightness", 0.5f); u("damping", "Damping", 0.5f);
    u("position", "Position", 0.3f);
    l.add(fparam("space", "Space", 0.0f, 2.0f, 0.4f));
    b("mBowTimbre", "Mod > Bow Timbre"); b("mFlow", "Mod > Flow"); b("mBlowTimbre", "Mod > Blow Timbre");
    b("mMallet", "Mod > Mallet"); b("mStrikeTimbre", "Mod > Strike Timbre");
    b("mGeometry", "Mod > Geometry"); b("mBrightness", "Mod > Brightness"); b("mDamping", "Mod > Damping");
    b("mPosition", "Mod > Position"); b("mSpace", "Mod > Space");
    l.add(cparam("modSrc", "Mod Source", { "Velocity", "Mod wheel", "Aftertouch", "LFO", "Random" }, 0));
    u("lfoRate", "LFO Rate", 0.3f);
    l.add(cparam("model", "Resonator", { "Modal", "String", "Strings" }, 0));
    l.add(bparam("ominous", "Ominous Mode", false));
    l.add(bparam("play", "Play", false));
    u("extIn", "Ext In", 0.0f);
    l.add(fparam("level", "Level", 0.0f, 1.5f, 0.8f));
    u("velStrength", "Vel > Strength", 1.0f);
    l.add(iparam("bend", "Bend Range", 0, 24, 2));
    return l;
  }

  bool isBusesLayoutSupported(const BusesLayout& l) const override {
    const auto out = l.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono()) return false;
    for (const auto& in : l.inputBuses)
      if (!(in.isDisabled() || in == AudioChannelSet::stereo() || in == AudioChannelSet::mono())) return false;
    return true;
  }

  void prepareToPlay(double sr, int) override { core.prepare(sr); setLatencySamples(core.latency()); }

  void processBlock(AudioBuffer<float>& buf, MidiBuffer& midi) override {
    ScopedNoDenormals nd;
    updateParams();
    const int n = buf.getNumSamples();
    // exciter audio comes from the sidechain (or the main input if a host enables it)
    inL.assign((size_t) n, 0.0f); inR.assign((size_t) n, 0.0f);
    for (int bus = 0; bus < getBusCount(true); ++bus) {
      if (!getBus(true, bus)->isEnabled()) continue;
      auto in = getBusBuffer(buf, true, bus);
      if (in.getNumChannels() == 0) continue;
      FloatVectorOperations::add(inL.data(), in.getReadPointer(0), n);
      FloatVectorOperations::add(inR.data(), in.getReadPointer(in.getNumChannels() > 1 ? 1 : 0), n);
    }
    auto out = getBusBuffer(buf, false, 0);
    float* L = out.getWritePointer(0);
    tmpR.resize((size_t) n);
    float* R = out.getNumChannels() > 1 ? out.getWritePointer(1) : tmpR.data();
    int pos = 0;
    for (const auto meta : midi) {
      const int at = jlimit(0, n, meta.samplePosition);
      if (at > pos) { core.render(p, inL.data() + pos, inR.data() + pos, L + pos, R + pos, at - pos); pos = at; }
      handle(meta.getMessage());
    }
    if (n > pos) core.render(p, inL.data() + pos, inR.data() + pos, L + pos, R + pos, n - pos);
    midi.clear();
  }

  void handle(const MidiMessage& m) {
    if (m.isNoteOn()) { sustained.erase(m.getNoteNumber()); core.noteOn(m.getNoteNumber(), m.getFloatVelocity()); }
    else if (m.isNoteOff()) { if (sustain) sustained.insert(m.getNoteNumber()); else core.noteOff(m.getNoteNumber()); }
    else if (m.isPitchWheel()) p.pitchBend = (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isController() && m.getControllerNumber() == 1) p.modWheel = m.getControllerValue() / 127.0f;
    else if (m.isChannelPressure()) p.aftertouch = m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch()) p.aftertouch = m.getAfterTouchValue() / 127.0f;
    else if (m.isSustainPedalOn()) sustain = true;
    else if (m.isSustainPedalOff()) { sustain = false; for (int nn : sustained) core.noteOff(nn); sustained.clear(); }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) { core.allNotesOff(); sustained.clear(); }
  }

  void updateParams() {
    p.contour = v("contour"); p.bowLevel = v("bow"); p.blowLevel = v("blow"); p.strikeLevel = v("strike");
    p.flow = v("flow"); p.mallet = v("mallet");
    p.bowTimbre = v("bowTimbre"); p.blowTimbre = v("blowTimbre"); p.strikeTimbre = v("strikeTimbre");
    // TUNED: COARSE snaps to semitones (as on the module) so MIDI notes stay in tune.
    p.tuned = vb("tuned");
    p.coarse = p.tuned ? std::round(v("coarse")) : v("coarse");
    p.fine = v("fine") / 100.0f + 12.0f * std::log2(v("a4") / 440.0f); p.fmAmt = v("fm");
    p.geometry = v("geometry"); p.brightness = v("brightness"); p.damping = v("damping");
    p.position = v("position"); p.space = v("space");
    p.modBowTimbre = v("mBowTimbre"); p.modFlow = v("mFlow"); p.modBlowTimbre = v("mBlowTimbre");
    p.modMallet = v("mMallet"); p.modStrikeTimbre = v("mStrikeTimbre"); p.modGeometry = v("mGeometry");
    p.modBrightness = v("mBrightness"); p.modDamping = v("mDamping"); p.modPosition = v("mPosition"); p.modSpace = v("mSpace");
    p.modSource = vi("modSrc"); p.lfoRate = v("lfoRate");
    p.model = vi("model"); p.ominous = vb("ominous"); p.play = vb("play");
    p.extIn = v("extIn") * 4.0f; p.level = v("level"); p.velToStrength = v("velStrength");
    p.bendRange = (float) vi("bend");
  }

  AudioProcessorEditor* createEditor() override;

  ElementsCore core;
  ElementsParams p;
  std::vector<float> inL, inR, tmpR;
  bool sustain = false;
  std::set<int> sustained;
};

static Theme alloyTheme() {
  return { Colour(0xffc9ccce), Colour(0xff6d7276), Colour(0xff1d2124), Colour(0xff2f7fd6), Colour(0xff2b2f33), Colour(0x10000000) };
}

AudioProcessorEditor* AlloyProcessor::createEditor() {
  // Two halves like the module: exciter on the left, resonator on the right,
  // each knob with its attenuverter (here: depth from the chosen MOD source).
  std::vector<Widget> L {
    { W::Section, "", 20, 64, 430, 450, "Exciter" },
    { W::Section, "", 460, 64, 440, 450, "Resonator" },
    { W::Section, "", 20, 524, 880, 92, "Performance" },

    { W::Knob, "contour", 75, 125, 0, 0, "Contour" },
    { W::BigKnob, "bow", 175, 125, 0, 0, "Bow" },
    { W::BigKnob, "blow", 275, 125, 0, 0, "Blow" },
    { W::BigKnob, "strike", 375, 125, 0, 0, "Strike" },
    { W::Knob, "flow", 275, 245, 0, 0, "Flow" },
    { W::Knob, "mallet", 375, 245, 0, 0, "Mallet" },
    { W::Knob, "bowTimbre", 175, 345, 0, 0, "Bow timbre" },
    { W::Knob, "blowTimbre", 275, 345, 0, 0, "Blow timbre" },
    { W::Knob, "strikeTimbre", 375, 345, 0, 0, "Strike timbre" },
    { W::Atten, "mFlow", 275, 300, 0, 0, "" },
    { W::Atten, "mMallet", 375, 300, 0, 0, "" },
    { W::Atten, "mBowTimbre", 175, 420, 0, 0, "mod" },
    { W::Atten, "mBlowTimbre", 275, 420, 0, 0, "mod" },
    { W::Atten, "mStrikeTimbre", 375, 420, 0, 0, "mod" },
    { W::Button, "play", 40, 222, 80, 46, "PLAY" },
    { W::SmallKnob, "extIn", 80, 345, 0, 0, "Ext in", "sidechain" },

    { W::BigKnob, "coarse", 530, 125, 0, 0, "Coarse" },
    { W::Knob, "fine", 630, 125, 0, 0, "Fine", "cents" },
    { W::Knob, "fm", 720, 125, 0, 0, "FM", "mod > pitch" },
    { W::Knob, "geometry", 530, 245, 0, 0, "Geometry" },
    { W::Knob, "brightness", 630, 245, 0, 0, "Brightness" },
    { W::Knob, "damping", 530, 345, 0, 0, "Damping" },
    { W::Knob, "position", 630, 345, 0, 0, "Position" },
    { W::BigKnob, "space", 810, 300, 0, 0, "Space" },
    { W::Atten, "mGeometry", 530, 300, 0, 0, "" },
    { W::Atten, "mBrightness", 630, 300, 0, 0, "" },
    { W::Atten, "mDamping", 530, 420, 0, 0, "mod" },
    { W::Atten, "mPosition", 630, 420, 0, 0, "mod" },
    { W::Atten, "mSpace", 810, 380, 0, 0, "mod" },
    { W::Choice, "model", 760, 190, 110, 24, "Resonator" },
    { W::Toggle, "tuned", 760, 226, 110, 24, "TUNED" },
    { W::Toggle, "ominous", 760, 440, 110, 30, "OMINOUS" },

    { W::Choice, "modSrc", 50, 566, 120, 24, "Mod source" },
    { W::SmallKnob, "lfoRate", 215, 572, 0, 0, "LFO rate" },
    { W::SmallKnob, "velStrength", 300, 572, 0, 0, "Vel>strength" },
    { W::SmallKnob, "a4", 730, 572, 0, 0, "A4", "Hz" },
    { W::SmallKnob, "level", 820, 572, 0, 0, "Level" },
  };
  auto* ed = new PanelEditor(*this, state, alloyTheme(), "ALLOY", "modal synthesis voice - after Elements by Emilie Gillet", 920, 650, L);
  auto* proc = this;
  ed->drawExtras = [proc](Graphics& g, PanelEditor& e) {
    // gate / exciter / resonator LEDs, like the three on the module
    e.drawLed(g, 450, 572, 6, proc->core.gate() || proc->vb("play") ? 1.0f : 0.0f, Colour(0xff3ad16b));
    e.drawLed(g, 520, 572, 6, jlimit(0.0f, 1.0f, proc->core.exciterMeter() * 2.0f), Colour(0xffff4a3d));
    e.drawLed(g, 590, 572, 6, jlimit(0.0f, 1.0f, proc->core.resonatorMeter() * 1.5f), Colour(0xff2f7fd6));
    g.setColour(e.theme().ink.withAlpha(0.8f));
    g.setFont(FontOptions(9.5f * e.scale(), Font::bold));
    g.drawText("GATE", e.unit(420, 584, 60, 12).toNearestInt(), Justification::centred);
    g.drawText("EXCITER", e.unit(490, 584, 60, 12).toNearestInt(), Justification::centred);
    g.drawText("RESONATOR", e.unit(555, 584, 70, 12).toNearestInt(), Justification::centred);
    // exciter signal-flow hint
    g.setColour(e.theme().ink.withAlpha(0.35f));
    g.drawArrow(Line<float>(e.unit(450, 125, 1, 1).getPosition(), e.unit(482, 125, 1, 1).getPosition()), 1.5f * e.scale(), 8 * e.scale(), 8 * e.scale());
  };
  return ed;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AlloyProcessor(); }
