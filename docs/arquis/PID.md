# ARQUIS XQ1 — Autoajuste PID de hotend y cama desde la TS35

Estado: **implementado, probado en PC (lógica, pantalla LVGL simulada, firmas contra Marlin) y compilado por el propietario.** **Probado en la impresora por el propietario (2026-11-22);** los resultados detallados de esas pruebas no se han trasladado a este repositorio.

## Uso
**Ajustes → PID.**
1. Elige **Hotend** o **Cama**, la temperatura objetivo (−/+ 5 °C) y los ciclos (−/+ 1). Por defecto: hotend 210 °C / 8 ciclos, cama 60 °C / 6 ciclos. Debajo se ve el PID actual.
2. **Iniciar autoajuste** → confirmar. Marlin ejecuta `M303 E0 S<t> C<n>` (hotend) o `M303 E-1 S<t> C<n>` (cama). **Se apagan todos los calentadores al empezar** (comportamiento de M303) y tarda varios minutos. La pantalla muestra temperatura, ciclo y tiempo.
3. **Cancelar** (o ◄ con confirmación) detiene el ajuste con el mecanismo de Marlin (`wait_for_heatup = false`, como `M108`), apaga los calentadores y **no cambia ningún valor**.
4. Al terminar se muestran **Kp, Ki, Kd nuevos y (anteriores)**. Nada se ha aplicado todavía:
   * **Aplicar y guardar** → `M301 P I D` (hotend) / `M304 P I D` (cama) y `M500` (+ comprobación de la EEPROM). Si la EEPROM falla, el PID queda aplicado pero se avisa de que NO está guardado.
   * **Descartar** → todo queda como estaba.
5. Si Marlin falla (temperatura demasiado alta, tiempo agotado, valores no plausibles) se muestra el motivo y no se ofrece nada que aplicar.

## Qué cambia en el firmware
* `Configuration.h`: **`PIDTEMPBED` activado** (el PID de cama no existía; la cama usaba bang-bang). Valores iniciales de Marlin (10 / 0,023 / 305,4) hasta que hagas el ajuste. Con PID de cama el calentador se modula con PWM suave (frecuencia ~7,7 Hz de Marlin): es el comportamiento estándar, pero si tu cama usa un relé/SSR que no lo admite, desactiva `PIDTEMPBED` y esta pantalla solo ofrecerá el hotend.
* **Parche mínimo en `module/temperature.cpp`** (8 líneas, todas bajo `TERN_(ARQUIS_UI, …)`; sin ARQUIS_UI el código de Marlin queda idéntico): anota ciclo y resultado en `arquis_pid_status.h` y llama a `arq_pid_idle()` en cada vuelta del bucle de M303. Motivo: ese bucle es bloqueante y **no atiende la pantalla**, y el resultado solo sale por el puerto serie; sin el parche la pantalla se congelaría y no se podría cancelar ni ver el resultado.
* Código nuevo: `arquis_pid_logic` (pura), `arquis_pid` (controlador), `arquis_pid_view` (LVGL), `arquis_pid_ui` (pantalla `PID_UI`, tarjeta en Ajustes), `arquis_pid_status.h`.
* **EEPROM:** activar `PIDTEMPBED` añade el PID de cama a los datos guardados → cambia su tamaño → **la primera vez tras flashear Marlin descarta la EEPROM y vuelve a los valores de fábrica** (igual que con la malla 5×5). Guarda `M503` antes de flashear, reaplica tus ajustes, haz `M500`, y después el autoajuste.

## Seguridad
No arranca con una impresión activa o pausada, con Marlin detenido, con la nivelación 5×5 en curso ni con comandos pendientes. Los límites de objetivo son los de Marlin (hotend `HEATER_0_MAXTEMP − HOTEND_OVERSHOOT`, cama `BED_MAX_TARGET`) más mínimos razonables (150 / 40 °C). La protección térmica de Marlin sigue activa durante el ajuste (`WATCH_PID`): si el calentador no sube, Marlin detiene la máquina. Resultados con Kp ≤ 0, Ki ≈ 0 (Marlin divide por Ki), Kd < 0 o fuera de rango se rechazan.

## Recomendaciones de uso
* Hotend: temperatura de uso habitual (p. ej. 210 °C para PLA), con el ventilador de capa **como lo usarás** (apagado si no quieres que influya) y la boquilla con filamento o al menos fría de corrientes de aire. 8 ciclos es un buen compromiso.
* Cama: a la temperatura de uso (60 °C PLA), cama limpia y sin corrientes. 5–6 ciclos bastan; tarda más que el hotend.
* Repite el ajuste si cambias el hotend, el sensor, el ventilador del hotend o la cama.

## Pruebas
`tests/pid/run.sh` (31 comprobaciones estáticas + 74 de lógica) y `herramientas/arquis_ui/sim/build_pid_sim.sh` (pantalla con LVGL real: toques, fases, 300 ciclos sin fugas). Capturas en `docs/arquis/pid/`. **Pendiente en la impresora:** ajuste real de hotend y de cama, cancelación a mitad, aplicar/guardar/reiniciar, comportamiento del tactil durante M303 y la estabilidad de temperatura con el PID de cama.
