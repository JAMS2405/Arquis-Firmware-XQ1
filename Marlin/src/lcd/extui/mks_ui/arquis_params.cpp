/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */


#include "../../../inc/MarlinConfigPre.h"

#if HAS_TFT_LVGL_UI && ENABLED(ARQUIS_UI)

#include "arquis_params.h"
#include "arquis_params_view.h"
#include "draw_ui.h"
#include <string.h>

// ---- Traduccion al espanol -------------------------------------------------------------------------------------
// MKS no trae el menu de parametros en espanol (solo ingles, chino y ruso). Se identifica cada texto por el CAMPO de la tabla de
// idioma al que pertenece (comparacion de punteros): no se guardan los textos en ingles, solo la traduccion.
typedef struct { const char * const *field; const char *es; } es_item_t;
static const es_item_t es_table[] = {
  { &MachinePara_menu.AdvanceSetting, "Ajustes avanzados" },
  { &MachinePara_menu.MachineSetting, "Configuraci\xC3\xB3n de la m\xC3\xA1quina" },
  { &MachinePara_menu.MotorSetting, "Motores" },
  { &MachinePara_menu.leveling, "Nivelaci\xC3\xB3n" },
  { &common_menu.text_back, "Volver" },
  { &eeprom_menu.revert, "Restaurar valores de f\xC3\xA1" "brica" },
  { &eeprom_menu.store, "Guardar ajustes en la memoria" },
  { &leveling_menu.position1, "Punto 1" },
  { &leveling_menu.position2, "Punto 2" },
  { &leveling_menu.position3, "Punto 3" },
  { &leveling_menu.position4, "Punto 4" },
  { &leveling_menu.position5, "Punto 5" },
  { &machine_menu.AccelerationConf, "Aceleraci\xC3\xB3n" },
  { &machine_menu.AccelerationConfTitle, "Aceleraci\xC3\xB3n" },
  { &machine_menu.AdvancedConfTitle, "Ajustes avanzados" },
  { &machine_menu.E0MaxFeedRate, "Velocidad m\xC3\xA1x. E0" },
  { &machine_menu.E1MaxFeedRate, "Velocidad m\xC3\xA1x. E1" },
  { &machine_menu.E0_Acceleration, "Aceleraci\xC3\xB3n E0" },
  { &machine_menu.E1_Acceleration, "Aceleraci\xC3\xB3n E1" },
  { &machine_menu.E0_Current, "Corriente E0 (mA)" },
  { &machine_menu.E1_Current, "Corriente E1 (mA)" },
  { &machine_menu.E0_StepMode, "StealthChop E0" },
  { &machine_menu.E1_StepMode, "StealthChop E1" },
  { &machine_menu.E0_Steps, "Pasos/mm E0" },
  { &machine_menu.E1_Steps, "Pasos/mm E1" },
  { &machine_menu.E_Jerk, "Jerk del extrusor" },
  { &machine_menu.EncoderConfText, "\xC2\xBFUsar la perilla (encoder)?" },
  { &machine_menu.EncoderConfTitle, "Perilla (encoder)" },
  { &machine_menu.EncoderSettings, "Perilla (encoder)" },
  { &machine_menu.FilamentConf, "Ajustes de filamento" },
  { &machine_menu.FilamentConfTitle, "Ajustes de filamento" },
  { &machine_menu.FilamentTemperature, "Temperatura del filamento" },
  { &machine_menu.HomingSensitivityConf, "Sensibilidad de home" },
  { &machine_menu.HomingSensitivityConfTitle, "Sensibilidad de home" },
  { &machine_menu.InLength, "Longitud de carga" },
  { &machine_menu.InSpeed, "Velocidad de carga" },
  { &machine_menu.OutLength, "Longitud de descarga" },
  { &machine_menu.OutSpeed, "Velocidad de descarga" },
  { &machine_menu.JerkConf, "Jerk" },
  { &machine_menu.JerkConfTitle, "Jerk (velocidad de arranque)" },
  { &machine_menu.LevelingAutoCommandConf, "Comando de autonivelaci\xC3\xB3n" },
  { &machine_menu.LevelingAutoZoffsetConf, "Offset boquilla-sonda" },
  { &machine_menu.LevelingParaConfTitle, "Ajustes de nivelaci\xC3\xB3n" },
  { &machine_menu.LevelingZoffsetTitle, "Asistente de offset Z" },
  { &machine_menu.MachineConfigTitle, "Configuraci\xC3\xB3n de la m\xC3\xA1quina" },
  { &machine_menu.MaxFeedRateConf, "Velocidades m\xC3\xA1ximas" },
  { &machine_menu.MaxFeedRateConfTitle, "Velocidades m\xC3\xA1ximas" },
  { &machine_menu.MotorConfTitle, "Motores" },
  { &machine_menu.OffsetConfTitle, "Offset boquilla-sonda" },
  { &machine_menu.PausePosText, "Posici\xC3\xB3n de pausa" },
  { &machine_menu.PausePosition, "Posici\xC3\xB3n de pausa" },
  { &machine_menu.PrintAcceleration, "Aceleraci\xC3\xB3n de impresi\xC3\xB3n" },
  { &machine_menu.RetractAcceleration, "Aceleraci\xC3\xB3n de retracci\xC3\xB3n" },
  { &machine_menu.TravelAcceleration, "Aceleraci\xC3\xB3n de desplazamiento" },
  { &machine_menu.StepsConf, "Pasos por mm" },
  { &machine_menu.StepsConfTitle, "Pasos por mm" },
  { &machine_menu.TMCStepModeConf, "Modo silencioso (StealthChop)" },
  { &machine_menu.TmcStepModeConfTitle, "Modo silencioso (StealthChop)" },
  { &machine_menu.TMCcurrentConf, "Corriente de motores (TMC)" },
  { &machine_menu.TmcCurrentConfTitle, "Corriente de motores (mA)" },
  { &machine_menu.TrammingPosConf, "Posiciones de nivelaci\xC3\xB3n manual" },
  { &machine_menu.WifiSettings, "Par\xC3\xA1metros Wi-Fi" },
  { &machine_menu.next, "Siguiente" },
  { &machine_menu.previous, "Anterior" },
  { &machine_menu.xPos, "Posici\xC3\xB3n X" },
  { &machine_menu.yPos, "Posici\xC3\xB3n Y" },
  { &machine_menu.zPos, "Posici\xC3\xB3n Z" },
  { &machine_menu.Xoffset, "Offset X" },
  { &machine_menu.Yoffset, "Offset Y" },
  { &machine_menu.Zoffset, "Offset Z" },
  { &machine_menu.XMaxFeedRate, "Velocidad m\xC3\xA1x. eje X" },
  { &machine_menu.X_Acceleration, "Aceleraci\xC3\xB3n m\xC3\xA1x. eje X" },
  { &machine_menu.X_Current, "Corriente eje X (mA)" },
  { &machine_menu.X_Jerk, "Jerk eje X" },
  { &machine_menu.X_Sensitivity, "Sensibilidad eje X" },
  { &machine_menu.X_StepMode, "StealthChop eje X" },
  { &machine_menu.X_Steps, "Pasos/mm eje X" },
  { &machine_menu.YMaxFeedRate, "Velocidad m\xC3\xA1x. eje Y" },
  { &machine_menu.Y_Acceleration, "Aceleraci\xC3\xB3n m\xC3\xA1x. eje Y" },
  { &machine_menu.Y_Current, "Corriente eje Y (mA)" },
  { &machine_menu.Y_Jerk, "Jerk eje Y" },
  { &machine_menu.Y_Sensitivity, "Sensibilidad eje Y" },
  { &machine_menu.Y_StepMode, "StealthChop eje Y" },
  { &machine_menu.Y_Steps, "Pasos/mm eje Y" },
  { &machine_menu.ZMaxFeedRate, "Velocidad m\xC3\xA1x. eje Z" },
  { &machine_menu.Z_Acceleration, "Aceleraci\xC3\xB3n m\xC3\xA1x. eje Z" },
  { &machine_menu.Z_Current, "Corriente eje Z (mA)" },
  { &machine_menu.Z_Jerk, "Jerk eje Z" },
  { &machine_menu.Z_Sensitivity, "Sensibilidad eje Z" },
  { &machine_menu.Z_StepMode, "StealthChop eje Z" },
  { &machine_menu.Z_Steps, "Pasos/mm eje Z" },
  { &machine_menu.Z2_Sensitivity, "Sensibilidad eje Z2" },
};

const char * arq_es(const char *t) {
  if (!t) return t;
  for (uint8_t i = 0; i < COUNT(es_table); ++i) if (*es_table[i].field == t) return es_table[i].es;
  return t;
}

// ---- Pantallas que usan la lista de parametros -----------------------------------------------------------------------
bool arq_params_state(int s) {
  switch ((DISP_STATE)s) {
    case MACHINE_PARA_UI: case MACHINE_SETTINGS_UI: case MOTOR_SETTINGS_UI: case STEPS_UI: case ACCELERATION_UI: case JERK_UI:
    case MAXFEEDRATE_UI: case TMC_CURRENT_UI: case TMC_MODE_UI: case HOMING_SENSITIVITY_UI: case ADVANCED_UI: case LEVELING_PARA_UI:
    case MANUAL_LEVELING_POSITION_UI: case NOZZLE_PROBE_OFFSET_UI: case FILAMENT_SETTINGS_UI: case ENCODER_SETTINGS_UI: case PAUSE_POS_UI:
    case EEPROM_SETTINGS_UI:
      return true;
    default: return false;
  }
}

void arq_params_begin(lv_obj_t *scr, int s, const char *title) {
  if (!title) title = (s == MACHINE_PARA_UI) ? "Par\xC3\xA1metros" : (s == EEPROM_SETTINGS_UI) ? "Memoria (EEPROM)" : "";
  arq_pl_begin(scr, arq_es(title));
}

bool arq_params_button(lv_obj_t *par, const char *text, lv_event_cb_t cb, int id) {
  if (text == common_menu.text_back) arq_pl_back(par, cb, (uint16_t)id);
  else                               arq_pl_page(par, arq_es(text), cb, (uint16_t)id);
  return true;
}

void arq_params_row(lv_obj_t *par, const char *text, lv_coord_t y, lv_event_cb_t cb, int id, uint8_t kind, const char *v1, const char *v2, int id2, bool on) {
  int row = (y - PARA_UI_POS_Y) / PARA_UI_POS_Y;           // las pantallas originales colocan la fila n en y = PARA_UI_POS_Y * n
  if (row < 0) row = 0;
  if (row >= ARQ_PL_ROWS) row = ARQ_PL_ROWS - 1;
  arq_pl_row_t r;
  memset(&r, 0, sizeof(r));
  r.name = arq_es(text);
  r.kind = kind;
  r.on   = on;
  r.id1  = (int16_t)id;
  r.id2  = (int16_t)id2;
  if (v1) strncpy(r.v1, v1, sizeof(r.v1) - 1);             // el original pasa public_buf_l, que se reutiliza en cada fila: se copia
  if (v2) strncpy(r.v2, v2, sizeof(r.v2) - 1);
  arq_pl_row(par, (uint8_t)row, &r, cb);
}

void arq_params_onoff_update(bool on) { arq_pl_set_on(on); }

bool arq_numpad_value_ok(const char *s) {
  if (!s) return false;
  for (; *s; ++s) if (*s >= '0' && *s <= '9') return true;
  return false;
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
