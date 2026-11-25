# Pruebas físicas del firmware único ARQUIS

**BIN:** `firmware_pruebas/fase4/Robin_nano_v3.bin` (SHA-256 en `SHA256SUMS.txt`). **Nada de esta lista se ha probado todavía en una impresora:** solo hay compilación, pruebas de software y simulador.

**Estado de cada prueba:** `Pendiente` · `Aprobada` · `Fallida` · `No aplicable`. Anota la fecha y lo que veas en «Notas».

**Seguridad:**
- Haz las pruebas de movimiento con la mano en el interruptor.
- En las pruebas 7 y 8, empieza con los ejes a mitad de recorrido.
- Si algo se mueve hacia donde no debe, **apaga** y anota qué pasó.

## Arquis XQ1
| # | Prueba | Cómo | Resultado esperado | Estado | Notas |
|---|---|---|---|---|---|
| 1 | Instalar el BIN | Copiar a la raíz de la microSD y encender | Se actualiza; el archivo pasa a `.CUR` | Pendiente | |
| 2 | Asistente | Encender tras instalar | Aparece el asistente, no la pantalla de inicio. Sin terminarlo, `G28` por USB responde «ARQUIS: movimiento bloqueado» | Pendiente | |
| 3 | Elegir XQ1 | Paso 1 | Se puede elegir; si la EEPROM tenía los pasos del XQ1, sale «Detectada» | Pendiente | |
| 4 | Dimensiones y perfil | Pasos 2 y 7 | 300 × 300 × 450; pasos Z/E 791,74 / 748,72 (o los tuyos si elegiste Conservar) | Pendiente | |
| 5 | Persistencia | Guardar, apagar, encender (3 veces) | Va directo a Inicio; Sistema muestra «Arquis XQ1»; `M115` dice `MACHINE_TYPE:Arquis XQ1` | Pendiente | |
| 6 | Finales | Paso 3 del asistente y `M119` con cada final pulsado y suelto | En reposo «Abierto» / `open`; pulsado «Activado» / `TRIGGERED` | Pendiente | |
| 7 | Direcciones | Paso 4: Probar X, Y, Z, E | Cada eje se aleja de su final 10 mm (Z 5 mm) despacio; E extruye | Pendiente | |
| 8 | Home de cada eje | Paso 5 y luego `G28` | Cada eje va a su final y se detiene; sin golpes | Pendiente | |
| 9 | Límites X/Y/Z | Tras home, `G1 X300 Y300 F3000`, luego `G1 X310` y `G1 Z450 F600` | No pasa de 300 / 300 / 450 (límites por software) | Pendiente | |
| 10 | Malla 5×5 | Herramientas → Nivelación | Puntos dentro de 15–285 × 15–285; la malla guardada se carga | Pendiente | |
| 11 | PID y temperaturas | Precalentar; Ajustes → PID | Temperaturas estables; autoajuste igual que antes | Pendiente | |
| 12 | Wi-Fi | Ajustes → Wi-Fi; enviar un archivo e imprimirlo | Conecta e imprime | Pendiente | |
| 13 | microSD | Imprimir una pieza pequeña | Imprime de principio a fin | Pendiente | |
| 14 | USB CDC / COM | Conectar al PC: `M115`, `M119`, `M9050`, `M122` | Aparece el puerto COM; responden; `M122` muestra los 4 drivers con valores | Pendiente | |
| 15 | Input Shaping | Ajustes → Rendimiento; `M593` | 35 / 25 Hz; se pueden cambiar y guardar | Pendiente | |
| 16 | Velocidad real | Durante una impresión | La fila «Veloc.» muestra la velocidad real y la máxima | Pendiente | |
| 17 | M221 | Herramientas → Flujo (o `M221 S95`) | Cambia el caudal | Pendiente | |
| 18 | Ajuste Z del XQ1 (provisional) | Imprimir con Z a 5 mm/s | Z sube bien al imprimir; si no, anotarlo (valor pendiente desde antes) | Pendiente | |

## Aquila D1 (repetir con su impresora)
| # | Prueba | Resultado esperado | Estado | Notas |
|---|---|---|---|---|
| 19 | Pruebas 1 a 17 eligiendo **Aquila D1** | 230 × 240 × 240; pasos Z/E 803,41 / 781,08; X/Y con lógica de finales normal; E0 con sentido invertido | Pendiente | |
| 20 | **Lectura de los TMC (imprescindible en el Aquila)** | `M122` muestra datos de los 4 drivers y `M9050` dice `tmc=1` (correcto). Sin lectura, el Aquila no se mueve: el firmware no puede confirmar el bit `shaft` | Pendiente | |
| 21 | Límites | Tras home: `G1 X240` se queda en 230 y `G1 Y250` en 240; la malla queda en 15–215 × 15–225 | Pendiente | |
| 22 | Aparcado / cambio de filamento | Pausa con aparcado o `M600` | Aparca dentro de la cama (Y ≤ 230) y no contra el bastidor | Pendiente | |

## Cambio de perfil y reinicios
| # | Prueba | Resultado esperado | Estado | Notas |
|---|---|---|---|---|
| 23 | Cambiar de modelo y volver (solo en un banco de pruebas, **sin imprimir**) | Ajustes → Máquina → Modelo: tras cambiar, las medidas, pasos, finales y sentidos son los del modelo nuevo y no queda nada del anterior (revisión del asistente y `M503`). La malla del modelo anterior aparece como «Sin malla» | Pendiente | |
| 24 | Restaurar la mecánica de fábrica | Vuelven pasos, velocidades y corrientes de fábrica; se conservan PID, malla y ajuste Z | Pendiente | |
| 25 | Cambiar la lógica de un final en el menú sin probarlo | `G28` queda bloqueado hasta repetir la prueba de finales | Pendiente | |
| 26 | Reinicios | Apagar y encender 5 veces | El asistente no reaparece | Pendiente | |
| 27 | Actualizar el BIN | Instalar otra vez el mismo BIN | El asistente no reaparece; se conservan perfil y ajustes | Pendiente | |
| 28 | Recuperación | `M9050 R` por USB, luego volver a Inicio | Aparece el asistente con el modelo preseleccionado | Pendiente | |
