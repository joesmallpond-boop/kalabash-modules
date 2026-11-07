// The Mutable Instruments firmware was written for a microcontroller where all
// DSP objects are globals (zero-initialised .bss). Some members are never set by
// Init() and rely on that. Allocate firmware objects in zeroed memory to match.
#pragma once
#include <cstdlib>
#include <new>
namespace kb {
template <class T> T* newZeroed() {
  void* m = std::calloc(1, sizeof(T));
  if (!m) throw std::bad_alloc();
  return new (m) T();
}
template <class T> void deleteZeroed(T* p) {
  if (!p) return;
  p->~T();
  std::free(p);
}
}  // namespace kb
