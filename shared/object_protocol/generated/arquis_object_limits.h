// SPDX-License-Identifier: LGPL-3.0-or-later
// ARCHIVO GENERADO por shared/object_protocol/gen_limits.py a partir de shared/object_protocol/limits.json. NO EDITAR A MANO.
#pragma once
// ARQUIS_OBJECTS_V1: limites y constantes compartidos entre Cura y el firmware.
#define ARQ_OBJ_PROTOCOL_VERSION   1
#define ARQ_OBJ_DESIGN_CAPACITY    30   // objetos simultaneos de diseno = IDs cancelables 0..ARQ_OBJ_MAX_ID
#define ARQ_OBJ_MAX_ID             (ARQ_OBJ_DESIGN_CAPACITY - 1)
#define ARQ_OBJ_CAPACITY_CEILING   32   // maximo que admite el motor M486 actual (mascara uint32_t)
#define ARQ_OBJ_PROTOCOL_MAX       128  // limite absoluto (int8_t)
#define ARQ_OBJ_NAME_MAX           20   // bytes UTF-8 de un nombre en el manifiesto
#define ARQ_OBJ_NAME_BUF           (ARQ_OBJ_NAME_MAX + 1)
#define ARQ_OBJ_COORD_UNIT_DMM     1    // 1 unidad = 0,1 mm

#ifdef __cplusplus
static_assert(ARQ_OBJ_DESIGN_CAPACITY >= 1 && ARQ_OBJ_DESIGN_CAPACITY <= ARQ_OBJ_CAPACITY_CEILING, "capacidad de diseno fuera de rango");
static_assert(ARQ_OBJ_CAPACITY_CEILING <= 32, "la mascara canceled es uint32_t: subir el techo exige cambiar el motor de cancelacion");
static_assert(ARQ_OBJ_CAPACITY_CEILING <= ARQ_OBJ_PROTOCOL_MAX, "el techo no puede superar el limite del protocolo");
#endif
