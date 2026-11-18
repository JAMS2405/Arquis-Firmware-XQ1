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
  { &print_file_dialog_menu.confirm, "Aceptar" },
  { &print_file_dialog_menu.cancel, "Cancelar" },
  { &print_file_dialog_menu.print_file, "\xC2\xBFImprimir este modelo?" },
  { &print_file_dialog_menu.cancel_print, "\xC2\xBF" "Detener la impresi\xC3\xB3n?" },
  { &print_file_dialog_menu.retry, "Reintentar" },
  { &print_file_dialog_menu.stop, "Detener" },
  { &print_file_dialog_menu.no_file_print_tips, "Error: no hay archivo. Rev\xC3\xADsalo e int\xC3\xA9ntalo de nuevo." },
  { &print_file_dialog_menu.print_from_breakpoint, "\xC2\xBFReanudar la impresi\xC3\xB3n desde el punto de corte?" },
  { &print_file_dialog_menu.close_machine_error, "Error de comunicaci\xC3\xB3n.\nComprueba que la velocidad (baudios) de la pantalla coincida con la de la placa." },
  { &print_file_dialog_menu.filament_no_press, "El interruptor de detecci\xC3\xB3n de filamento no est\xC3\xA1 pulsado" },
  { &print_file_dialog_menu.print_finish, "\xC2\xA1Impresi\xC3\xB3n terminada!" },
  { &print_file_dialog_menu.print_time, "Tiempo de impresi\xC3\xB3n: " },
  { &print_file_dialog_menu.reprint, "Imprimir de nuevo" },
  { &print_file_dialog_menu.wifi_enable_tips, "Configurando el m\xC3\xB3" "dulo Wi-Fi.\nEspera un momento..." },
  { &pause_msg_menu.pausing, "Estacionando el cabezal..." },
  { &pause_msg_menu.changing, "Espera: va a empezar el cambio de filamento" },
  { &pause_msg_menu.unload, "Espera: descargando el filamento" },
  { &pause_msg_menu.waiting, "Pulsa el bot\xC3\xB3n para reanudar la impresi\xC3\xB3n" },
  { &pause_msg_menu.insert, "Inserta el filamento y pulsa el bot\xC3\xB3n para continuar" },
  { &pause_msg_menu.load, "Espera: cargando el filamento" },
  { &pause_msg_menu.purge, "Espera: purgando el filamento" },
  { &pause_msg_menu.resume, "Espera: se reanuda la impresi\xC3\xB3n..." },
  { &pause_msg_menu.heat, "Pulsa el bot\xC3\xB3n para calentar la boquilla" },
  { &pause_msg_menu.heating, "Calentando la boquilla. Espera..." },
  { &pause_msg_menu.option, "\xC2\xBFPurgar m\xC3\xA1s o continuar la impresi\xC3\xB3n?" },
  { &pause_msg_menu.purgeMore, "Purgar" },
  { &pause_msg_menu.continuePrint, "Continuar" },
  { &eeprom_menu.storeTips, "\xC2\xBFGuardar los ajustes en la memoria?" },
  { &eeprom_menu.readTips, "\xC2\xBFLeer los ajustes de la memoria?" },
  { &eeprom_menu.revertTips, "\xC2\xBFRestaurar los valores de f\xC3\xA1" "brica?" },
  { &filament_menu.filament_dialog_load_heat, "Calentando la boquilla.\nEspera..." },
  { &filament_menu.filament_dialog_unload_heat, "Calentando la boquilla.\nEspera..." },
  { &filament_menu.filament_dialog_load_heat_confirm, "Calentamiento completo.\nIntroduce el filamento en el extrusor y pulsa Aceptar para empezar a cargar." },
  { &filament_menu.filament_dialog_unload_heat_confirm, "Calentamiento completo.\nPulsa Aceptar para empezar a descargar." },
  { &filament_menu.filament_dialog_loading, "Cargando el filamento. Espera..." },
  { &filament_menu.filament_dialog_unloading, "Descargando el filamento. Espera..." },
  { &filament_menu.filament_dialog_load_completed, "Carga completada.\nPulsa Aceptar para volver." },
  { &filament_menu.filament_dialog_unload_completed, "Descarga completada.\nPulsa Aceptar para volver." },
  { &machine_menu.wifiConfigTips, "\xC2\xBF" "Configurar el Wi-Fi?" },
  { &common_menu.dialog_confirm_title, "Confirmar" },
  { &common_menu.close_machine_tips, "Apagando la m\xC3\xA1quina..." },
  { &common_menu.unbind_printer_tips, "\xC2\xBF" "Desvincular la impresora?" },
};

const char * arq_es(const char *t) {
  if (!t) return t;
  for (uint8_t i = 0; i < COUNT(es_table); ++i) if (*es_table[i].field == t) return es_table[i].es;          // por puntero (rapido)
  for (uint8_t i = 0; i < COUNT(es_table); ++i) if (*es_table[i].field && strcmp(*es_table[i].field, t) == 0) return es_table[i].es;   // por contenido (texto copiado por LVGL)
  return t;
}

// ---- Pantallas que usan la lista de parametros -----------------------------------------------------------------------
bool arq_params_state(int s) {
  switch ((DISP_STATE)s) {
    case MACHINE_PARA_UI: case MACHINE_SETTINGS_UI: case MOTOR_SETTINGS_UI: case STEPS_UI: case ACCELERATION_UI: case JERK_UI:
    case MAXFEEDRATE_UI: case TMC_CURRENT_UI: case TMC_MODE_UI: case HOMING_SENSITIVITY_UI: case ADVANCED_UI: case LEVELING_PARA_UI:
    case MANUAL_LEVELING_POSITION_UI: case FILAMENT_SETTINGS_UI: case ENCODER_SETTINGS_UI: case PAUSE_POS_UI:
    case EEPROM_SETTINGS_UI:
    #if HAS_BED_PROBE
      case NOZZLE_PROBE_OFFSET_UI:     // (el enum de MKS solo lo define con sonda)
    #endif
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

// ---- Dialogos ----------------------------------------------------------------------------------------------------
static lv_event_cb_t d_ok, d_cancel;

static void dlg_event(lv_obj_t *o, lv_event_t ev) {
  if (ev != LV_EVENT_RELEASED) return;
  if (o->mks_obj_id == ARQ_MSG_OK) { if (d_ok) d_ok(o, ev); }
  else if (d_cancel) d_cancel(o, ev);                       // el manejador original libera la pantalla: no se usa 'o' despues
}

static const char * btn_text(lv_obj_t *btn) {
  if (!btn) return nullptr;
  lv_obj_t *l = lv_obj_get_child(btn, nullptr);
  return l ? lv_label_get_text(l) : nullptr;
}

void arq_dialog_restyle(lv_obj_t *scr, lv_obj_t *label, lv_obj_t *btnOk, lv_obj_t *btnCancel, lv_event_cb_t ok_cb, lv_event_cb_t cancel_cb, bool danger, bool compact) {
  char msg[200] = "", ok[24] = "", cancel[24] = "";
  const char *t;
  if (label && (t = lv_label_get_text(label))) strncpy(msg, arq_es(t), sizeof(msg) - 1);
  if ((t = btn_text(btnOk)))     strncpy(ok, arq_es(t), sizeof(ok) - 1);
  if ((t = btn_text(btnCancel))) strncpy(cancel, arq_es(t), sizeof(cancel) - 1);
  const bool two = btnOk && btnCancel;
  if (label)     lv_obj_del(label);                          // objetos originales (estilo MKS)
  if (btnOk)     lv_obj_del(btnOk);
  if (btnCancel) lv_obj_del(btnCancel);
  d_ok = ok_cb; d_cancel = cancel_cb;
  arq_msg_build(scr, dlg_event, (two || danger) ? "Confirmar" : "Aviso", msg, btnOk ? ok : nullptr, btnCancel ? cancel : nullptr, danger, compact);
}

bool arq_numpad_value_ok(const char *s) {
  if (!s) return false;
  for (; *s; ++s) if (*s >= '0' && *s <= '9') return true;
  return false;
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
