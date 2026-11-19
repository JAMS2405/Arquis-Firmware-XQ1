#include "prelude.h"
#include "co.cpp"
#define CHECK(c) do{ if(!(c)){ printf("FALLA: %s (linea %d)\n",#c,__LINE__); fails++; } }while(0)
int main(){ int fails=0;
  cancelable.reset();
  // objeto 0: cuadrado 20..60 x 20..60 (varios G1 con extrusion)
  cancelable.set_active_object(0);
  float p0[][2]={{20,20},{60,20},{60,60},{20,60},{20,20}}; for(auto&p:p0) cancelable.note_extrusion(p[0],p[1]);
  CHECK(cancelable.has_bbox(0)); CHECK(cancelable.bbox_x0[0]==20&&cancelable.bbox_x1[0]==60&&cancelable.bbox_y0[0]==20&&cancelable.bbox_y1[0]==60);
  // objeto 1 todavia sin imprimir
  cancelable.set_active_object(1); CHECK(cancelable.object_count==2); CHECK(!cancelable.has_bbox(1));
  float p1[][2]={{150.4f,200.9f},{180,230},{149.7f,229.2f}}; for(auto&p:p1) cancelable.note_extrusion(p[0],p[1]);
  CHECK(cancelable.bbox_x0[1]==149&&cancelable.bbox_x1[1]==180&&cancelable.bbox_y0[1]==200&&cancelable.bbox_y1[1]==230);
  // entrelazado por capas: volver al objeto 0 amplia sin perder lo anterior
  cancelable.set_active_object(0); cancelable.note_extrusion(25,25); cancelable.note_extrusion(70,15);
  CHECK(cancelable.bbox_x0[0]==20&&cancelable.bbox_x1[0]==70&&cancelable.bbox_y0[0]==15&&cancelable.bbox_y1[0]==60);
  // sin objeto activo no registra nada
  cancelable.clear_active_object(); cancelable.note_extrusion(5,5); CHECK(cancelable.bbox_x0[0]==20&&cancelable.bbox_x1[1]==180);
  // cancelar / restaurar y 'skipping'
  cancelable.set_active_object(1); CHECK(!cancelable.skipping);
  cancelable.cancel_object(1); CHECK(cancelable.is_canceled(1)); CHECK(cancelable.skipping);
  cancelable.uncancel_object(1); CHECK(!cancelable.is_canceled(1)); CHECK(!cancelable.skipping);
  cancelable.cancel_object(0); cancelable.set_active_object(0); CHECK(cancelable.skipping);
  // limites (Etapa 3): capacidad de diseno 30 (IDs 0..29); 30, 31, 32, 256 y -2 se rechazan sin romper ni tocar otro objeto
  cancelable.set_active_object(29); cancelable.note_extrusion(1,2); CHECK(cancelable.has_bbox(29)); CHECK(cancelable.object_count==30); CHECK(!cancelable.overflow);
  cancelable.note_extrusion(1000,-50);
  const int32_t bad[]={30,31,32,256,300,-2,65536,2147483647};
  for(int32_t v:bad){ cancelable.set_active_object(v); CHECK(cancelable.active_object==-1&&!cancelable.skipping); cancelable.note_extrusion(9,9); CHECK(!cancelable.has_bbox(v)); CHECK(cancelable.overflow);
    cancelable.overflow=false; uint32_t before=cancelable.canceled; cancelable.cancel_object(v); cancelable.uncancel_object(v); CHECK(cancelable.canceled==before); CHECK(cancelable.overflow); cancelable.overflow=false; }
  cancelable.set_active_object(-1); CHECK(!cancelable.overflow);                 // S-1 es valido (no es un objeto)
  cancelable.set_object_count(200); CHECK(cancelable.object_count==30&&cancelable.overflow); cancelable.overflow=false;
  cancelable.set_object_count(-4); CHECK(cancelable.object_count==0&&cancelable.overflow); cancelable.overflow=false;
  cancelable.note_z(0.2f); CHECK(cancelable.z_print==2); cancelable.note_z(12.34f); CHECK(cancelable.z_print==123); cancelable.note_z(-5); CHECK(cancelable.z_print==0); cancelable.note_z(99999); CHECK(cancelable.z_print==65535);
  // nueva impresion (M486 T): reset limpia todo, incluidas las cajas
  cancelable.reset(); CHECK(!cancelable.has_bbox(0)&&!cancelable.has_bbox(1)&&!cancelable.has_bbox(29)&&!cancelable.overflow&&cancelable.z_print==0); CHECK(cancelable.object_count==0&&cancelable.canceled==0);
  printf(fails? "CANCEL OBJECT: %d fallas\n":"CANCEL OBJECT: OK (cajas XY, entrelazado, cancelar/restaurar, limites, reset)\n",fails); return fails!=0; }
