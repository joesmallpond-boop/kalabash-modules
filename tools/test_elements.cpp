#include "../cores/ElementsCore.h"
#include "wav.h"
using namespace kb;
int main(){ const int sr=48000; ElementsCore c; c.prepare(sr); ElementsParams p;
 std::vector<float> AL,AR;
 // pitch: string model, strike, sustained bow
 for (int test=0; test<6; ++test) {
   ElementsParams q = p;
   if (test==1){ q.bowLevel=0.8f; q.strikeLevel=0; }
   if (test==2){ q.blowLevel=0.8f; q.strikeLevel=0; }
   if (test==3){ q.model=1; }
   if (test==4){ q.model=2; }
   if (test==5){ q.ominous=true; q.bowLevel=0.6f; }
   int len=sr*2; std::vector<float> l(len), r(len);
   for(int i=0;i<len;i+=200){ if(i==0) c.noteOn(57,0.9f); if(i==sr) c.noteOff(57); c.render(q,nullptr,nullptr,&l[i],&r[i],std::min(200,len-i)); }
   int zc=0; for(int i=sr/2;i<sr;++i) if(r[i]<=0&&r[i+1]>0) zc++;
   Stats s=stats(l), t=stats(r); printf("test %d peakL %.3f peakR %.3f rms %.3f zcHz %d %s\n",test,s.peak,t.peak,t.rms,zc*2,(s.nan||t.nan)?"NAN":"");
   AL.insert(AL.end(),l.begin(),l.end()); AR.insert(AR.end(),r.begin(),r.end()); }
 writeWav("/tmp/elements.wav",AL,AR,sr); printf("latency %d\n", c.latency()); }
