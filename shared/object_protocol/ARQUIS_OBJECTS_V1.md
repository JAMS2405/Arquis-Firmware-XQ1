# ARQUIS_OBJECTS_V1 — manifiesto de objetos (BORRADOR, se implementa en la Etapa 2)

Versión de la enmienda: capacidad **64** (decisión del propietario). La etapa 0 (`docs/slicer_suite/00_etapa0_auditoria.md`, §5) contiene el razonamiento; este archivo es la referencia normativa y se mantiene aquí.

**Transporte:** comentarios ASCII del propio G-code (autosuficiente). **Unidades:** enteros en 0,1 mm, en el sistema de coordenadas del G-code. **Ubicación (pendiente de confirmar):** bloque al final del archivo + marcador en la cabecera (dentro de los 512 B iniciales, para no desplazar `;simage:`).

```
;ARQUIS_OBJECTS=1 PID=3FA9C21B N=50                       cabecera, ≤ 40 B
;ARQUIS_OBJECTS_BEGIN
;ARQ:H V=1 PID=3FA9C21B N=50 BED=3000,3000 MIN=0,0 ORG=FL U=0.1mm CAP=64
;ARQ:M <idx> <nombre ≤ 20 B UTF-8>                       tabla de nombres
;ARQ:O <id> <idx_nombre> <instancia> <cx> <cy> <ancho> <prof> <rot> <tipo C|S|R> <ztop>
;ARQUIS_OBJECTS_END CRC=1A2B3C4D                         CRC-32 de las líneas entre BEGIN y END
;ARQUIS_TRAILER OFF=0001234567 LEN=00003210 CRC=1A2B3C4D PID=3FA9C21B   última línea (campos de ancho fijo)
```
- `id` = índice de `M486 S<id>` (0…N−1, contiguos). Objetos con `id ≥ CAP` no reciben `M486 S`: se publican como "No disponible".
- Tamaños medidos con el generador de referencia (`02_capacidad_64.md`): 64 objetos con 64 nombres de 20 B = **5 150 B** (peor caso); 40 piezas con 3 nombres = **2 066 B**; línea más larga 72 B.
- Validaciones obligatorias (plugin y firmware): versión desconocida, `N` ≠ filas, IDs duplicados o fuera de rango, coordenadas fuera de cama, CRC/PID, trailer sin bloque. Cualquier fallo ⇒ sin cancelación visual; la impresión normal no se ve afectada.
- Verificación en ejecución: `N` = valor de `M486 T`; la caja XY aprendida de cada objeto (`note_extrusion`) debe caer dentro de la del manifiesto (+ margen). Si no, `UNTRUSTED`.
