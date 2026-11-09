// Cobra Arena - jogo estilo slither.io, offline, nativo para PS3 (PSL1GHT)
// 1 ou 2 jogadores (tela dividida), com som e musica.
// Controles: analogico esquerdo ou direcional = direcao
//            X / R1 / R2 / L1 / L2 = turbo
//            START = pausa | SELECT (na pausa) = menu | TRIANGULO = liga/desliga musica
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#include <audio/audio.h>

#define CB_SIZE   (0x100000)
#define HOST_SIZE (32 * 1024 * 1024)

#define WORLD  3200.0f
#define MAXSEG 420
#define NSN    10
#define NPEL   500
#define SP     7.0f
#define PI     3.14159265f
#define MAXP   2      /* jogadores */
#define NPADS  7      /* portas de controle verificadas */

typedef struct {
    float x[MAXSEG], y[MAXSEG];
    int len, alive, respawn, boost, grow, score, tick;
    float ang, tgt;
    u32 col;
} Snake;

typedef struct {
    float x, y;
    int size, active;
    u32 col;
} Pellet;

static const u32 palette[8] = {
    0x00FF4D4D, 0x0055D6FF, 0x0066FF66, 0x00FFD23F,
    0x00C77DFF, 0x00FF8FAB, 0x0000E5C0, 0x00FF9F43
};

#include "audio.h"
#include "video.h"
#include "input.h"
#include "game.h"
#include "render.h"

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    init_screen();
    ioPadInit(NPADS);
    init_audio();
    sysUtilRegisterCallback(0, sysutil_cb, NULL);
    srand(20261109);
    init_game();
    memset(pprev, 0, sizeof(pprev));

    while (running) {
        sysUtilCheckCallback();
        read_pads();

        if (PRESS(0, tri) || PRESS(1, tri)) music_on ^= 1;

        if (state == ST_MENU) {
            update_menu();
        } else if (state == ST_PLAY) {
            if (PRESS(0, start) || PRESS(1, start)) { state = ST_PAUSE; sfx_menu(); }
            else update_game();
        } else { /* ST_PAUSE */
            if (PRESS(0, start) || PRESS(1, start)) { state = ST_PLAY; sfx_menu(); }
            else if (PRESS(0, select) || PRESS(1, select)) { state = ST_MENU; sfx_menu(); }
        }

        if (state == ST_MENU) render_menu();
        else {
            render_game();
            if (state == ST_PAUSE) render_pause();
        }

        audio_pump();
        present();
        memcpy(pprev, pin, sizeof(pin));
    }

    quit_audio();
    ioPadEnd();
    return 0;
}