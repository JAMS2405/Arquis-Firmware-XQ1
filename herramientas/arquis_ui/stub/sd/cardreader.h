// STUB: firmas reales de cardreader.h (Marlin 2.1.2.10)
#pragma once
struct card_flags_stub { bool sdprintdone; };
class CardReader { public:
  static card_flags_stub flag;
  static bool isFileOpen(); static char* longest_filename(); static uint8_t percentDone(); static uint32_t getIndex(); static uint32_t getFileSize();
  static bool isMounted(); static void openFileRead(const char * const path, const uint8_t subcall=0); static void closefile(const bool store_location=false); static int16_t read(void *buf, uint16_t nbyte); static void setIndex(const uint32_t index); static void cdup(); static void pauseSDPrint(); static bool isPrinting(); static void abortFilePrintSoon();
}; extern CardReader card;
