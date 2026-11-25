# BIN de PRUEBAS: firmware único ARQUIS (Fase 4)

**No es una versión oficial.** Sirve para las pruebas físicas de `docs/arquis/PRUEBAS_FISICAS_ARQUIS.md`.

- `Robin_nano_v3.bin`: el mismo archivo para Arquis XQ1 y Aquila D1. Se compiló desde el commit `758fe50` de la rama `feature/arquis-firmware-unico-perfiles`, con `pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc`.
- `SHA256SUMS.txt`: suma de comprobación.
- `compilacion.log`: registro completo de la compilación. Las rutas de la máquina de compilación aparecen como `<scratchpad>`.

**Instalación:** ver `docs/arquis/INSTALACION_FIRMWARE_UNICO.md`.

**Nota:** si compilas tú mismo desde la rama, el BIN no saldrá idéntico byte a byte. Llevará otra fecha y hora (`M115`) y otras rutas de carpeta en los mensajes internos. El código es el mismo.
