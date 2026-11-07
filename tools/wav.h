#pragma once
#include <cstdio>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
inline void writeWav(const char* path, const std::vector<float>& l, const std::vector<float>& r, int sr) {
  FILE* f = fopen(path, "wb"); if (!f) return;
  uint32_t n = (uint32_t) l.size(); uint32_t data = n * 4;
  auto w32 = [&](uint32_t v){ fwrite(&v,4,1,f); }; auto w16 = [&](uint16_t v){ fwrite(&v,2,1,f); };
  fwrite("RIFF",1,4,f); w32(36+data); fwrite("WAVEfmt ",1,8,f); w32(16); w16(1); w16(2); w32(sr); w32(sr*4); w16(4); w16(16);
  fwrite("data",1,4,f); w32(data);
  for (uint32_t i=0;i<n;++i){ int16_t a=(int16_t)std::max(-32767.f,std::min(32767.f,l[i]*32767.f)); int16_t b=(int16_t)std::max(-32767.f,std::min(32767.f,r[i]*32767.f)); fwrite(&a,2,1,f); fwrite(&b,2,1,f);} fclose(f);
}
struct Stats { float peak=0, rms=0; bool nan=false; };
inline Stats stats(const std::vector<float>& x, size_t from=0, size_t to=SIZE_MAX) {
  Stats s; double acc=0; to = std::min(to, x.size()); for (size_t i=from;i<to;++i){ if(!std::isfinite(x[i])) s.nan=true; s.peak=std::max(s.peak,std::fabs(x[i])); acc+=x[i]*x[i]; }
  s.rms = to>from ? (float)std::sqrt(acc/(to-from)) : 0; return s;
}
// autocorrelation pitch estimate
inline float estimatePitch(const std::vector<float>& x, size_t from, size_t len, int sr) {
  int minLag = sr/2000, maxLag = sr/30; double best=-1; int bestLag=0;
  std::vector<double> c(maxLag+2,0);
  for (int lag=minLag; lag<=maxLag+1; ++lag){ double acc=0; for(size_t i=from;i<from+len;++i) acc+=x[i]*x[i+lag]; c[lag]=acc; }
  for (int lag=minLag+1; lag<=maxLag; ++lag){ if (c[lag]>c[lag-1] && c[lag]>=c[lag+1] && c[lag] > 0.8*c[minLag>0?minLag:1] ) { } }
  // pick global max beyond first zero crossing
  int lag=minLag; while (lag<maxLag && c[lag]>0) ++lag;
  for (; lag<=maxLag; ++lag) if (c[lag]>best){best=c[lag];bestLag=lag;}
  if (bestLag<=minLag||bestLag>=maxLag) return 0;
  double a=c[bestLag-1], b=c[bestLag], cc=c[bestLag+1]; double off=0.5*(a-cc)/(a-2*b+cc);
  return (float)(sr/(bestLag+off));
}
