# Instalar el firmware único ARQUIS (XQ1 y Aquila D1)

**Versión de PRUEBAS**, rama `feature/arquis-firmware-unico-perfiles`. Todavía no está validada en la impresora y no es una versión oficial.

Es **el mismo archivo para las dos impresoras**: `firmware_pruebas/fase4/Robin_nano_v3.bin`. Su SHA-256 está en `firmware_pruebas/fase4/SHA256SUMS.txt`.

## 0. Antes de instalar (recomendado)
1. Conecta la impresora al PC por USB (puerto COM) y envía `M503`. Guarda la respuesta en un archivo de texto: son tus pasos, PID, *offsets* y aceleraciones.
2. Ten a mano el BIN que usas ahora, por si quieres volver atrás.

Esta versión **no cambia la estructura de la EEPROM de Marlin**: la malla, el PID, el ajuste Z y los pasos se conservan. El paso 0 es solo una precaución.

## 1. Instalar (igual en las dos impresoras)
1. Con la impresora **apagada**, saca la microSD y ponla en el PC con un lector de tarjetas. Por el cable USB ya no aparece como disco.
2. Copia `Robin_nano_v3.bin` en la **raíz** de la microSD.
3. Mete la microSD y enciende. La placa se actualiza en unos segundos y renombra el archivo a `Robin_nano_v3.CUR`.

## 2. Primer arranque: el asistente
Si la impresora no tiene todavía un perfil ARQUIS, en lugar de la pantalla de inicio aparece el **asistente**, con 8 pasos. **Mientras no termine, la impresora no se mueve ni imprime**, tampoco por USB ni por Wi-Fi.

| Paso | Qué haces |
|---|---|
| 1 · Modelo | Elige **Arquis XQ1** o **Aquila D1**. Si la EEPROM ya tiene los pasos de una de ellas, aparece como «Detectada». En «Ajustes guardados», **Conservar** mantiene tus pasos, PID, malla y ajuste Z; **De fábrica** carga los del modelo |
| 2 · Dimensiones | Revisa ancho, fondo y altura. El máximo permitido es el de fábrica + 10 mm |
| 3 · Finales | Aleja los ejes de los finales con la mano. Pulsa y suelta cada final: debe pasar de **Abierto** a **Activado** y volver a **Abierto**. Si sin pulsar pone «Activado», cambia su lógica (Normal / Invertida) |
| 4 · Motores | Para cada motor pulsa **Probar**: se mueve 10 mm despacio (Z, 5 mm). Responde si se **alejó de su final**; si respondes «No», se invierte el sentido y repites la prueba. El extrusor es opcional: calienta a 200 °C y extruye 10 mm (con filamento cargado) |
| 5 · Home | Prueba el home de X, de Y y de Z, en ese orden. **Ten la mano en el interruptor**: si un eje no se para en su final, apaga |
| 6 · Mecánica | Pasos/mm, velocidades, aceleraciones, corrientes e Input Shaping. Los valores fuera de rango se rechazan. En el XQ1, Z (5 mm/s y 100 mm/s²) aparece con `*`: es provisional, pendiente de validar |
| 7 · Revisión | Lista con todos los valores |
| 8 · Guardar | Guarda en la EEPROM y vuelve a la pantalla de inicio. No hace falta reiniciar |

**Si se corta la luz durante el asistente, no se pierde nada:** el asistente vuelve a aparecer y la configuración anterior sigue intacta.

## 3. Después
- **El asistente no vuelve a aparecer**, ni al apagar y encender ni al instalar otra versión, mientras el perfil de la EEPROM sea válido.
- **Ajustes → Máquina** sirve para:
  - ver el modelo;
  - cambiar dimensiones, finales, motores, home y *offsets*, pasos, velocidades, corrientes e Input Shaping;
  - **cambiar de modelo**;
  - **restaurar la mecánica de fábrica** (conserva PID, malla y ajuste Z);
  - **restaurar todo** (avisa de lo que se pierde);
  - repetir el asistente.

  Las demás pantallas de parámetros de MKS siguen en «Otros parámetros (MKS)».
- **Durante una impresión, Ajustes → Máquina no se puede usar.**
- **Si cambias la lógica de un final o el sentido de un motor**, hay que repetir su prueba en el mismo menú. Hasta entonces la impresora no se mueve.

## 4. Recuperación
| Situación | Qué hacer |
|---|---|
| Quiero repetir la configuración | Ajustes → Máquina → Repetir el asistente |
| La pantalla no deja hacerlo | Por USB o Wi-Fi envía `M9050 R`, o imprime desde la microSD un archivo `.gcode` que contenga solo `M9050 R`. El asistente vuelve a aparecer (conserva el modelo) |
| Consultar el estado | `M9050` responde con el modelo, las comprobaciones, el estado de los drivers TMC y si el movimiento está bloqueado |
| Aviso «Un driver TMC perdió su configuración» | Comprueba la alimentación de los motores. El firmware restaura los drivers solo si no se está imprimiendo; después, haz home |
| Volver al firmware anterior | Copia el BIN anterior a la microSD y enciende. Los ajustes de Marlin se conservan. El registro ARQUIS se queda en la EEPROM sin molestar |

## 5. Compilar tú mismo
```bash
git fetch origin feature/arquis-firmware-unico-perfiles
git switch feature/arquis-firmware-unico-perfiles
pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc
```
Es un solo entorno para las dos impresoras. Si en tu copia hay cambios sin guardar de las fases anteriores (por ejemplo, los parches F3 aplicados a mano), **guárdalos antes de cambiar de rama**: `git stash push -m "antes de fase 4"`. Los parches F3-01 a F3-08 ya están en la rama como commits.
