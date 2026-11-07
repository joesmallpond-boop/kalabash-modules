// FOGBANK core — wrapper around the Clouds texture synthesizer DSP
// (c) Emilie Gillet, MIT). Runs at 32 kHz like the module.
#pragma once

#include <vector>
#include <cstdint>
#include "../common/StreamResampler.h"

namespace clouds { class GranularProcessor; }

namespace kb {

struct CloudsParams {
  int mode = 0;            // 0 granular, 1 stretch, 2 looping delay, 3 spectral
  int quality = 0;         // 0 16-bit stereo, 1 16-bit mono, 2 8-bit u-law stereo, 3 8-bit mono
  float position = 0.5f, size = 0.5f, pitch = 0.0f /*semitones*/, density = 0.5f, texture = 0.5f;
  float dryWet = 0.5f, spread = 0.5f, feedback = 0.0f, reverb = 0.3f;
  float inGain = 1.0f;     // linear
  float outGain = 1.0f;
  bool freeze = false;
  bool trigger = false;    // rising edge = trigger
};

class CloudsCore {
 public:
  CloudsCore();
  ~CloudsCore();
  void prepare(double hostRate);
  void process(const CloudsParams& p, const float* inL, const float* inR, float* outL, float* outR, int n);
  int latency() const { return bridge_.latencySamples(); }
  float inMeter() const { return inMeter_; }
  float outMeter() const { return outMeter_; }

 private:
  void processNative(const CloudsParams& p, const float* il, const float* ir, float* l, float* r, int n);
  clouds::GranularProcessor* proc_ = nullptr;
  std::vector<uint8_t> large_, small_;
  RateBridge bridge_;
  bool prevTrig_ = false;
  bool pendingTrig_ = false;
  float inMeter_ = 0, outMeter_ = 0;
};

}  // namespace kb
