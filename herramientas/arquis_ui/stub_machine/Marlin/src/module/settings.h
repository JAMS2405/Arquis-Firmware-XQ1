#pragma once
class MarlinSettings { public: static bool save(); static bool load(); static bool validate(); static void reset(); };
extern MarlinSettings settings;
