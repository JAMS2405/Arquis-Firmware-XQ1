// STUB: powerloss.h real: extern PrintJobRecovery recovery; info.print_job_elapsed es millis_t
#pragma once
#include <stdint.h>
struct job_recovery_info_stub { uint32_t print_job_elapsed; }; struct PrintJobRecovery { static job_recovery_info_stub info; }; extern PrintJobRecovery recovery;
