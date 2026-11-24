/**
 * ARQUIS - firmware unico XQ1 / Aquila D1: asistente del primer arranque y Ajustes > Maquina.
 *
 * Usa solo componentes existentes: lista de parametros (arq_pl_*), teclado numerico (arq_numpad_*), dialogos
 * (arq_dialog_open) y un chip de estado. Las MISMAS paginas sirven al asistente (Anterior / Siguiente) y al menu (Volver).
 * Nada se aplica ni se guarda hasta el final: Guardar valida, aplica, guarda la EEPROM de Marlin (M500) y, como ultimo
 * paso, el registro ARQUIS. Sin terminar el asistente no se puede mover la maquina (ver arquis_machine.cpp).
 */
#pragma once

#include "arquis_machine.h"

#if ARQ_HAS_MACHINE
  void arq_machine_wizard_draw();   // asistente obligatorio (lv_draw_ready_print lo abre si no hay perfil valido)
  void arq_machine_menu_draw();     // Ajustes > Maquina
  void arq_machine_ui_draw();       // draw_ui: MACHINE_UI (vuelve a la pagina en la que estaba)
  void arq_machine_ui_clear();
  void arq_machine_tick();          // LV_TASK_HANDLER: verificacion de los TMC, avisos y estado de las pruebas
#endif
