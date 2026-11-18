#pragma once
enum MarlinState : uint8_t { MF_INITIALIZING = 0, MF_STOPPED, MF_KILLED, MF_RUNNING, MF_SD_COMPLETE, MF_PAUSED, MF_WAITING };
extern MarlinState marlin_state;
inline bool IsStopped() { return marlin_state == MF_STOPPED; }
bool printingIsActive();
bool printingIsPaused();

class __FlashStringHelper; typedef const __FlashStringHelper* FSTR_P;
#define F(str) (reinterpret_cast<const __FlashStringHelper *>(str))
extern bool wait_for_heatup;
void suicide();
uint32_t millis();
