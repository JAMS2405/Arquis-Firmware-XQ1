#pragma once
#include "../inc/MarlinConfigPre.h"
struct ArqFakeParser { const char *args = ""; bool seen(const char c) { return strchr(args, c) != nullptr; } };
extern ArqFakeParser parser;
