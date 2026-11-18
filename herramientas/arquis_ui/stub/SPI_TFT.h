// STUB: firmas reales de SPI_TFT.h / tft_io.h (WriteSequence(uint16_t*, uint16_t); setWindow(x, y, ancho, alto))
#pragma once
#include <stdint.h>
struct TFT_IO_stub { static void WriteSequence(uint16_t *Data, uint16_t Count); };
class TFT { public: TFT_IO_stub tftio; void setWindow(uint16_t x, uint16_t y, uint16_t with, uint16_t height); };
extern TFT SPI_TFT;
