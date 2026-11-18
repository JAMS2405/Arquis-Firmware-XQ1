#pragma once
// Stopwatch::duration() real: static millis_t duration() -> segundos
// printStatistics real (printcounter.h:34): uint16_t totalPrints, finishedPrints; uint32_t printTime, longestPrint; float filamentUsed
struct printStatistics { uint16_t totalPrints; uint16_t finishedPrints; uint32_t printTime; uint32_t longestPrint; float filamentUsed; };
class PrintJobTimer { public: static uint32_t duration() { return 0; } static printStatistics getStats(); };
extern PrintJobTimer print_job_timer;
