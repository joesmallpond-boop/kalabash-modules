// Minimal stand-in for the Max runtime: loads a kalabash external (.so built on Linux)
// and drives it like the Max for Live device would (params, MIDI, transport, perform).
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "ext_itm.h"
#include <dlfcn.h>
#include <map>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

struct MockClass { method mnew, mfree; long size; std::map<std::string, method> methods; };
static std::map<std::string, t_symbol*> syms;
static MockClass* g_cls_created = nullptr;
static MockClass* g_cls = nullptr;
static int warnings = 0;
static double g_ticks = 0, g_bpm = 120, g_sr = 48000; static long g_play = 1;
typedef void (*perf64)(void*, t_object*, double**, long, double**, long, long, long, void*);
static perf64 g_perf = nullptr;

extern "C" {
t_symbol* gensym(C74_CONST char* s) { auto it = syms.find(s); if (it != syms.end()) return it->second; auto* t = new t_symbol; t->s_name = strdup(s); t->s_thing = nullptr; syms[s] = t; return t; }
t_class* class_new(C74_CONST char*, C74_CONST method mnew, C74_CONST method mfree, long size, C74_CONST method, short, ...) { auto* c = new MockClass{ mnew, mfree, size, {} }; g_cls_created = c; return (t_class*) c; }
t_max_err class_addmethod(t_class* c, C74_CONST method m, C74_CONST char* name, ...) { ((MockClass*) c)->methods[name] = m; return 0; }
void class_dspinit(t_class*) {}
t_max_err class_register(t_symbol*, t_class*) { return 0; }
void* object_alloc(t_class* c) { g_cls = (MockClass*) c; return calloc(1, g_cls->size); }
void z_dsp_setup(t_pxobject*, long) {}
void z_dsp_free(t_pxobject*) {}
void* outlet_new(void*, C74_CONST char*) { return (void*) 1; }
t_atom_float atom_getfloat(C74_CONST t_atom* a) { return a->a_type == A_FLOAT ? a->a_w.w_float : (t_atom_float) a->a_w.w_long; }
void object_warn(t_object*, C74_CONST char* fmt, ...) { va_list ap; va_start(ap, fmt); printf("  [warn] "); vprintf(fmt, ap); printf("\n"); va_end(ap); ++warnings; }
void* object_method_imp(void*, void*, void* x, void* fn, void*, void*, void*, void*, void*, void*) { g_perf = (perf64) fn; return nullptr; }
float sys_getsr(void) { return (float) g_sr; }
static int itm_dummy;
void* itm_getglobal(void) { return &itm_dummy; }
void itm_reference(t_itm*) {}
void itm_dereference(t_itm*) {}
double itm_getticks(t_itm*) { return g_ticks; }
long itm_getstate(t_itm*) { return g_play; }
double itm_tickstoms(t_itm*, double ticks) { return ticks / 480.0 * 60000.0 / g_bpm; }
}

static void* obj;
static void send(const char* name, double v) {
  t_atom a; a.a_type = A_FLOAT; a.a_w.w_float = v;
  auto it = g_cls->methods.find("anything");
  ((void (*)(void*, t_symbol*, long, t_atom*)) it->second)(obj, gensym(name), 1, &a);
}
static void midi(int b) { ((void (*)(void*, long)) g_cls->methods["midi"])(obj, b); }

int main(int argc, char** argv) {
  void* h = dlopen(argv[1], RTLD_NOW);
  if (!h) { printf("dlopen: %s\n", dlerror()); return 1; }
  ((void (*)(void*)) dlsym(h, "ext_main"))(nullptr);
  g_cls = g_cls_created;
  obj = ((void* (*)(t_symbol*, long, t_atom*)) g_cls->mnew)(nullptr, 0, nullptr);
  bool synth = std::string(argv[2]) == "synth";
  // params from a list file: "name value" per line
  FILE* f = fopen(argv[3], "r"); char nm[128]; double v; int np = 0;
  while (f && fscanf(f, "%127s %lf", nm, &v) == 2) { send(nm, v); ++np; }
  if (f) fclose(f);
  ((void (*)(void*, t_object*, short*, double, long, long)) g_cls->methods["dsp64"])(obj, nullptr, nullptr, g_sr, 128, 0);
  const int vs = 128; std::vector<double> il(vs), ir(vs), ol(vs), orr(vs);
  double* ins[2] = { il.data(), ir.data() }; double* outs[2] = { ol.data(), orr.data() };
  double peak = 0, acc = 0; long N = 0; int nan = 0; long t = 0;
  for (int b = 0; b < (int) (g_sr * 4 / vs); ++b) {
    if (synth && b % 100 == 0) { midi(0x90); midi(48 + (b / 100) % 12); midi(100); }
    if (synth && b % 100 == 60) { midi(0x80); midi(48 + (b / 100) % 12); midi(0); }
    for (int i = 0; i < vs; ++i, ++t) { double tt = t / g_sr; il[i] = ir[i] = 0.3 * sin(2 * M_PI * 220 * tt) * exp(-4 * fmod(tt, 0.5)); }
    g_ticks = t / g_sr * g_bpm / 60.0 * 480.0;
    g_perf(obj, nullptr, ins, 2, outs, 2, vs, 0, nullptr);
    for (int i = 0; i < vs; ++i) { if (!std::isfinite(ol[i])) ++nan; peak = std::max(peak, std::fabs(ol[i])); acc += ol[i] * ol[i]; ++N; }
  }
  printf("  params sent %d, warnings %d, peak %.3f rms %.4f nan %d\n", np, warnings, peak, std::sqrt(acc / N), nan);
  ((void (*)(void*)) g_cls->mfree)(obj);
  return 0;
}
