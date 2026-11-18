#pragma once
// Stopwatch::duration() real: static millis_t duration() -> segundos
class PrintJobTimer { public: static uint32_t duration() { return 0; } };
extern PrintJobTimer print_job_timer;
