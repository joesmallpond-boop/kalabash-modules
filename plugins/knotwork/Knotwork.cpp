// KNOTWORK — macro-oscillator instrument (DSP: Mutable Instruments Plaits by Emilie Gillet, MIT)
#include "../../common/KbProcessor.h"
#include "../../common/PanelUI.h"
#include "../../cores/PlaitsCore.h"
#include <set>

using namespace juce;
using namespace kb;

static const StringArray kModelNames {
  // bank A — pitched (original bank 1)
  "Virtual analog", "Waveshaper", "Two-op FM", "Formant / grain", "Additive", "Wavetable", "Chords", "Speech",
  // bank B — noise & percussion (original bank 2)
  "Swarm", "Filtered noise", "Particles", "String", "Modal", "Bass drum", "Snare drum", "Hi-hat",
  // bank C — extended engines
  "VA + filter", "Phase distortion", "Six-op FM I", "Six-op FM II", "Six-op FM III", "Wave terrain", "String machine", "Chiptune"
};

// what HARMONICS / TIMBRE / MORPH do in each model (shown under the knobs)
static const char* kModelHints[24][3] = {
  { "detune", "square / sync", "saw shape" }, { "waveshape", "fold amount", "asymmetry" },
  { "ratio", "mod index", "feedback" }, { "formant ratio", "formant freq", "width / shape" },
  { "bumps", "dominant harm.", "bump shape" }, { "bank", "row", "column" },
  { "chord type", "inversion", "waveform" }, { "synth type", "species", "phoneme" },
  { "pitch random", "density", "duration" }, { "filter type", "clock freq", "resonance" },
  { "freq random", "density", "filter type" }, { "inharmonicity", "brightness", "decay" },
  { "material", "brightness", "decay" }, { "punch / drive", "tone", "decay" },
  { "tone / noise", "drum modes", "decay" }, { "metal / noise", "highpass", "decay" },
  { "filter type", "cutoff", "waveform" }, { "PD type", "PD amount", "PD shape" },
  { "patch", "brightness", "envelope" }, { "patch", "brightness", "envelope" },
  { "patch", "brightness", "envelope" }, { "terrain", "path radius", "path offset" },
  { "chord", "filter", "ensemble" }, { "chord / arp", "PW / sync", "bass / chord" },
};

class KnotworkProcessor : public KbProcessor {
 public:
  KnotworkProcessor()
      : KbProcessor(BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true), makeLayout()) {}

  static Layout makeLayout() {
    Layout l;
    l.add(cparam("model", "Model", kModelNames, 0));
    l.add(cparam("octave", "Octave", { "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4" }, 4));
    l.add(fparam("tune", "Frequency", -12.0f, 12.0f, 0.0f, -1.0f, "st"));
    l.add(bparam("tuned", "Tuned", true));
    l.add(fparam("cents", "Fine", -50.0f, 50.0f, 0.0f, -1.0f, "ct"));
    l.add(fparam("a4", "A4 Reference", 415.0f, 466.0f, 440.0f, -1.0f, "Hz"));
    l.add(fparam("harmonics", "Harmonics", 0.0f, 1.0f, 0.5f));
    l.add(fparam("timbre", "Timbre", 0.0f, 1.0f, 0.5f));
    l.add(fparam("morph", "Morph", 0.0f, 1.0f, 0.5f));
    l.add(fparam("timbreAmt", "Timbre Env", -1.0f, 1.0f, 0.0f));
    l.add(fparam("fmAmt", "FM Env", -1.0f, 1.0f, 0.0f));
    l.add(fparam("morphAmt", "Morph Env", -1.0f, 1.0f, 0.0f));
    l.add(fparam("decay", "LPG Decay", 0.0f, 1.0f, 0.5f));
    l.add(fparam("colour", "LPG Colour", 0.0f, 1.0f, 0.5f));
    l.add(cparam("amp", "Amp Mode", { "Pluck", "Gate", "Drone" }, 0));
    l.add(cparam("voices", "Voices", { "1", "2", "3", "4", "5", "6", "7", "8" }, 3));
    l.add(fparam("glide", "Glide", 0.0f, 1.0f, 0.0f));
    l.add(fparam("aux", "Aux Mix", 0.0f, 1.0f, 0.0f));
    l.add(fparam("level", "Level", 0.0f, 1.5f, 0.8f));
    l.add(fparam("velTimbre", "Vel > Timbre", 0.0f, 1.0f, 0.0f));
    l.add(iparam("bend", "Bend Range", 0, 24, 2));
    return l;
  }

  void prepareToPlay(double sr, int) override { core.prepare(sr); }
  bool isBusesLayoutSupported(const BusesLayout& l) const override {
    return l.getMainOutputChannelSet() == AudioChannelSet::stereo() || l.getMainOutputChannelSet() == AudioChannelSet::mono();
  }

  void processBlock(AudioBuffer<float>& buf, MidiBuffer& midi) override {
    ScopedNoDenormals nd;
    updateParams();
    const int n = buf.getNumSamples();
    float* L = buf.getWritePointer(0);
    float* R = buf.getNumChannels() > 1 ? buf.getWritePointer(1) : nullptr;
    tmpR.resize((size_t) n);
    if (!R) R = tmpR.data();
    int pos = 0;
    for (const auto meta : midi) {
      const int at = jlimit(0, n, meta.samplePosition);
      if (at > pos) { core.render(p, L + pos, R + pos, at - pos); pos = at; }
      handle(meta.getMessage());
    }
    if (n > pos) core.render(p, L + pos, R + pos, n - pos);
    midi.clear();
  }

  void handle(const MidiMessage& m) {
    if (m.isNoteOn()) { sustained.erase(m.getNoteNumber()); core.noteOn(m.getNoteNumber(), m.getFloatVelocity()); }
    else if (m.isNoteOff()) { if (sustain) sustained.insert(m.getNoteNumber()); else core.noteOff(m.getNoteNumber()); }
    else if (m.isPitchWheel()) p.pitchBend = (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isController() && m.getControllerNumber() == 1) p.modWheel = m.getControllerValue() / 127.0f;
    else if (m.isSustainPedalOn()) sustain = true;
    else if (m.isSustainPedalOff()) { sustain = false; for (int nn : sustained) core.noteOff(nn); sustained.clear(); }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) { core.allNotesOff(); sustained.clear(); }
  }

  void updateParams() {
    p.engine = vi("model");
    p.octave = (float) (vi("octave") - 4) * 12.0f;
    // TUNED: the FREQUENCY knob snaps to semitones so MIDI notes stay in tune;
    // FINE (cents) and the A4 reference give deliberate detuning / alternate concert pitch.
    const float coarse = vb("tuned") ? std::round(v("tune")) : v("tune");
    p.fine = coarse + v("cents") / 100.0f + 12.0f * std::log2(v("a4") / 440.0f);
    p.harmonics = v("harmonics"); p.timbre = v("timbre"); p.morph = v("morph");
    p.timbreAmt = v("timbreAmt"); p.fmAmt = v("fmAmt"); p.morphAmt = v("morphAmt");
    p.decay = v("decay"); p.lpgColour = v("colour");
    p.ampMode = vi("amp"); p.voices = vi("voices") + 1; p.glide = v("glide");
    p.auxMix = v("aux"); p.level = v("level"); p.velToTimbre = v("velTimbre"); p.bendRange = (float) vi("bend");
  }

  AudioProcessorEditor* createEditor() override;

  PlaitsCore core;
  PlaitsParams p;
  std::vector<float> tmpR;
  bool sustain = false;
  std::set<int> sustained;
};

//------------------------------------------------------------------------------

static Theme knotTheme() {
  return { Colour(0xff23262b), Colour(0xff0d0e10), Colour(0xffe8e2d4), Colour(0xffe8a33d), Colour(0xff4a4f57), Colour(0x14ffffff) };
}

AudioProcessorEditor* KnotworkProcessor::createEditor() {
  // Layout follows the module: model selector on top, FREQUENCY / HARMONICS big knobs,
  // TIMBRE / MORPH medium knobs, three attenuverters, then the hidden LPG settings
  // brought out to their own section, plus plugin-only voice controls.
  std::vector<Widget> L {
    { W::Section, "", 20, 64, 480, 196, "" },
    { W::Section, "", 20, 270, 480, 244, "Oscillator" },
    { W::Section, "", 20, 524, 230, 112, "Low-pass gate" },
    { W::Section, "", 260, 524, 240, 112, "Voice" },

    { W::BigKnob, "tune", 120, 330, 0, 0, "Frequency" },
    { W::BigKnob, "harmonics", 400, 330, 0, 0, "Harmonics" },
    { W::Knob, "timbre", 165, 400, 0, 0, "Timbre" },
    { W::Knob, "morph", 355, 400, 0, 0, "Morph" },
    { W::Atten, "timbreAmt", 190, 478, 0, 0, "env>timbre" },
    { W::Atten, "fmAmt", 260, 478, 0, 0, "env>freq" },
    { W::Atten, "morphAmt", 330, 478, 0, 0, "env>morph" },
    { W::Choice, "octave", 228, 300, 64, 22, "Octave" },
    { W::Toggle, "tuned", 228, 330, 64, 24, "TUNED" },
    { W::SmallKnob, "cents", 60, 400, 0, 0, "Fine", "cents" },
    { W::SmallKnob, "a4", 460, 400, 0, 0, "A4", "Hz" },
    { W::SmallKnob, "aux", 260, 395, 0, 0, "Out/Aux" },

    { W::Knob, "decay", 75, 579, 0, 0, "Decay" },
    { W::Knob, "colour", 160, 579, 0, 0, "Colour", "VCFA - VCA" },
    { W::Choice, "amp", 198, 600, 0, 0, "" },

    { W::Choice, "voices", 280, 562, 56, 22, "Voices" },
    { W::Choice, "amp", 280, 604, 56, 22, "Amp" },
    { W::SmallKnob, "glide", 372, 574, 0, 0, "Glide" },
    { W::SmallKnob, "velTimbre", 424, 574, 0, 0, "Vel>Tmb" },
    { W::SmallKnob, "level", 474, 574, 0, 0, "Level" },
  };
  // remove the placeholder duplicate entry (index with w==0 choice)
  L.erase(std::remove_if(L.begin(), L.end(), [](const Widget& w) { return w.type == W::Choice && w.w == 0; }), L.end());

  auto* ed = new PanelEditor(*this, state, knotTheme(), "KNOTWORK", "macro oscillator - after Plaits by Emilie Gillet", 520, 676, L);
  auto* proc = this;
  // model grid geometry
  struct Grid { static Rectangle<float> cell(int i) {
    const int bank = i / 8, row = i % 8;
    return { 34.0f + bank * 158.0f, 88.0f + row * 20.5f, 150.0f, 19.0f }; } };

  ed->drawExtras = [proc](Graphics& g, PanelEditor& e) {
    const int sel = proc->vi("model");
    const int active = (proc->core.activeEngine() + 16) % 24;
    const char* banks[3] = { "PITCHED", "NOISE / PERC", "EXTENDED" };
    for (int b = 0; b < 3; ++b) {
      g.setColour(e.theme().ink.withAlpha(0.5f));
      g.setFont(FontOptions(9.0f * e.scale(), Font::bold));
      g.drawText(banks[b], e.unit(34.0f + b * 158.0f + 22, 72, 128, 12).toNearestInt(), Justification::centredLeft);
    }
    for (int i = 0; i < 24; ++i) {
      auto c = Grid::cell(i);
      const bool on = i == sel;
      if (on) { g.setColour(e.theme().accent.withAlpha(0.13f)); g.fillRoundedRectangle(e.unit(c.getX(), c.getY(), c.getWidth(), c.getHeight()), 4.0f * e.scale()); }
      e.drawLed(g, c.getX() + 9, c.getCentreY(), 4.2f, on ? 1.0f : (i == active ? 0.35f : 0.0f), e.theme().accent);
      g.setColour(e.theme().ink.withAlpha(on ? 1.0f : 0.7f));
      g.setFont(FontOptions(11.0f * e.scale(), on ? Font::bold : Font::plain));
      g.drawText(kModelNames[i], e.unit(c.getX() + 20, c.getY(), c.getWidth() - 20, c.getHeight()).toNearestInt(), Justification::centredLeft);
    }
    // per-model hints under the three timbral knobs
    g.setColour(e.theme().accent.withAlpha(0.85f));
    g.setFont(FontOptions(9.5f * e.scale()));
    g.drawText(kModelHints[sel][0], e.unit(350, 375, 100, 12).toNearestInt(), Justification::centred);
    g.drawText(kModelHints[sel][1], e.unit(115, 437, 100, 12).toNearestInt(), Justification::centred);
    g.drawText(kModelHints[sel][2], e.unit(305, 437, 100, 12).toNearestInt(), Justification::centred);
    // output meter
    const float m = jlimit(0.0f, 1.0f, proc->core.meter());
    e.drawLed(g, 478, 292, 4.0f, m, e.theme().accent);
  };
  ed->onClick = [proc](const MouseEvent& ev, PanelEditor& e) {
    const auto pt = ev.position / e.scale();
    for (int i = 0; i < 24; ++i)
      if (Grid::cell(i).contains(pt)) {
        if (auto* prm = proc->state.getParameter("model")) {
          prm->beginChangeGesture();
          prm->setValueNotifyingHost(prm->convertTo0to1((float) i));
          prm->endChangeGesture();
        }
      }
  };
  return ed;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KnotworkProcessor(); }
