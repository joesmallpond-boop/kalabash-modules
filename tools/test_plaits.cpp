#include "../cores/PlaitsCore.h"
#include "wav.h"
#include <cstdio>
using namespace kb;
int main() {
  const int sr = 44100;
  PlaitsCore core; core.prepare(sr);
  PlaitsParams p; p.voices = 4;
  // pitch check: drone, VA engine (ui 0)
  p.ampMode = 2; p.engine = 0; p.harmonics = 0.0f; p.timbre = 0.0f; p.morph = 0.0f;
  std::vector<float> L(sr*2), R(sr*2);
  core.noteOn(69, 1.0f);
  for (int i=0;i<sr*2;i+=256) core.render(p, &L[i], &R[i], std::min(256, sr*2-i));
  printf("A4 drone pitch = %.2f Hz\n", estimatePitch(L, sr, 4096, sr));
  core.allNotesOff();
  // each engine, pluck, chord
  p.ampMode = 0; p.harmonics=0.5f; p.timbre=0.5f; p.morph=0.5f;
  std::vector<float> AL, AR;
  for (int e=0;e<24;++e) {
    p.engine = e;
    int len = sr; std::vector<float> l(len), r(len);
    int notes[3] = {48, 55, 64};
    for (int i=0;i<len;i+=128) {
      if (i==0) for (int nn: notes) core.noteOn(nn, 0.9f);
      if (i==sr/2) for (int nn: notes) core.noteOff(nn);
      core.render(p, &l[i], &r[i], std::min(128, len-i));
    }
    Stats s = stats(l), sr2 = stats(r);
    printf("engine %2d active=%2d peak %.3f rms %.3f | R peak %.3f %s\n", e, core.activeEngine(), s.peak, s.rms, sr2.peak, (s.nan||sr2.nan)?"NAN!":"");
    AL.insert(AL.end(), l.begin(), l.end()); AR.insert(AR.end(), r.begin(), r.end());
  }
  writeWav("/tmp/plaits_all.wav", AL, AR, sr);
}
