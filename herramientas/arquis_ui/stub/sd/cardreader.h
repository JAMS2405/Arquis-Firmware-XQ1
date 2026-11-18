#pragma once
class CardReader { public: static bool isFileOpen() { return false; } static char* longest_filename() { return nullptr; } static uint8_t percentDone() { return 0; } };
extern CardReader card;
