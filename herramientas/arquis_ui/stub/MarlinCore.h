#pragma once
enum MarlinState : uint8_t { MF_INITIALIZING = 0, MF_STOPPED, MF_KILLED, MF_RUNNING, MF_SD_COMPLETE, MF_PAUSED, MF_WAITING };
extern MarlinState marlin_state;
inline bool IsStopped() { return marlin_state == MF_STOPPED; }
bool printingIsActive();
bool printingIsPaused();
