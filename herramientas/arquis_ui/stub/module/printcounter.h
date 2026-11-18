#pragma once
struct duration_t { uint32_t value; };
class PrintJobTimer { public: static duration_t duration() { return { 0 }; } };
extern PrintJobTimer print_job_timer;
