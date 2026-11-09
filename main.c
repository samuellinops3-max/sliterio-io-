// COPO AVENTURA - Boss Rush estilo Cuphead - PS3 PSL1GHT
// Copo = heroi original. 3 bosses, dash, tiro, pulo
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#define CB_SIZE (0x100000)
#define HOST_SIZE (32*1024*1024)
#define PI 3.14159265f
#define MAXB 80
#define MAXE 80

static gcmContextData *ctx;
static u32 W,H,*fb[2],fboff[2],*back;
static int cur=0,first_flip=1;
static volatile int running=1;
static void sysutil_cb(u64 s,u64 p,void*u){(void)p;(void)u;if(s==SYSUTIL_EXIT_GAME)running=0;}
static void init_screen(void){
 void *host=memalign(1024*1024,HOST_SIZE); rsxInit(&ctx,CB_SIZE,HOST_SIZE,host);
 videoState st; videoGetState(VIDEO_PRIMARY,0,&st); u32 resid=st.displayMode.resolution;
 if(videoGetResolutionAvailability(VIDEO_PRIMARY,VIDEO_RESOLUTION_720,VIDEO_ASPECT_AUTO,0)) resid=VIDEO_RESOLUTION_720;
 videoResolution res; videoGetResolution(resid,&res); W=res.width; H=res.height;
 videoConfiguration vc; memset(&vc,0,sizeof(vc)); vc.resolution=resid; vc.format=VIDEO_BUFFER_FORMAT_XRGB; vc.aspect=VIDEO_ASPECT_AUTO; vc.pitch=W*4;
 videoConfigure(VIDEO_PRIMARY,&vc,NULL,0); videoGetState(VIDEO_PRIMARY,0,&st); gcmSetFlipMode(GCM_FLIP_VSYNC);
 for(int i=0;i<2;i++){fb[i]=(u32*)rsxMemalign(64,W*H*4); gcmAddressToOffset(fb[i],&fboff[i]); gcmSetDisplayBuffer(i,fboff[i],W*4,W,H);}
 back=(u32*)memalign(64,W*H*4); memset(back,0,W*H*4); gcmResetFlipStatus();
}
static void wait_flip(void){while(gcmGetFlipStatus()!=0) usleep(200); gcmResetFlipStatus();}
static void present(void){if(!first_flip) wait_flip(); else gcmResetFlipStatus(); memcpy(fb[cur],back,W*H*4); gcmSetFlip(ctx,cur); rsxFlushBuffer(ctx); gcmSetWaitFlip(ctx); cur^=1; first_flip=0;}
static void fill_rect(int x,int y,int w,int h,u32 c){int x0=x<0?0:x,y0=y<0?0:y,x1=x+w,y1=y+h; if(x1>(int)W)x1=W; if(y1>(int)H)y1=H; for(int yy=y0;yy<y1;yy++){u32*p=back+yy*W; for(int xx=x0;xx<x1;xx++)p[xx]=c;}}
static void circle(int cx,int cy,int r,u32 c){if(r<1)r=1; if(cx+r<0||cx-r>=(int)W||cy+r<0||cy-r>=(int)H)return; for(int dy=-r;dy<=r;dy++){int y=cy+dy; if(y<0||y>=(int)H)continue; int hw=(int)sqrtf((float)(r*r-dy*dy)); int x0=cx-hw,x1=cx+hw; if(x0<0)x0=0; if(x1>=(int)W)x1=W-1; u32*p=back+y*W; for(int x=x0;x<=x1;x++)p[x]=c;}}
typedef struct{char c;const char*p;} Glyph;
static const Glyph glyphs[]={
 {'0',"###" "#.#" "#.#" "#.#" "###"},{'1',".#." "##." ".#." ".#." "###"},
 {'2',"###" "..#" "###" "#.." "###"},{'3',"###" "..#" "###" "..#" "###"},
 {'4',"#.#" "#.#" "###" "..#" "..#"},{'5',"###" "#.." "###" "..#" "###"},
 {'6',"###" "#.." "###" "#.#" "###"},{'7',"###" "..#" "..#" "..#" "..#"},
 {'8',"###" "#.#" "###" "#.#" "###"},{'9',"###" "#.#" "###" "..#" "###"},
 {'A',".#." "#.#" "###" "#.#" "#.#"},{'B',"##." "#.#" "##." "#.#" "##."},
 {'C',"###" "#.." "#.." "#.." "###"},{'D',"##." "#.#" "#.#" "#.#" "##."},
 {'E',"###" "#.." "###" "#.." "###"},{'F',"###" "#.." "###" "#.." "#.."},
 {'G',"###" "#.." "#.#" "#.#" "###"},{'H',"#.#" "#.#" "###" "#.#" "#.#"},
 {'I',"###" ".#." ".#." ".#." "###"},{'L',"#.." "#.." "#.." "#.." "###"},
 {'M',"#.#" "###" "###" "#.#" "#.#"},{'N',"#.#" "###" "###" "#.#" "#.#"},
 {'O',"###" "#.#" "#.#" "#.#" "###"},{'P',"###" "#.#" "###" "#.." "#.."},
 {'R',"##." "#.#" "##." "#.#" "#.#"},{'S',"###" "#.." "###" "..#" "###"},
 {'T',"###" ".#." ".#." ".#." ".#."},{'U',"#.#" "#.#" "#.#" "#.#" "###"},
 {'V',"#.#" "#.#" "#.#" "#.#" ".#."},{'X',"#.#" "#.#" ".#." "#.#" "#.#"},
 {'W',"#.#" "#.#" "###" "###" "#.#"},{'Y',"#.#" "#.#" ".#." ".#." ".#."},
 {'!',".#." ".#." ".#." "..." ".#."},{' ',"..." "..." "..." "..." "..."},
 {':',"...".#." "..." "..." ".#."},{'-',"..." "..." "###" "..." "..."},
};
static const char* find_glyph(char c){for(unsigned i=0;i<sizeof(glyphs)/sizeof(glyphs[0]);i++) if(glyphs[i].c==c) return glyphs[i].p; return NULL;}
static int text_width(const char*s,int sc){return (int)strlen(s)*4*sc-sc;}
static void draw_text(const char*s,int x,int y,int sc,u32 c){for(;*s;s++){const char*g=find_glyph(*s); if(g) for(int r=0;r<5;r++) for(int k=0;k<3;k++) if(g[r*3+k]=='#') fill_rect(x+k*sc,y+r*sc,sc,sc,c); x+=4*sc;}}

/* ---------- JOGO ---------- */
typedef struct{float x,y,vx,vy; int active,dir,big;} Bullet;
typedef struct{float x,y,vx,vy; int active,type,life;} Enemy;
static float px,py,pvy; static int pface=1,php=3,pinv=0,dash_t=0,dash_cd=0,onground=1,shoot_cd=0;
static int pad_l,pad_r,pad_jump,pad_shoot,pad_dash,pad_special,pad_jump_prev,pad_dash_prev;
static Bullet bul[MAXB];
static Enemy ene[MAXE];
static int boss_hp,boss_max,boss_type,boss_state,boss_timer,boss_x,boss_y,boss_vx,boss_dir,stage=0,game_state=0,win_timer=0,score=0,frame=0;
static float camshake=0;

static float frand(void){return (float)rand()/(float)RAND_MAX;}
static void spawn_bullet(float x,float y,float vx,int big){for(int i=0;i<MAXB;i++) if(!bul[i].active){bul[i].x=x;bul[i].y=y;bul[i].vx=vx;bul[i].vy=0;bul[i].active=1;bul[i].dir=vx>0?1:-1;bul[i].big=big;return;}}
static void spawn_enemy(float x,float y,float vx,float vy,int type){for(int i=0;i<MAXE;i++) if(!ene[i].active){ene[i].x=x;ene[i].y=y;ene[i].vx=vx;ene[i].vy=vy;ene[i].active=1;ene[i].type=type;ene[i].life=60;return;}}

static void start_boss(int s){
 stage=s; boss_type=s; boss_state=0; boss_timer=0;
 boss_x=(int)W-300; boss_y=(int)H-260; boss_vx=0; boss_dir=-1;
 if(s==0){boss_max=120; boss_hp=120;}
 if(s==1){boss_max=170; boss_hp=170; boss_y=(int)H-220;}
 if(s==2){boss_max=230; boss_hp=230; boss_y=180;}
 for(int i=0;i<MAXE;i++) ene[i].active=0;
 for(int i=0;i<MAXB;i++) bul[i].active=0;
 px=120; py=(int)H-180; pvy=0; php=3; pinv=60;
}
static void start_game(void){score=0; game_state=1; start_boss(0);}

/* CONTROLE ARRUMADO - procura em todas as portas, nao trava */
static void read_pad(void){
 padInfo info; padData d;
 pad_l=pad_r=pad_jump=pad_shoot=pad_dash=pad_special=0;
 ioPadGetInfo(&info);
 int found=-1; for(int p=0;p<7;p++) if(info.status[p]){found=p;break;}
 if(found<0){ static int retry=0; if(++retry>60){ioPadInit(7); retry=0;} return; }
 memset(&d,0,sizeof(d));
 if(ioPadGetData(found,&d)!=0) return;
 float ax=((int)d.ANA_L_H-128)/128.0f;
 if(ax<-0.3f) pad_l=1; if(ax>0.3f) pad_r=1;
 if(d.BTN_LEFT) pad_l=1; if(d.BTN_RIGHT) pad_r=1;
 pad_jump=d.BTN_CROSS;
 pad_shoot=d.BTN_SQUARE;
 pad_dash=d.BTN_CIRCLE;
 pad_special=d.BTN_R1;
}

static void hurt_player(void){
 if(pinv>0||dash_t>0||game_state!=1) return;
 php--; pinv=90; camshake=12;
 if(php<=0){game_state=2; win_timer=0;}
}

static void update_game(void){
 frame++;
 if(game_state==1){
  float ground=(float)H-140;
  // movimento
  float spd=5.5f;
  if(dash_t>0){dash_t--; spd=13.0f; }
  if(pad_l){px-=spd; pface=-1;}
  if(pad_r){px+=spd; pface=1;}
  if(px<30) px=30; if(px>W-60) px=W-60;
  // pulo
  if(pad_jump && !pad_jump_prev && onground){pvy=-14.5f; onground=0;}
  pvy+=0.7f; if(pvy>16) pvy=16;
  py+=pvy; if(py>=ground){py=ground; pvy=0; onground=1;}
  // dash
  if(dash_cd>0) dash_cd--;
  if(pad_dash && !pad_dash_prev && dash_cd==0){dash_t=14; dash_cd=45; pinv=20;}
  // tiro
  if(shoot_cd>0) shoot_cd--;
  if(pad_shoot && shoot_cd==0){spawn_bullet(px+30*pface,py+10,9*pface,0); shoot_cd=9; score+=1;}
  if(pad_special && shoot_cd==0){spawn_bullet(px+30*pface,py+10,9*pface,1); shoot_cd=22; score+=2;}
  if(pinv>0) pinv--;
  // balas do jogador
  for(int i=0;i<MAXB;i++) if(bul[i].active){
   bul[i].x+=bul[i].vx;
   if(bul[i].x<0||bul[i].x>W) bul[i].active=0;
   // acertou boss?
   if(bul[i].active && boss_hp>0){
    float dx=bul[i].x-boss_x, dy=bul[i].y-boss_y;
    if(dx*dx+dy*dy < 90*90){
     bul[i].active=0; boss_hp -= bul[i].big?8:2; camshake=4;
     if(boss_hp<=0){score+=500; game_state=3; win_timer=0; for(int k=0;k<MAXE;k++) ene[k].active=0;}
    }
   }
  }
  // --- BOSS IA ---
  boss_timer++;
  int bt=boss_timer%240;
  if(boss_type==0){ // FLOR
   boss_y=(int)H-260 + (int)(sinf(frame*0.05f)*12);
   if(bt==30) spawn_enemy(boss_x-40,boss_y, -4, -3, 0);
   if(bt==90) spawn_enemy(boss_x-40,boss_y, -5, 0, 0);
   if(bt==150) spawn_enemy(boss_x-40,boss_y, -4, 3, 0);
   if(bt==200) spawn_enemy(boss_x-40,boss_y, -6, -1, 1);
  }
  if(boss_type==1){ // SAPO
   if(boss_state==0 && boss_timer%120==0){boss_state=1; boss_timer=0; boss_vx=-6;}
   if(boss_state==1){boss_x+=boss_vx; boss_y=(int)H-220 - (int)(sinf(frame*0.2f)*60); if(boss_x<600){boss_state=2; boss_timer=0;}}
   if(boss_state==2){boss_x+=3; if(boss_x>W-300){boss_state=0; boss_timer=0;} if(boss_timer%40==0) spawn_enemy(boss_x,boss_y-30,-5,-2,1);}
  }
  if(boss_type==2){ // DRAGAO
   boss_x=(int)W-320 + (int)(sinf(frame*0.03f)*80);
   boss_y=180 + (int)(sinf(frame*0.06f)*50);
   if(bt==40||bt==110||bt==180) spawn_enemy(boss_x-60,boss_y+20,-7,0,2);
   if(bt==220) spawn_enemy(boss_x-60,boss_y+40,-5,2,2);
  }
  // inimigos do boss
  for(int i=0;i<MAXE;i++) if(ene[i].active){
   ene[i].x+=ene[i].vx; ene[i].y+=ene[i].vy; ene[i].life--;
   if(ene[i].type==0) ene[i].vy+=0.15f;
   if(ene[i].x<0||ene[i].y>H||ene[i].life<=0) ene[i].active=0;
   float dx=ene[i].x-px, dy=ene[i].y-(py+10);
   if(dx*dx+dy*dy < 32*32) {ene[i].active=0; hurt_player();}
  }
  // encostou no boss
  {float dx=px-boss_x, dy=py-boss_y; if(dx*dx+dy*dy < 85*85) hurt_player();}
 }
 if(game_state==2){win_timer++; if(win_timer>120 && pad_jump){start_game();}}
 if(game_state==3){win_timer++; if(win_timer>140){ if(stage>=2){game_state=4;} else start_boss(stage+1); }}
 if(game_state==4 && pad_jump && win_timer>30){start_game();}
 if(game_state==0 && pad_jump && !pad_jump_prev){start_game();}
 pad_jump_prev=pad_jump; pad_dash_prev=pad_dash;
 if(camshake>0) camshake-=0.5f;
}

static void render(void){
 float sc=(float)W/1280.0f; int sh=(int)(camshake*(frand()-0.5f)*2);
 for(u32 i=0;i<W*H;i++) back[i]=0x00F7E7C3; // fundo papel antigo
 // ceu
 fill_rect(0,0,W,(int)H*0.55f,0x00A8D8F0);
 // nuvens
 for(int i=0;i<4;i++) {int cx=(frame*0.3f+i*320); cx%= (int)W+200; cx-=100; circle(cx+sh,80+i*30,28,0x00FFFFFF); circle(cx+30+sh,80+i*30,32,0x00FFFFFF);}
 // chao
 fill_rect(0,(int)H-140,W,140,0x005A3D1E);
 fill_rect(0,(int)H-140,W,12,0x007AB648);
 // plataforma
 fill_rect(0,(int)H-140,W,6,0x00FFD23F);
 int ui=(int)(3*sc+0.5f); if(ui<2) ui=2;

 if(game_state==0){
  draw_text("COPO", (int)W/2-180, 120, 10, 0x00FF4D4D);
  draw_text("AVENTURA", (int)W/2-180, 220, 7, 0x00222222);
  draw_text("X PARA COMECAR", (int)W/2-180, 380, 4, 0x00222222);
  draw_text("QUADRADO ATIRA BOLA DASH", (int)W/2-260, 450, 3, 0x00555555);
  // copo parado
  circle(200, (int)H-220, 55, 0x00FFFFFF);
  fill_rect(150,(int)H-260,100,20,0x00FF4D4D);
  circle(200,(int)H-220,20,0x00000000);
  return;
 }
 // jogador - COPO
 if(game_state!=2){
  int blink = (pinv>0 && (frame/6)%2==0);
  if(!blink){
   int pxx=(int)px+sh, pyy=(int)py+sh;
   // corpo copo branco
   fill_rect(pxx-28,pyy-20,56,62,0x00FFFFFF);
   fill_rect(pxx-28,pyy-20,56,10,0x00FF4D4D);
   // canudo
   fill_rect(pxx+10,pyy-45,8,30,0x00FF4D4D);
   // olho
   circle(pxx+10*pface,pyy+5,9,0x00000000);
   circle(pxx+12*pface,pyy+2,3,0x00FFFFFF);
   // pernas
   fill_rect(pxx-18,pyy+42,12,18,0x00222222);
   fill_rect(pxx+8,pyy+42,12,18,0x00222222);
   // braco atirando
   if(pad_shoot) fill_rect(pxx+28*pface,pyy+10,28,10,0x00FFFFFF);
   if(dash_t>0) fill_rect(pxx-50,pyy+10,100,8,0x00FFD23F);
  }
 }
 // balas
 for(int i=0;i<MAXB;i++) if(bul[i].active){
  if(bul[i].big) circle((int)bul[i].x+sh,(int)bul[i].y+sh,14,0x00FFD23F);
  else circle((int)bul[i].x+sh,(int)bul[i].y+sh,7,0x00FFFFFF);
 }
 // inimigos
 for(int i=0;i<MAXE;i++) if(ene[i].active){
  if(ene[i].type==0) circle((int)ene[i].x+sh,(int)ene[i].y+sh,12,0x0066FF66);
  if(ene[i].type==1) circle((int)ene[i].x+sh,(int)ene[i].y+sh,16,0x00A8D8F0);
  if(ene[i].type==2) circle((int)ene[i].x+sh,(int)ene[i].y+sh,14,0x00FF6A00);
 }
 // BOSS
 if(game_state==1||game_state==3){
  int bx=boss_x+sh, by=boss_y+sh;
  if(boss_type==0){
   // FLORONA
   for(int a=0;a<8;a++){float ang=a*0.785f; circle(bx+(int)(cosf(ang)*70),by+(int)(sinf(ang)*70),30,0x00FF8FAB);}
   circle(bx,by,55,0x00FFD23F); circle(bx-15,by-10,10,0x00000000); circle(bx+15,by-10,10,0x00000000);
   fill_rect(bx-8,by+20,16,60,0x0066AA33);
  }
  if(boss_type==1){
   circle(bx,by,75,0x0066CC44); circle(bx-30,by-60,28,0x0066CC44); circle(bx+30,by-60,28,0x0066CC44);
   circle(bx-30,by-60,10,0x00FFFFFF); circle(bx+30,by-60,10,0x00FFFFFF);
   circle(bx-30,by-60,5,0x00000000); circle(bx+30,by-60,5,0x00000000);
   fill_rect(bx-40,by+20,80,18,0x00FFFFFF);
  }
  if(boss_type==2){
   fill_rect(bx-90,by-20,180,70,0x00FF4D4D);
   circle(bx+80,by-10,35,0x00FF4D4D); circle(bx+90,by-15,8,0x00FFFFFF); circle(bx+90,by-15,4,0x00000000);
   fill_rect(bx-140,by+10,60,12,0x00FFD23F); fill_rect(bx-140,by-30,60,12,0x00FFD23F);
  }
 }
 // HUD
 char buf[64];
 draw_text("VIDAS",24,18,ui,0x00222222);
 for(int i=0;i<php;i++) circle(40+i*36,70,14,0x00FF4D4D);
 sprintf(buf,"SCORE %d",score); draw_text(buf,24,95,ui,0x00222222);
 if(game_state==1){
  draw_text("BOSS", (int)W-320,18,ui,0x00FF0000);
  int bw=260; fill_rect((int)W-320,45,bw,18,0x00222222);
  int fw=bw*boss_hp/boss_max; if(fw<0) fw=0; fill_rect((int)W-320,45,fw,18,0x00FF4D4D);
  if(stage==0) draw_text("FLORONA", (int)W-320,75,ui,0x00222222);
  if(stage==1) draw_text("SAPAO", (int)W-320,75,ui,0x00222222);
  if(stage==2) draw_text("DRAGAO", (int)W-320,75,ui,0x00222222);
 }
 if(game_state==2){
  draw_text("VOCE MORREU", (int)W/2-220, (int)H/2-60, 6, 0x00FF0000);
  draw_text("APERTE X P/ TENTAR DE NOVO", (int)W/2-260, (int)H/2+40, 3, 0x00222222);
 }
 if(game_state==3){
  draw_text("BOSS DERROTADO!", (int)W/2-280, (int)H/2-40, 6, 0x0066AA00);
 }
 if(game_state==4){
  draw_text("VOCE ZEROU!", (int)W/2-200, 200, 8, 0x00FFD23F);
  draw_text("COPO E O MELHOR!", (int)W/2-220, 320, 5, 0x00222222);
  sprintf(buf,"SCORE FINAL %d",score); draw_text(buf,(int)W/2-180,400,4,0x00222222);
  draw_text("X P/ JOGAR DE NOVO", (int)W/2-200, 480, 3, 0x00555555);
 }
}

int main(int argc,char**argv){(void)argc;(void)argv; init_screen(); ioPadInit(7); sysUtilRegisterCallback(0,sysutil_cb,NULL); srand(12345); px=120; py=(int)H-180; game_state=0; while(running){sysUtilCheckCallback(); read_pad(); update_game(); render(); present();} return 0;}