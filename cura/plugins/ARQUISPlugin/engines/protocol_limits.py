"""Constantes del protocolo ARQUIS_OBJECTS_V1.

Copia de shared/object_protocol/limits.json (el plugin se instala sin el resto del repositorio, por eso no lee ese archivo en
tiempo de ejecucion). ``cura/tests/test_protocol_limits.py`` comprueba que ambas fuentes son identicas.
"""

PROTOCOL_NAME = "ARQUIS_OBJECTS"
PROTOCOL_VERSION = 1

# Capacidad objetivo de cancelacion del firmware ARQUIS (indices M486 S0..S63). Decision del propietario: 64.
TARGET_CAPACITY = 64
# Capacidad del motor M486 de Marlin 2.1.2.10 sin modificar (mascara uint32_t).
LEGACY_FIRMWARE_CAPACITY = 32
# Mayor indice que cabe en los contadores int8_t de Marlin (object_count/active_object). Tope absoluto del protocolo.
MAX_PROTOCOL_INDEX = 127

NAME_MAX_BYTES = 20          # nombres de objeto en el manifiesto (UTF-8)
COORD_UNIT_MM = 0.1          # el manifiesto usa enteros en decimas de mm
