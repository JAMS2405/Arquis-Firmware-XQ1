#pragma once
/**
 * ARQUIS - selector de perfil de maquina.
 *
 * El firmware (pantallas, nivelacion 5x5, PID...) es el MISMO para todas las maquinas. Lo que cambia de una impresora a otra
 * (medidas, pasos/mm, sentidos de motor, finales de carrera, PID, input shaping...) vive en un perfil por maquina:
 *   Marlin/machine/xq1.h         Arquis XQ1 (por defecto: sin overrides, valen los de Configuration.h)
 *   Marlin/machine/aquila_d1.h   Aquila D1
 * El perfil se elige con el entorno de PlatformIO (ini/arquis.ini): `pio run -e aquila_d1`. Sin entorno especifico se usa el XQ1.
 *
 * Como funciona: Configuration.h y Configuration_adv.h incluyen este archivo ANTES de sus valores; cada valor "de maquina" esta
 * protegido con #ifndef, asi que lo que defina el perfil gana. Ver docs/arquis/MAQUINAS.md.
 */
#if defined(ARQUIS_MACHINE_AQUILA_D1)
  #include "aquila_d1.h"
#else
  #include "xq1.h"
#endif
