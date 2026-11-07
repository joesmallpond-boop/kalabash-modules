#include "../cores/BeadsCore.h"
#include "wav.h"
using namespace kb;
int main(){ const int sr=44100; BeadsCore c; c.prepare(sr); std::vector<float> AL,AR;
 for (int cfg=0; cfg<7; ++cfg) { BeadsParams p; p.quality = cfg<4?cfg:0; p.dryWet=1; p.density=0.7f; p.size=0.4f; p.timeAmt=0.4f; p.time=0.05f; p.reverb=0.3f;
   if(cfg==4){ p.size=1.0f; p.feedback=0.5f; } // delay mode
   if(cfg==5){ p.synth=true; }
   if(cfg==6){ p.latched=false; p.seedPatched=true; }
   int len=sr*4; std::vector<float> in(len),l(len),r(len);
   for(int i=0;i<len;++i){ float t=(float)i/sr; in[i]=0.4f*std::exp(-5*fmodf(t,0.4f))*std::sin(2*M_PI*(200+100*((int)(t/0.4f)%4))*t); }
   for(int i=0;i<len;i+=256){ p.seedGate = cfg==6 && ((i/(sr/4))%2==0); c.process(p,&in[i],&in[i],&l[i],&r[i],std::min(256,len-i)); }
   Stats s=stats(l,sr,len), t=stats(r,sr,len); printf("cfg %d peak %.3f/%.3f rms %.3f lat %d %s\n",cfg,s.peak,t.peak,s.rms,c.latency(),(s.nan||t.nan)?"NAN":"");
   AL.insert(AL.end(),l.begin(),l.end()); AR.insert(AR.end(),r.begin(),r.end()); }
 writeWav("/tmp/beads.wav",AL,AR,sr); printf("pitch law: %.2f %.2f %.2f %.2f\n", BeadsCore::pitchKnobToSemitones(0.5f),BeadsCore::pitchKnobToSemitones(0.75f),BeadsCore::pitchKnobToSemitones(1.0f),BeadsCore::pitchKnobToSemitones(0.0f)); }
