#pragma once
#include <stdint.h>
#include <stddef.h>
// EEPROM I2C simulada (4 KB) con la semantica de HAL/STM32/eeprom_wired.cpp: read_data/write_data devuelven true si hay error
class PersistentStore { public:
  static size_t capacity();
  static bool access_start(); static bool access_finish();
  static bool write_data(int &pos, const uint8_t *value, size_t size, uint16_t *crc);
  static bool read_data(int &pos, uint8_t *value, size_t size, uint16_t *crc, const bool writing = true);
};
extern PersistentStore persistentStore;
