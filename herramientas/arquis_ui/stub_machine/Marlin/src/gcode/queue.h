#pragma once
#include "../inc/MarlinConfigPre.h"
class GCodeQueue { public: static void inject(FSTR_P const gcode); static bool has_commands_queued(); };
extern GCodeQueue queue;
