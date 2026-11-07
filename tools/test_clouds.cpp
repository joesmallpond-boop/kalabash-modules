#include "../cores/CloudsCore.h"
#include "wav.h"
using namespace kb;
int main(){ const int sr=44100; CloudsCore c; c.prepare(sr);
 std::vector<float> AL,AR;
 for (int mode=0; mode<4; ++mode) for (int qual=0; qual<4; qual+=3) {
   CloudsParams p; p.mode=mode; p.quality=qual; p.dryWet=1.0f; p.density=0.7f; p.pitch=(mode==1)?7.0f:0.0f;
   int len=sr*3; std::vector<float> inL(len), inR(len), l(len), r(len);
   for(int i=0;i<len;++i){ float t=(float)i/sr; float env=std::exp(-6*fmodf(t,0.5f)); inL[i]=0.4f*env*std::sin(2*M_PI*(220+110*(int)(t*2)%3*110)*t); inR[i]=inL[i]; }
   for(int i=0;i<len;i+=256){ p.freeze = i>sr*2; c.process(p,&inL[i],&inR[i],&l[i],&r[i],std::min(256,len-i)); }
   Stats s=stats(l), t=stats(r), late=stats(l, sr*2, len); printf("mode %d q %d peak %.3f/%.3f rms %.3f frozen-rms %.3f %s\n",mode,qual,s.peak,t.peak,s.rms,late.rms,(s.nan||t.nan)?"NAN":"");
   AL.insert(AL.end(),l.begin(),l.end()); AR.insert(AR.end(),r.begin(),r.end()); }
 writeWav("/tmp/clouds.wav",AL,AR,sr); printf("latency %d\n", c.latency()); }
