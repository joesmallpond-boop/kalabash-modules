// ALLOY core — wrapper around the Elements modal synthesis DSP
// (c) Emilie Gillet, MIT). Monophonic, as on the module.
#pragma once

#include <vector>
#include <cstdint>
#include "../common/StreamResampler.h"

namespace elements { class Part; }

namespace kb {

enum ModSource { MOD_VELOCITY = 0, MOD_WHEEL, MOD_AFTERTOUCH, MOD_LFO, MOD_RANDOM, MOD_LAST };

struct ElementsParams {
  // exciter
  float contour = 0.5f;          // envelope shape
  float bowLevel = 0.0f, blowLevel = 0.0f, strikeLevel = 0.8f;
  float flow = 0.5f;             // blow meta
  float mallet = 0.5f;           // strike meta
  float bowTimbre = 0.5f, blowTimbre = 0.5f, strikeTimbre = 0.5f;
  // resonator
  float coarse = 0.0f;           // semitones -30..30
  float fine = 0.0f;             // semitones -1..1
  float fmAmt = 0.0f;            // depth of mod source on pitch (-1..1) -> +/- 12 st
  float geometry = 0.5f, brightness = 0.5f, damping = 0.5f, position = 0.3f, space = 0.4f;
  // modulation attenuverters (-1..1) applied from the selected mod source
  float modBowTimbre = 0, modFlow = 0, modBlowTimbre = 0, modMallet = 0, modStrikeTimbre = 0;
  float modGeometry = 0, modBrightness = 0, modDamping = 0, modPosition = 0, modSpace = 0;
  int modSource = MOD_VELOCITY;
  float lfoRate = 0.3f;          // 0..1 -> 0.05..20 Hz
  int model = 0;                 // 0 modal, 1 string, 2 strings
  bool ominous = false;          // the "easter egg" mode
  bool play = false;             // panel PLAY button (gate)
  float extIn = 0.0f;            // external audio into exciter
  float level = 0.8f;
  float bendRange = 2.0f;
  float pitchBend = 0.0f;
  float modWheel = 0.0f;
  float aftertouch = 0.0f;
  float velToStrength = 1.0f;
  bool tuned = true;              // compensate string-model delay so MIDI notes are in tune
  float stringComp = 0.35f;       // samples (calibrated)
};

class ElementsCore {
 public:
  ElementsCore();
  ~ElementsCore();
  void prepare(double hostRate);
  void noteOn(int note, float velocity);
  void noteOff(int note);
  void allNotesOff();
  // in may be null. outL/outR host-rate.
  void render(const ElementsParams& p, const float* inL, const float* inR, float* outL, float* outR, int n);
  float exciterMeter() const { return exciterMeter_; }
  float resonatorMeter() const { return resonatorMeter_; }
  bool gate() const { return gate_; }
  int latency() const { return bridge_.latencySamples(); }

 private:
  void initPatch();
  void renderNative(const ElementsParams& p, const float* inL, const float* inR, float* l, float* r, int n);
  elements::Part* part_ = nullptr;
  std::vector<uint16_t> reverb_;
  RateBridge bridge_;
  std::vector<int> held_;
  int note_ = 60;
  float velocity_ = 0.8f;
  float randomValue_ = 0.0f;
  bool gate_ = false;
  bool retrigger_ = false;
  float lfoPhase_ = 0.0f;
  float exciterMeter_ = 0.0f, resonatorMeter_ = 0.0f;
  int model_ = -1;
  bool ominous_ = false;
  float strikeLevel_ = 0.0f, blowLevel_ = 0.0f;
  uint32_t rng_ = 0x1234567u;
};

}  // namespace kb
