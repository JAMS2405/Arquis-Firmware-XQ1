#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
// ---- mocks minimos ----
struct lv_color_t { uint16_t full; };
#define LV_COLOR_BACKGROUND (lv_color_t{0x0881})
static inline lv_color_t lv_color_make(uint8_t r,uint8_t g,uint8_t b){ return lv_color_t{(uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3))}; }
static inline lv_color_t lv_color_mix(lv_color_t c1, lv_color_t c2, uint8_t mix){ // mezcla por canal RGB565 (igual que LVGL)
  auto ch=[&](int sh,int mask,uint16_t a,uint16_t b){ int x=(a>>sh)&mask, y=(b>>sh)&mask; return ((x*mix + y*(255-mix))/255)&mask; };
  return lv_color_t{(uint16_t)((ch(11,31,c1.full,c2.full)<<11)|(ch(5,63,c1.full,c2.full)<<5)|ch(0,31,c1.full,c2.full))}; }
#define SPI_QUARTER_SPEED 0
#define BAK_VIEW_ADDR_TFT35 0x1000
#define DEFAULT_VIEW_ADDR_TFT35 0x20000
uint8_t preview_src=1; int16_t preview_fill_line=-1;
uint8_t bmp_public_buf[17*1024];
static uint8_t flash[0x40000];            // "flash SPI": miniatura de prueba
static uint16_t screen_[200][200];        // "TFT" 200x200
static int win_y;
struct { void init(int){} void SPI_FLASH_BufferRead(uint8_t*b,uint32_t a,uint32_t n){ memcpy(b,flash+a,n);} } W25QXX;
struct TFTIO { void WriteSequence(uint16_t*p,int n){ memcpy(screen_[win_y],p,n*2);} };
struct { TFTIO tftio; void setWindow(int x,int y,int w,int h){ (void)x;(void)w;(void)h; win_y=y-36; } } SPI_TFT;
#include "fill_body.inc"
static uint16_t orig(int r,int c){ return (uint16_t)(0x1234 + r*7 + c*3); }
static uint16_t dim(uint16_t v){ return lv_color_mix(lv_color_t{v},LV_COLOR_BACKGROUND,70).full; }
int main(){
  for(int r=0;r<200;r++) for(int c=0;c<200;c++){ uint16_t v=orig(r,c); memcpy(flash+BAK_VIEW_ADDR_TFT35+r*400+c*2,&v,2); }
  int fails=0; const uint16_t mark=lv_color_make(0,0xB4,0xFF).full;
  int seq[]={0,1,3,10,10,49,50,51,50,99,100};  // incluye repetido y retroceso (reanudar/recalcular)
  for(int pct: seq){
    preview_fill_update(pct);
    int line=200-(200*pct)/100;
    for(int r=0;r<200;r++) for(int c=0;c<200;c++){
      uint16_t want = (r==line && pct<100)? mark : (r<line? dim(orig(r,c)) : orig(r,c));
      if(screen_[r][c]!=want){ if(fails<3) printf("FALLA pct=%d fila=%d col=%d\n",pct,r,c); fails++; }
    }
  }
  // repintado total tras volver de otra pantalla (la miniatura vuelve a todo color y la cache se invalida)
  for(int r=0;r<200;r++) for(int c=0;c<200;c++) screen_[r][c]=orig(r,c);
  preview_fill_line=-1; preview_fill_update(40);
  int line=200-80; for(int r=0;r<200;r++) for(int c=0;c<200;c++){ uint16_t want=(r==line)?mark:(r<line?dim(orig(r,c)):orig(r,c)); if(screen_[r][c]!=want) fails++; }
  printf(fails? "FILL: %d pixeles mal\n":"FILL: OK (11 actualizaciones + repintado total, 0 pixeles mal)\n", fails);
  return fails!=0;
}
