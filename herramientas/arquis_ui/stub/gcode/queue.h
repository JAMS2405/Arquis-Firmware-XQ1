// STUB: queue.h real: RingBuffer::full(uint8_t cmdCount=1) const; static void inject(FSTR_P) y inject(const char * const)
#pragma once
#include "../MarlinCore.h"
struct RingBufferStub { bool full(uint8_t cmdCount = 1) const; };
class GCodeQueue { public: static RingBufferStub ring_buffer; static void inject(FSTR_P const fgcode); static void inject(const char * const gcode); }; extern GCodeQueue queue;
