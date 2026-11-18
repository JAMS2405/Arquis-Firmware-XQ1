# ARQUIS UI 1.0.0 — Lista de verificación de aceptación

Hazla **con la impresora fría, la cama despejada y sin filamento** salvo donde se indique. Marca cada punto; si algo falla, anota el paso y haz una foto de la pantalla.
Los puntos con ⚠ mueven la máquina o calientan: hazlos con cuidado y con la mano cerca del interruptor.

## A. Arranque
- [ ] Sale el **logo de ARQUIS** (no el de Makerbase) y luego el **Dashboard**.
- [ ] *Ajustes → Sistema* dice **Interfaz ARQUIS 1.0.0** y muestra tu máquina, la versión de Marlin y la placa.
- [ ] El tacto responde con precisión en las cuatro esquinas (si no, recalibrar el táctil).

## B. Dashboard y estados
- [ ] Con la **tarjeta SD puesta** y todo apagado: estado **Lista**.
- [ ] **Sin tarjeta SD** y todo apagado: estado **Reposo** (punto gris).
- [ ] Hotend y cama muestran temperatura; los ventiladores muestran su porcentaje.
- [ ] ⚠ Al calentar (Temp.): el estado pasa a **Calentando** y luego a **Temp. alcanzada**; al apagar, **Enfriando** (azul) hasta bajar de 50 °C.
- [ ] Ajustes → Sistema, sensores: hotend y cama **sin "Revisar"**. *(Pendiente conocido: la cama marca ≈ 45 °C con todo apagado.)*

## C. Imprimir archivo
- [ ] *Imprimir* abre el explorador: lista los archivos y carpetas de la SD; las flechas cambian de página; entrar y salir de una carpeta funciona.
- [ ] Al tocar un G-code **con miniatura** se ve la miniatura y los botones *Cancelar* / *Imprimir*. Con uno **sin miniatura**, un recuadro "Sin miniatura".
- [ ] *Cancelar* vuelve a la lista sin iniciar nada.

## D. Durante una impresión (usa una pieza corta)
- [ ] ⚠ *Imprimir* inicia la impresión; aparece la supervisión con progreso, tiempos, temperaturas y ventiladores.
- [ ] *Inicio* vuelve al Dashboard sin parar la impresión; desde el Dashboard, la tarjeta de avance vuelve a la supervisión. *Herram.* está oculto; *Mover* y *Ajustes* están deshabilitados.
- [ ] *Pausar* y *Reanudar* funcionan y el botón cambia de estado.
- [ ] *Opciones*: Temperatura, Ventiladores, Velocidad y Babystep abren y se puede volver.
- [ ] *Objetos* (si el G-code define objetos): se ve el mapa de la cama, se puede seleccionar un objeto y **pide confirmación** antes de cancelarlo. Si no define objetos, lo dice.
- [ ] ⚠ *Cancelar* **pide confirmación**; con *Seguir imprimiendo* no pasa nada; al confirmar se detiene la impresión y vuelve al Dashboard.
- [ ] Al terminar una impresión aparece el aviso de fin con el tiempo total.

## E. Control
- [ ] *Temp.*: cambia el objetivo del hotend y de la cama (pasos de 1/5/10 y preajustes). **Apaga** los calentadores al terminar.
- [ ] *Ventiladores*: cambia el porcentaje de Capa y Aux 1.
- [ ] ⚠ *Mover*: sin Home avisa; con Home, los pasos de 0,1/1/10 mueven el eje correcto y la posición mostrada coincide.
- [ ] ⚠ *Extruir*: con el hotend frío **bloquea** y lo dice; con el hotend caliente extruye/retrae.
- [ ] ⚠ *Herram. → Home*: Home todo/X/Y/Z mueven lo esperado; *Motores off* los libera.
- [ ] ⚠ *Herram. → Nivelar cama* (cama despejada): cada punto sube, se desplaza y baja.
- [ ] *Herram. → Nivel. auto* (si existe): **pide confirmación**.

## F. Cambio de filamento
- [ ] *Herram. → Filamento* muestra la temperatura del hotend y **Cargar / Descargar**.
- [ ] ⚠ *Cargar*: calienta (diálogo con la temperatura en vivo y *Cancelar*), pide insertar el filamento y muestra la barra de carga. **Cancela antes de extruir** si no quieres cargar.
- [ ] Durante una impresión, *Opciones → Filamento* **pide confirmación** (pausa y mueve el cabezal).

## G. Ajustes
- [ ] *Parámetros*: entra en Pasos por mm, Aceleración, Velocidades, Modo silencioso (interruptores), Posiciones de nivelación (dos valores por fila).
- [ ] **Teclado numérico:** muestra el nombre del parámetro y su valor; *Cancelar* sale sin cambiar; con un valor no numérico (solo "-" o ".") *Aceptar* queda deshabilitado; un valor válido se guarda y al volver a entrar se mantiene. **Anota los valores originales antes de probar.**
- [ ] *Memoria*: *Guardar* y *Leer* piden confirmación; *Restaurar valores de fábrica* pide confirmación **en rojo** (pulsa Cancelar).
- [ ] *Wi-Fi*: muestra el estado (IP, red); sin conexión abre la **lista de redes** (la conectada en verde) y al elegir una pide la clave con el teclado de texto.
- [ ] *Idioma*: solo *Español* e *Inglés*; el activo en verde.
- [ ] *Más → G-code*: *Escribir un comando* abre el teclado; `M115` (o `M114`) devuelve un resultado.
- [ ] ⚠ *Apagar / Motores off* **pide confirmación** (sin control de alimentación solo ofrece *Motores off*).

## H. Seguridad (no deben ocurrir)
- [ ] Ninguna pulsación accidental mueve la máquina ni calienta sin una acción intencionada.
- [ ] Ninguna pantalla se queda en negro ni reinicia la placa al entrar y salir varias veces.
- [ ] Con la impresión en marcha no se puede mover ejes ni apagar motores desde la interfaz.

## Registro
| Fecha | Quién | Versión | Resultado | Observaciones |
|---|---|---|---|---|
| | | 1.0.0 | | |
