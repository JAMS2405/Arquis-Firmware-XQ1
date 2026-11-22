# ARQUIS UI 1.0.0 — Guía de usuario

Recorrido por la pantalla táctil de la **Arquis XQ1**. (Las capturas son de la maqueta de pruebas con LVGL real; en la pantalla física los colores pueden verse ligeramente distintos según la iluminación.)

## Código de colores
| Color | Significado |
|---|---|
| **Azul** | Navegación, selección y acciones normales |
| **Verde** | Correcto, objetivo alcanzado, activo |
| **Ámbar** | Calentando, advertencia, pendiente |
| **Rojo** | Error, cancelar, acción peligrosa |
| **Gris** | Apagado, deshabilitado, secundario |

## 1. Dashboard
![Dashboard](ui/dash_1_lista_x2.png)
- **Arriba a la derecha** el estado de la máquina: *Reposo* (sin tarjeta SD y todo apagado, punto gris), *Lista* (con tarjeta y todo apagado), *Calentando* (ámbar), *Temp. alcanzada* (verde), *Enfriando* (azul, calentador apagado y por encima de 50 °C), *Imprimiendo*, *Pausada* y *Error* (rojo).
- **Hotend y Cama:** temperatura actual en grande y objetivo o "Apagado" debajo. Tocar una tarjeta abre *Temp.*; tocar *Ventiladores* abre su pantalla.
- **Accesos:** *Imprimir*, *Mover*, *Temp.*, *Herram.* y *Ajustes*.

![Dashboard calentando](ui/dash_2_calentando_x2.png)

## 2. Imprimir un archivo
![Explorador](ui/fil_1_lista_x2.png)
1. *Imprimir* abre el **explorador** de la tarjeta SD: las carpetas llevan una flecha; las flechas de la derecha cambian de página; *Atrás* sube un nivel.
2. Al tocar un archivo se ve su **miniatura** (si el G-code la trae) para confirmar qué vas a imprimir.

![Confirmar impresión](ui/fil_5_confirmar_x2.png)

3. *Imprimir* inicia el trabajo; *Cancelar* vuelve a la lista sin hacer nada.

## 3. Durante la impresión
![Supervisión](ui/print_1_imprimiendo_x2.png)
- Muestra el **progreso**, el tiempo transcurrido y restante (solo cuando hay datos suficientes para estimarlo), la altura Z, las temperaturas, los ventiladores y la **velocidad real del cabezal** (*Veloc.*: la actual en mm/s y la máxima alcanzada en esa impresión). Esa velocidad se mide con la posición real de X e Y (pasos ya ejecutados), no con la que pide el G-code; se promedia en ventanas de 0,15 s, así que un tramo muy corto puede no llegar a verse. La máxima se pone a cero al empezar cada impresión.
- **Pausar / Reanudar:** el botón refleja el estado real. **Inicio** vuelve al Dashboard sin detener nada; desde allí, la tarjeta de avance regresa a la supervisión. Durante una impresión *Herram.* se oculta y *Mover* y *Ajustes* están deshabilitados (para no estropear la pieza).
- **Cancelar** siempre pide confirmación:

![Cancelar impresión](ui/dialogo_cancelar_x2.png)

- **Objetos:** si el G-code define objetos, el mapa de la cama permite **cancelar uno solo** (también con confirmación) sin parar el resto.

![Cancelar objeto](ui/obj_1_normal_x2.png)

- **Opciones:** Temperatura, Ventiladores, Velocidad, Babystep y Filamento mientras imprime.

![Opciones](ui/ctl_9_opciones_imprimiendo_x2.png)

## 4. Control de la máquina
| Pantalla | Qué hace |
|---|---|
| ![Temp.](ui/ctl_1_temp_hotend_x2.png) **Temp.** | Cambia el objetivo del hotend o de la cama con pasos de 1, 5 y 10 °C y preajustes |
| ![Ventiladores](ui/ctl_3_ventiladores_x2.png) **Ventiladores** | Ajusta el porcentaje de cada ventilador |
| ![Mover](ui/ctl_5_mover_x2.png) **Mover** | Mueve X, Y y Z con pasos de 0,1, 1 y 10 mm; avisa si falta hacer Home |
| ![Extruir](ui/ctl_7_extruir_x2.png) **Extruir** | Extruye o retrae; con el hotend frío lo bloquea y lo explica |

**Herramientas** reúne Temperatura, Extruir, Mover, Home, Nivelación (automática si existe y manual de 5 puntos), Filamento y Más:

![Herramientas](ui/st5_5_herramientas_x2.png)

- **Home** (todo, X, Y, Z) y *Motores off*:

![Home](ui/st5_7_home_x2.png)

- **Nivelar cama:** el esquema muestra los 5 puntos; cada uno sube, se desplaza y baja. **Despeja la cama antes.**

![Nivelar cama](ui/st5_2_nivelar_x2.png)

- **Velocidad** (movimiento y extrusión en %) y **Babystep** (ajuste fino de Z durante la impresión):

![Velocidad](ui/st5_1_velocidad_x2.png) ![Babystep](ui/st5_4_babystep_x2.png)

### Cambio de filamento
![Cambio de filamento](ui/st5b_11_filamento_x2.png)
*Cargar* o *Descargar* calientan el hotend a la temperatura de filamento, avisan cuando llega y piden confirmación antes de mover el filamento. *Cancelar* detiene el proceso.

## 5. Ajustes
![Ajustes](ui/st5_6_ajustes_x2.png)
- **Parámetros de máquina:** pasos por mm, aceleración, jerk, velocidades máximas, corriente TMC, modo silencioso, sensibilidad de home, nivelación… Tocar una fila abre el teclado numérico.

![Parámetros](ui/st5b_1_parametros_x2.png) ![Teclado numérico](ui/st5b_5_teclado_x2.png)

El teclado muestra el nombre del parámetro; **Cancelar** sale sin cambiar nada y **Aceptar** se deshabilita si el valor no es un número.

- **Memoria:** guardar, leer o restaurar los ajustes. *Restaurar valores de fábrica* lleva el botón en rojo y pide confirmación.

![Confirmación peligrosa](ui/st5b_8_dialogo_peligro_x2.png)

- **Sistema:** datos de la máquina (versión de Marlin e **Interfaz ARQUIS 1.0.0**, placa, horas encendida, impresiones y tiempo total) y **diagnóstico en vivo**: sensores de hotend y cama y finales de carrera X, Y, Z. Una lectura anómala sale en rojo con "Revisar".

![Sistema](ui/st5_3_sistema_x2.png)

- **Wi-Fi:** estado (IP, red) y lista de redes disponibles; la red conectada se ve en verde.

![Wi-Fi](ui/st5b_16_wifi_estado_x2.png) ![Redes](ui/st5b_17_wifi_redes_x2.png)

- **Idioma:** español o inglés. **Más → G-code:** escribe un comando y muestra el resultado.

![G-code](ui/st5b_12_gcode_x2.png)

## 6. Consejos de seguridad
- Hasta que no te sientas cómodo, haz *Home*, *Nivelar cama* y *Mover* con la **cama despejada**.
- Si un sensor aparece en **Revisar** en *Ajustes → Sistema*, revisa el termistor y su cable **antes de calentar**.
- Las protecciones térmicas de Marlin siguen activas y no las cambia la interfaz.
- Ante cualquier comportamiento inesperado, apaga la impresora y consulta `docs/arquis/ENTREGA_v1.0.0.md` (limitaciones conocidas).
