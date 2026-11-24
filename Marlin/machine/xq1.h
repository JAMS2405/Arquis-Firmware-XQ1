#pragma once
// REFERENCIA (firmware unico): el firmware YA NO incluye este archivo. Los valores de fabrica del Arquis XQ1 estan en la tabla de
// Marlin/src/lcd/extui/mks_ui/arquis_machine_logic.cpp; tests/machines/test_profiles.py comprueba que coinciden con
// Configuration.h + este archivo. Si cambias un valor aqui, cambia tambien la tabla (la prueba lo detecta).
// Perfil Arquis XQ1 (300x300x450). Salvo lo de abajo, sus valores son los de Configuration.h / Configuration_adv.h.

// PRUEBA (Z no subia al imprimir): Z mas lento y con menos aceleracion, como antes del commit c5b5cfd (Z 45 mm/s y 150 mm/s2).
// Con 791,74 pasos/mm, 45 mm/s son ~35.600 pasos/s y el motor de Z puede perder pasos. Si con esto sube bien, se puede ir
// subiendo poco a poco (8, 10, 15...) hasta encontrar el limite; si no mejora, el problema es otro (mecanica o corriente de Z).
#define DEFAULT_MAX_FEEDRATE          { 500, 500, 5, 60 }
#define DEFAULT_MAX_ACCELERATION      { 4500, 4500, 100, 10000 }
