; ARQUIS - G-code de INICIO universal (firmware Arquis / Marlin)
; Sin variables del laminador: sirve igual en Cura, PrusaSlicer, OrcaSlicer, IdeaMaker...
; Las temperaturas las pone el laminador solo (ver docs/arquis/GCODE_INICIO_FIN.md)
G21                    ; milimetros
G90                    ; coordenadas absolutas en XYZ
M220 S100              ; velocidad al 100 %
M221 S100              ; flujo al 100 %
M107                   ; ventilador de pieza apagado
G92 E0                 ; poner el extrusor a cero
G28                    ; Home XYZ. El firmware activa solo la malla 5x5 guardada. NO usar G29 (no hay sonda)
G1 Z5 F600             ; subir antes de moverse
G1 X10 Y10 F5000       ; esquina delantera izquierda (vale para cualquier cama)
G1 Z0.3 F600           ; altura de la linea de purga
G1 X90 Y10 E9 F1200    ; linea de purga de 80 mm
G1 X92 Y10 Z2 F3000    ; limpiar la gota y subir
G92 E0                 ; extrusor a cero otra vez
