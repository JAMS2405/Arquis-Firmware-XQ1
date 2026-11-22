; ARQUIS - G-code de FIN universal (firmware Arquis / Marlin)
; Sin variables del laminador. Sirve igual en Cura, PrusaSlicer, OrcaSlicer, IdeaMaker...
M400                   ; esperar a que terminen los movimientos
G91                    ; relativo
G1 E-2 F2400           ; retraer un poco para no dejar hilos
G90                    ; absoluto
G27 P2                 ; sube 20 mm (limitado por el maximo de Z) y aparca atras a la izquierda
M104 S0                ; hotend apagado
M140 S0                ; cama apagada
M107                   ; ventilador apagado
M220 S100              ; velocidad al 100 %
M221 S100              ; flujo al 100 %
M84 X Y E              ; soltar motores X, Y y extrusor. Z se queda sujeto para que no caiga
