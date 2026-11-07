// KNOTWORK core — polyphonic wrapper around the Plaits macro-oscillator DSP
// (c) Emilie Gillet, MIT). JUCE-independent so it can be tested offline.
#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include "../common/StreamResampler.h"

namespace plaits { class Voice; }

namespace kb {

struct PlaitsParams {
  int engine = 8;              // 0..23 (0-7 bank "new", 8-15 pitched, 16-23 noise/perc)
  float octave = 0.0f;         // coarse transpose, semitones (-48..+48)
  float fine = 0.0f;           // semitones -1..+1
  float harmonics = 0.5f;
  float timbre = 0.5f;
  float morph = 0.5f;
  float timbreAmt = 0.0f;      // -1..1 internal envelope amount
  float fmAmt = 0.0f;
  float morphAmt = 0.0f;
  float decay = 0.5f;          // LPG decay / envelope decay
  float lpgColour = 0.5f;      // VCFA <-> VCA
  int ampMode = 0;             // 0 = pluck (trigger strikes LPG), 1 = gate (LPG follows gate), 2 = drone
  float level = 0.8f;          // output level
  float auxMix = 0.0f;         // 0 = OUT only on both channels, 1 = OUT left / AUX right
  int voices = 4;              // 1..8
  float glide = 0.0f;          // 0..1
  float bendRange = 2.0f;
  float pitchBend = 0.0f;      // -1..1
  float modWheel = 0.0f;       // 0..1 -> morph modulation
  float velToTimbre = 0.0f;    // 0..1
};

class PlaitsCore {
 public:
  static constexpr int kMaxVoices = 8;
  PlaitsCore();
  ~PlaitsCore();

  void prepare(double hostRate);
  void noteOn(int note, float velocity);
  void noteOff(int note);
  void allNotesOff();
  void render(const PlaitsParams& p, float* outL, float* outR, int n);
  int activeEngine() const { return activeEngine_; }
  float meter() const { return meter_; }

 private:
  struct Slot {
    plaits::Voice* voice = nullptr;
    std::vector<char> mem;
    int note = -1;
    float targetNote = 60.0f;
    float currentNote = 60.0f;
    float velocity = 0.0f;
    bool gate = false;
    int retrigBlocks = 0;      // force trigger low for N blocks before rising
    uint32_t age = 0;
    bool everPlayed = false;
    int silentBlocks = 0;
  };
  void renderNative(const PlaitsParams& p, float* l, float* r, int n);
  int allocate(int note);

  std::array<Slot, kMaxVoices> slots_;
  RateBridge bridge_;
  uint32_t clock_ = 0;
  int activeEngine_ = 0;
  float meter_ = 0.0f;
  const PlaitsParams* current_ = nullptr;
  std::vector<int> heldStack_;  // for mono legato
};

}  // namespace kb
