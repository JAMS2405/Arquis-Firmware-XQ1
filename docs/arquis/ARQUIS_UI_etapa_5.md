# ARQUIS UI — Etapa 5 (parte A): herramientas, calibración y sistema

Rama `claude/nice-gauss-y90u12`. `main` intacta. **Estado: simulador y comprobación de firmas OK. NO compilada con Marlin completo y NO probada en la impresora.**

## 1. Qué incluye
| Pantalla | Qué ofrece | Objetos | Heap (sim.) |
|---|---|---|---|
| **Herramientas** | Temperatura, Extruir, Mover, Home, Nivel. auto (con confirmación), Nivelar cama, Filamento*, Más* | 10 | 3,5 KB |
| **Home** | Home todo / X / Y / Z, Motores off, Motores XY off | 8 | 3,5 KB |
| **Ajustes** | Parámetros*, Memoria (EEPROM)*, Ventiladores, Sistema, Idioma* y Wi-Fi* (si están activos), Apagar (con confirmación) | 6–9 | 3,5 KB |
| **Sistema y diagnóstico** | Máquina, firmware, placa, compilación, tiempo encendida, nº de impresiones y tiempo total (PRINTCOUNTER) + sensores de hotend y cama, finales de carrera X/Y/Z en vivo, tarjeta SD, estado de Marlin | 4 | 1,6 KB |
| **Babystep** (calibración Z) | Mismos mandos que Mover con pasos 0,01 / 0,05 / 0,1 mm y el **offset Z** visible; se guarda al volver (como la original) | 12 | 5,1 KB |
| **Velocidad** | Movimiento / Extrusión en %, − / + con paso 1, 5 o 10 | 11 | 3,9 KB |
| **Nivelar cama** | Esquema de la cama con los 5 puntos en su posición real (`trammingPos`), instrucciones | 9 | 3,5 KB |
\* Estas entradas abren aún la pantalla MKS original (ver §5).

400 + 400 ciclos de crear/destruir las cuatro pantallas nuevas: sin fugas. Peor caso con diálogo abierto: >9 KB libres de 16 KB.

## 2. Seguridad
- Las llamadas a Marlin son las de las pantallas originales (`draw_tool/home/set/baby_stepping/change_speed/manuaLevel/about.cpp`), copiadas literalmente: `G28…`, `M84`, `M290`, `suicide()`, `feedrate_percentage`, `planner.flow_percentage`, `settings.save()` al salir de Babystep, el comando de nivelación guardado en la flash SPI, etc.
- **Confirmaciones nuevas** (la original no las pedía): **Nivelación automática** (recorre la cama), **Apagar** la impresora / motores (`suicide()` corta la alimentación) y, desde la Etapa 4, Cambio de filamento imprimiendo.
- Home, Motores off y los puntos de Nivelar cama siguen sin confirmación (como la original): Home mueve los ejes hacia los finales de carrera y cada punto sube a Z10, se desplaza y baja a **Z0**. Despeja la cama antes.
- Con una impresión en marcha, Herramientas y Ajustes siguen bloqueados desde el Dashboard.
- Con `ARQUIS_UI` desactivado compila el código original (los diffs solo añaden líneas).
- Diagnóstico: los finales de carrera se leen con `READ(pin) != *_ENDSTOP_INVERTING`, la misma regla de M119; los sensores se marcan "Revisar" si leen ≤ 0 °C o cerca del máximo configurado. Son comprobaciones de lectura, no tocan nada.

## 3. Hallazgo durante la integración
La comprobación con cabeceras de Marlin detectó que mi variable `settings` chocaba con el objeto global `settings` (EEPROM) de Marlin: habría dado un error en la compilación real. Renombrada. (Los demás nombres estáticos se compararon con las cabeceras de Marlin.)

## 4. Qué se comprobó y qué NO
Comprobado: dibujo y toques en el simulador con LVGL real; memoria y fugas; firmas de las llamadas de Marlin contra cabeceras de prueba (`check_marlin_api.sh`, 13 archivos).
**NO comprobado:** compilación completa de Marlin; pantalla real; los finales de carrera y sensores del diagnóstico con tu placa; los pasos de babystep con tu sonda (`probe.offset.z` solo se muestra si hay sonda); el ancho de los textos en la pantalla física.

## 5. Pendiente de la Etapa 5 (parte B)
Siguen con el aspecto MKS: **Cambio de filamento**, **Parámetros de máquina** (pasos, aceleración, jerk, velocidades, TMC, sensibilidad de home…), **Memoria (EEPROM)**, **Idioma**, **Más** (G-code personalizado), teclado y teclado numérico, calibración táctil, Wi-Fi (no activo en tu firmware), y los diálogos de pausa/error de Marlin.

## 6. Prueba segura
1. Dashboard → Herram.: comprueba cada acceso y que **Atrás** vuelve al Dashboard.
2. **Sistema:** deben verse tus datos reales; mueve un final de carrera con la mano y comprueba que cambia (y que Z "Activado" = el sensor activo).
3. **Velocidad / Babystep:** solo desde una impresión en marcha (Opciones) o con el hotend frío; el offset Z debe coincidir con `M851`.
4. **Nivelar cama (con la cama despejada):** el primer punto hace Home; después cada punto sube, se desplaza y baja a Z0.
5. **Ajustes → Apagar:** debe pedir confirmación; pulsa Volver.
