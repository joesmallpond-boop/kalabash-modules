// ABACUS core — wrapper around the Beads granular texture synthesizer DSP
// (c) Emilie Gillet, MIT).
#pragma once

#include <vector>
#include <cstdint>
#include "../common/StreamResampler.h"

namespace beads { class Processor; class Synth; }

namespace kb {

struct BeadsParams {
  // grain parameters (0..1) and pitch (semitones, with the module's snapping law applied by caller)
  float time = 0.5f, size = 0.5f, shape = 0.5f, pitchKnob = 0.5f /*0..1 raw knob*/, density = 0.5f;
  // attenurandomizers (-1..1): >0 = random/smooth modulation depth; <0 = S&H/peaky random
  float timeAmt = 0.0f, sizeAmt = 0.0f, shapeAmt = 0.0f, pitchAmt = 0.0f;
  float feedback = 0.0f, dryWet = 0.5f, reverb = 0.0f;
  int quality = 3;          // 0 48k/16b, 1 32k/12b (cloudy), 2 24k/12b hi-fi dry (clean tape), 3 24k u-law (dirty tape)
  bool mono = false;
  bool freeze = false;
  bool seedButton = false;  // momentary SEED
  bool latched = true;      // SEED latch (continuous grains)
  bool seedGate = false;    // external gate / clock (from host tempo or MIDI)
  bool seedPatched = false; // true when an external seed source is active (like a patched SEED jack)
  bool synth = false;       // wavetable synth mode (the module's behaviour with nothing patched)
  float inGain = 1.0f;
  float outGain = 1.0f;
};

class BeadsCore {
 public:
  BeadsCore();
  ~BeadsCore();
  void prepare(double hostRate);
  void process(const BeadsParams& p, const float* inL, const float* inR, float* outL, float* outR, int n);
  int latency() const { return bridge_.latencySamples(); }
  bool latencyChanged() { bool c = latencyChanged_; latencyChanged_ = false; return c; }
  float inMeter() const { return inMeter_; }
  float outMeter() const { return outMeter_; }
  float reserve() const { return reserve_; }
  bool seedActivity() { bool s = seedActivity_; seedActivity_ = false; return s; }

  // The module's PITCH knob law: snaps to musical intervals (0, 1, 3, 4, 7, 12, 24 st).
  static float pitchKnobToSemitones(float knob);

 private:
  void configure(int media, bool mono, bool synth);
  void processNative(const BeadsParams& p, const float* il, const float* ir, float* l, float* r, int n);
  beads::Processor* proc_ = nullptr;
  beads::Synth* synth_ = nullptr;
  std::vector<uint8_t> buf1_, buf2_;
  std::vector<uint16_t> reverb_;
  RateBridge bridge_;
  double hostRate_ = 48000.0;
  int media_ = -1;
  bool mono_ = false, synthMode_ = false;
  int block_ = 12;
  uint8_t prevSeed_ = 0;
  float inMeter_ = 0, outMeter_ = 0, reserve_ = 1.0f;
  bool latencyChanged_ = false, seedActivity_ = false;
  bool configured_ = false;
};

}  // namespace kb
