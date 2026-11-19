# ARCHIVO GENERADO por shared/object_protocol/gen_limits.py a partir de shared/object_protocol/limits.json. NO EDITAR A MANO.
"""Constantes del protocolo ARQUIS_OBJECTS_V1 (la fuente es shared/object_protocol/limits.json)."""

PROTOCOL_NAME = 'ARQUIS_OBJECTS'
PROTOCOL_VERSION = 1

DESIGN_CAPACITY = 30                  # objetos simultaneos de diseno: IDs cancelables 0..DESIGN_CAPACITY-1
MAX_CANCELABLE_ID = DESIGN_CAPACITY - 1
CAPACITY_CEILING = 32                # mayor capacidad que admite el motor del firmware actual (mascara uint32_t)
PROTOCOL_MAX_OBJECTS = 128               # limite absoluto del protocolo (contadores int8_t de Marlin)

NAME_MAX_BYTES = 20                     # nombres de objeto en el manifiesto (UTF-8)
COORD_UNIT_MM = 0.1                    # el manifiesto usa enteros en decimas de mm
