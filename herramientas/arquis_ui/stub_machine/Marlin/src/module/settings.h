#pragma once
class MarlinSettings { public: static bool save(); static bool load(); static bool validate(); };
extern MarlinSettings settings;
