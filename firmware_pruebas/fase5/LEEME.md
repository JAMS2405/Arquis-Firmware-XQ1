# BIN de PRUEBAS: firmware único ARQUIS con las correcciones P1–P5 (Fase 5)

**No es una versión oficial ni definitiva.** No está probado en ninguna impresora. Sirve para auditarlo y, si se aprueba, para las pruebas físicas de `docs/arquis/PRUEBAS_FISICAS_ARQUIS.md`.

- `Robin_nano_v3.bin`: un solo archivo para Arquis XQ1 y Aquila D1. Compilado desde el `git archive` del commit `0bae5fc` de la rama `claude/pensive-cray-ib96kb` (basada en `feature/arquis-firmware-unico-perfiles` @ `fdff0cd`), con `pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc`.
- `SHA256SUMS.txt`: suma de comprobación.
- `compilacion.log`: registro completo. Las rutas de la máquina de compilación aparecen como `<scratchpad>`.

**Correcciones y pruebas:** `docs/arquis/FASE5_CORRECCIONES_AUDITORIA.md`.

**Nota:** si compilas tú mismo, el BIN no saldrá idéntico byte a byte: cambian la fecha y hora (`M115`) y unas rutas de carpeta en los mensajes internos. El código es el mismo.
