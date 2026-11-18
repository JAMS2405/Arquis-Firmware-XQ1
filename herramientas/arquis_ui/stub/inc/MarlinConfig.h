#pragma once
#include "MarlinConfigPre.h"
constexpr uint8_t ui8_to_percent(const uint8_t i) { return (int(i) * 100 + 127) / 255; }
