# ARQUIS Slicer Suite — documentación

| Documento | Contenido |
|---|---|
| [`00_etapa0_auditoria.md`](00_etapa0_auditoria.md) | Auditoría de Cura 5.11 y del firmware, arquitectura, protocolo, riesgos, plan de pruebas (con la Enmienda 1) |
| [`01_arquitectura_repositorio.md`](01_arquitectura_repositorio.md) | Organización de este repositorio (firmware / `cura/` / `shared/` / docs / tests) y cómo se mantienen separados |
| [`02_capacidad_30.md`](02_capacidad_30.md) | Estudio de capacidad: 30 objetos (IDs 0–29), motor M486 real, tipos, RAM, manifiesto, LVGL, con mediciones |
| [`04_cambio_capacidad_30.md`](04_cambio_capacidad_30.md) | Revisión de la Etapa 1, cambios por el nuevo límite y hallazgos con G-code real de Cura |
| [`03_etapa1_m486.md`](03_etapa1_m486.md) | Etapa 1: motor M486 en Cura — reglas, pruebas, limitaciones, cómo validarlo con Cura real |
| [`05_etapa2_manifiesto.md`](05_etapa2_manifiesto.md) | Etapa 2: gestor de objetos, mapa de cama y manifiesto — qué hay, pruebas, limitaciones, validación |
| [`07_etapa3_firmware.md`](07_etapa3_firmware.md) | Etapa 3: firmware y TS35 (rechazo M486 fuera de 0-29, lector de manifiesto, mapa y lista de 30 objetos, puerta de cancelación), pruebas por tipo, memoria antes/después, riesgos e instrucciones para tu impresora |
| [`08_estado_proyecto.md`](08_estado_proyecto.md) | **Estado actual** frente al PROMPT MAESTRO: funciones comprobadas en la impresora, estado por etapa (implementado / simulación / físico), pendientes para cerrar la Etapa 3, mejoras futuras, próxima etapa autorizada y tabla de avance |
| [`../../cura/VALIDACION_EN_CURA.md`](../../cura/VALIDACION_EN_CURA.md) | Cómo validar el plugin dentro de Cura 5.11 real (hoy: NO validado) |
| [`06_decisiones.md`](06_decisiones.md) | Decisiones confirmadas del propietario (manifiesto, licencia, capacidad, repositorio, orientación provisional, política de cancelación visual) |
| [`../../shared/object_protocol/ARQUIS_OBJECTS_V1.md`](../../shared/object_protocol/ARQUIS_OBJECTS_V1.md) | Especificación normativa del manifiesto y del mapa |
