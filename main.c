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

/* ======================================================================
 *  AUDIO  (sintetizado em tempo real: efeitos + musica, sem arquivos)
 * ====================================================================== */
#define SR      48000.0f
#define ABLK    256          /* amostras por bloco (fixo no PS3) */
#define ANBLK   16           /* blocos no buffer (16 = ~85 ms) */
#define NVOICE  8

typedef struct {
    int on, wave, left, total;
    float f, df, ph, vol, nz;
} Voice;

static Voice vo[NVOICE];
static int audio_ok = 0, music_on = 1;
static u32 aport;
static audioPortConfig acfg;
static float *abase;
static u32 awr = ANBLK - 1;

static u32 rng_s = 2463534242u;
static float arand(void)
{
    rng_s ^= rng_s << 13; rng_s ^= rng_s >> 17; rng_s ^= rng_s << 5;
    return (float)(rng_s & 0xFFFF) / 65535.0f;
}

static float mtof(int n) { return 440.0f * powf(2.0f, (n - 69) / 12.0f); }

/* efeito: wave 0=quadrada 1=triangular 2=ruido; frequencia desliza de f0 a f1 */
static void sfx(int wave, float f0, float f1, float dur, float vol)
{
    if (!audio_ok) return;
    int best = 0, bl = 0x7FFFFFFF;
    for (int i = 0; i < NVOICE; i++) {
        int rem = vo[i].on ? vo[i].left : 0;
        if (rem < bl) { bl = rem; best = i; }
    }
    Voice *v = &vo[best];
    v->wave = wave;
    v->f = f0;
    v->total = v->left = (int)(dur * SR);
    if (v->total < 1) v->total = v->left = 1;
    v->df = (f1 - f0) / (float)v->total;
    v->ph = 0; v->vol = vol; v->nz = 0;
    v->on = 1;
}

static void sfx_eat(int k)   { sfx(0, 560.0f + (k % 6) * 70.0f, 1150.0f, 0.07f, 0.13f); }
static void sfx_boost(void)  { sfx(2, 4000.0f, 1500.0f, 0.15f, 0.10f); }
static void sfx_die(void)    { sfx(2, 7000.0f, 500.0f, 0.75f, 0.22f); sfx(1, 260.0f, 45.0f, 0.75f, 0.28f); }
static void sfx_boom(void)   { sfx(2, 5000.0f, 800.0f, 0.25f, 0.10f); }
static void sfx_menu(void)   { sfx(0, 880.0f, 880.0f, 0.04f, 0.10f); }
static void sfx_start(void)  { sfx(0, 400.0f, 1300.0f, 0.28f, 0.16f); sfx(1, 200.0f, 650.0f, 0.28f, 0.2f); }
static void sfx_error(void)  { sfx(0, 180.0f, 140.0f, 0.18f, 0.14f); }

/* musica: lead (quadrada) + baixo (triangular), 140 BPM, 32 colcheias */
static const u8 mel_lead[32] = {
    72, 0, 75, 0, 79, 0, 75, 0,   77, 0, 74, 0, 71, 0, 74, 0,
    75, 0, 72, 0, 68, 0, 72, 0,   74, 0, 71, 0, 67, 71, 74, 0
};
static const u8 mel_bass[4] = { 48, 43, 44, 43 };
#define STEP_SAMPLES ((int)(SR * 60.0f / 140.0f / 2.0f))

static int m_left = 1, m_step = -1;
static float lead_f = 0, lead_ph = 0, lead_env = 0, bass_f = 0, bass_ph = 0, bass_env = 0;

static float music_sample(void)
{
    if (--m_left <= 0) {
        m_left += STEP_SAMPLES;
        m_step = (m_step + 1) & 31;
        int ln = mel_lead[m_step];
        lead_f = ln ? mtof(ln) : 0.0f;
        lead_env = 1.0f;
        if ((m_step & 1) == 0) {
            bass_f = mtof(mel_bass[m_step >> 3] + (((m_step >> 1) & 1) ? 12 : 0));
            bass_env = 1.0f;
        }
    }
    if (!music_on) return 0.0f;
    lead_ph += lead_f / SR; if (lead_ph >= 1.0f) lead_ph -= 1.0f;
    bass_ph += bass_f / SR; if (bass_ph >= 1.0f) bass_ph -= 1.0f;
    lead_env *= 0.99985f;
    bass_env *= 0.99980f;
    float sq = (lead_ph < 0.25f) ? 1.0f : -1.0f;
    float tr = 4.0f * fabsf(bass_ph - 0.5f) - 1.0f;
    return (lead_f > 0 ? sq * lead_env * 0.05f : 0.0f) + tr * bass_env * 0.09f;
}

static float mix_sample(void)
{
    float s = music_sample();
    for (int i = 0; i < NVOICE; i++) {
        Voice *v = &vo[i];
        if (!v->on) continue;
        float env = (float)v->left / (float)v->total;
        float o;
        v->ph += v->f / SR;
        if (v->ph >= 1.0f) {
            v->ph -= 1.0f;
            if (v->wave == 2) v->nz = arand() * 2.0f - 1.0f;
        }
        if (v->wave == 0)      o = (v->ph < 0.5f) ? 1.0f : -1.0f;
        else if (v->wave == 1) o = 4.0f * fabsf(v->ph - 0.5f) - 1.0f;
        else                   o = v->nz;
        s += o * env * v->vol;
        v->f += v->df;
        if (v->f < 20.0f) v->f = 20.0f;
        if (--v->left <= 0) v->on = 0;
    }
    if (s > 0.9f) s = 0.9f;
    if (s < -0.9f) s = -0.9f;
    return s;
}

static void init_audio(void)
{
    audioPortParam p;
    memset(&p, 0, sizeof(p));
    p.numChannels = 2;       /* AUDIO_PORT_2CH */
    p.numBlocks = ANBLK;     /* AUDIO_BLOCK_16 */
    p.attrib = 0;
    p.level = 1.0f;
    if (audioInit() != 0) return;
    if (audioPortOpen(&p, &aport) != 0) { audioQuit(); return; }
    audioGetPortConfig(aport, &acfg);
    abase = (float *)(u64)acfg.audioDataStart;
    memset(abase, 0, ANBLK * ABLK * 2 * sizeof(float));
    audioPortStart(aport);
    audio_ok = 1;
}

static void audio_pump(void)
{
    if (!audio_ok) return;
    u32 N = ANBLK;
    u32 r = (u32)(*(volatile u64 *)(u64)acfg.readIndex) % N;
    u32 ahead = (awr + N - r) % N;
    if (ahead > N - 2) { awr = (r + 2) % N; ahead = 2; }   /* (re)sincroniza */
    while (ahead < N - 2) {
        float *out = abase + awr * ABLK * 2;
        for (int n = 0; n < ABLK; n++) {
            float s = mix_sample();
            out[n * 2] = s;
            out[n * 2 + 1] = s;
        }
        awr = (awr + 1) % N;
        ahead = (awr + N - r) % N;
    }
}

static void quit_audio(void)
{
    if (!audio_ok) return;
    audio_ok = 0;
    audioPortStop(aport);
    audioPortClose(aport);
    audioQuit();
}

/* ======================================================================
 *  VIDEO
 * ====================================================================== */
static gcmContextData *ctx;
static u32 W, H;
static u32 *fb[2];
static u32 fboff[2];
static u32 *back;
static int cur = 0, first_flip = 1;
static volatile int running = 1;
static int clx0 = 0, clx1 = 0;   /* area de recorte horizontal (tela dividida) */

static void sysutil_cb(u64 status, u64 param, void *usrdata)
{
    (void)param; (void)usrdata;
    if (status == SYSUTIL_EXIT_GAME) running = 0;
}

static void init_screen(void)
{
    void *host = memalign(1024 * 1024, HOST_SIZE);
    rsxInit(&ctx, CB_SIZE, HOST_SIZE, host);

    videoState st;
    videoGetState(VIDEO_PRIMARY, 0, &st);
    u32 resid = st.displayMode.resolution;
    if (videoGetResolutionAvailability(VIDEO_PRIMARY, VIDEO_RESOLUTION_720, VIDEO_ASPECT_AUTO, 0))
        resid = VIDEO_RESOLUTION_720;

    videoResolution res;
    videoGetResolution(resid, &res);
    W = res.width;
    H = res.height;

    videoConfiguration vc;
    memset(&vc, 0, sizeof(vc));
    vc.resolution = resid;
    vc.format = VIDEO_BUFFER_FORMAT_XRGB;
    vc.aspect = VIDEO_ASPECT_AUTO;
    vc.pitch = W * 4;
    videoConfigure(VIDEO_PRIMARY, &vc, NULL, 0);
    videoGetState(VIDEO_PRIMARY, 0, &st);

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    for (int i = 0; i < 2; i++) {
        fb[i] = (u32 *)rsxMemalign(64, W * H * 4);
        gcmAddressToOffset(fb[i], &fboff[i]);
        gcmSetDisplayBuffer(i, fboff[i], W * 4, W, H);
    }
    back = (u32 *)memalign(64, W * H * 4);
    memset(back, 0, W * H * 4);
    gcmResetFlipStatus();
    clx0 = 0; clx1 = (int)W;
}

static void wait_flip(void)
{
    while (gcmGetFlipStatus() != 0) {
        audio_pump();          /* mantem o audio alimentado enquanto espera o vsync */
        usleep(200);
    }
    gcmResetFlipStatus();
}

static void present(void)
{
    if (!first_flip) wait_flip();
    else gcmResetFlipStatus();
    memcpy(fb[cur], back, W * H * 4);
    gcmSetFlip(ctx, cur);
    rsxFlushBuffer(ctx);
    gcmSetWaitFlip(ctx);
    cur ^= 1;
    first_flip = 0;
}

/* ---------- desenho (com recorte horizontal) ---------- */
static void fill_rect(int x, int y, int w, int h, u32 c)
{
    int x0 = x < clx0 ? clx0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w, y1 = y + h;
    if (x1 > clx1) x1 = clx1;
    if (y1 > (int)H) y1 = H;
    for (int yy = y0; yy < y1; yy++) {
        u32 *p = back + yy * W;
        for (int xx = x0; xx < x1; xx++) p[xx] = c;
    }
}

static void circle(int cx, int cy, int r, u32 c)
{
    if (r < 1) r = 1;
    if (cx + r < clx0 || cx - r >= clx1 || cy + r < 0 || cy - r >= (int)H) return;
    for (int dy = -r; dy <= r; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= (int)H) continue;
        int hw = (int)sqrtf((float)(r * r - dy * dy));
        int x0 = cx - hw, x1 = cx + hw;
        if (x0 < clx0) x0 = clx0;
        if (x1 >= clx1) x1 = clx1 - 1;
        u32 *p = back + y * W;
        for (int x = x0; x <= x1; x++) p[x] = c;
    }
}

/* fonte 3x5 minima */
typedef struct { char c; const char *p; } Glyph;
static const Glyph glyphs[] = {
    {'0', "###" "#.#" "#.#" "#.#" "###"}, {'1', ".#." "##." ".#." ".#." "###"},
    {'2', "###" "..#" "###" "#.." "###"}, {'3', "###" "..#" "###" "..#" "###"},
    {'4', "#.#" "#.#" "###" "..#" "..#"}, {'5', "###" "#.." "###" "..#" "###"},
    {'6', "###" "#.." "###" "#.#" "###"}, {'7', "###" "..#" "..#" "..#" "..#"},
    {'8', "###" "#.#" "###" "#.#" "###"}, {'9', "###" "#.#" "###" "..#" "###"},
    {'A', ".#." "#.#" "###" "#.#" "#.#"}, {'B', "##." "#.#" "##." "#.#" "##."},
    {'C', "###" "#.." "#.." "#.." "###"}, {'D', "##." "#.#" "#.#" "#.#" "##."},
    {'E', "###" "#.." "###" "#.." "###"}, {'F', "###" "#.." "##." "#.." "#.."},
    {'G', "###" "#.." "#.#" "#.#" "###"}, {'H', "#.#" "#.#" "###" "#.#" "#.#"},
    {'I', "###" ".#." ".#." ".#." "###"}, {'J', "..#" "..#" "..#" "#.#" "###"},
    {'K', "#.#" "#.#" "##." "#.#" "#.#"}, {'L', "#.." "#.." "#.." "#.." "###"},
    {'M', "#.#" "###" "###" "#.#" "#.#"}, {'N', "###" "#.#" "#.#" "#.#" "#.#"},
    {'O', "###" "#.#" "#.#" "#.#" "###"}, {'P', "###" "#.#" "###" "#.." "#.."},
    {'Q', "###" "#.#" "#.#" "###" "..#"}, {'R', "##." "#.#" "##." "#.#" "#.#"},
    {'S', "###" "#.." "###" "..#" "###"}, {'T', "###" ".#." ".#." ".#." ".#."},
    {'U', "#.#" "#.#" "#.#" "#.#" "###"}, {'V', "#.#" "#.#" "#.#" "#.#" ".#."},
    {'W', "#.#" "#.#" "###" "###" "#.#"}, {'X', "#.#" "#.#" ".#." "#.#" "#.#"},
    {'Y', "#.#" "#.#" ".#." ".#." ".#."}, {'Z', "###" "..#" ".#." "#.." "###"},
    {'>', "#.." ".#." "..#" ".#." "#.."}, {'-', "..." "..." "###" "..." "..."},
    {':', "..." ".#." "..." ".#." "..."}, {'!', ".#." ".#." ".#." "..." ".#."},
};

static const char *find_glyph(char c)
{
    c = (char)toupper((unsigned char)c);
    for (unsigned i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++)
        if (glyphs[i].c == c) return glyphs[i].p;
    return NULL;
}

static int text_width(const char *s, int sc) { return (int)strlen(s) * 4 * sc - sc; }

static void draw_text(const char *s, int x, int y, int sc, u32 c)
{
    for (; *s; s++) {
        const char *g = find_glyph(*s);
        if (g)
            for (int r = 0; r < 5; r++)
                for (int k = 0; k < 3; k++)
                    if (g[r * 3 + k] == '#') fill_rect(x + k * sc, y + r * sc, sc, sc, c);
        x += 4 * sc;
    }
}

static void draw_text_c(const char *s, int cx, int y, int sc, u32 c)
{
    draw_text(s, cx - text_width(s, sc) / 2, y, sc, c);
}

/* ======================================================================
 *  CONTROLES
 * ====================================================================== */
typedef struct {
    int on;                      /* controle conectado */
    float ax, ay, mag;           /* direcao desejada */
    int boost, cross, start, select, tri, up, down;
} PadIn;

static PadIn pin[MAXP], pprev[MAXP];
static int npads = 0;
#define PRESS(p, f) (pin[p].f && !pprev[p].f)

/* Jogador 1 = primeiro controle conectado (em qualquer porta),
   Jogador 2 = segundo controle conectado. */
static void read_pads(void)
{
    padInfo info;
    padData d;
    int n = 0;

    memset(pin, 0, sizeof(pin));
    ioPadGetInfo(&info);

    for (int i = 0; i < NPADS; i++) {
        if (!info.status[i]) continue;
        if (n < MAXP) {
            memset(&d, 0, sizeof(d));
            ioPadGetData(i, &d);
            PadIn *p = &pin[n];
            p->on = 1;

            float ax = 0, ay = 0;
            /* se todos os eixos vierem 0 o dado ainda nao e valido: ignora o analogico */
            if (d.ANA_L_H || d.ANA_L_V || d.ANA_R_H || d.ANA_R_V) {
                ax = ((int)d.ANA_L_H - 128) / 128.0f;
                ay = ((int)d.ANA_L_V - 128) / 128.0f;
                if (sqrtf(ax * ax + ay * ay) < 0.25f) ax = ay = 0;   /* zona morta */
            }
            /* direcional tem prioridade e aceita diagonal */
            float dx = (d.BTN_RIGHT ? 1.0f : 0.0f) - (d.BTN_LEFT ? 1.0f : 0.0f);
            float dy = (d.BTN_DOWN ? 1.0f : 0.0f) - (d.BTN_UP ? 1.0f : 0.0f);
            if (dx != 0 || dy != 0) { ax = dx; ay = dy; }

            p->ax = ax; p->ay = ay;
            p->mag = sqrtf(ax * ax + ay * ay);
            p->boost = d.BTN_CROSS || d.BTN_R1 || d.BTN_R2 || d.BTN_L1 || d.BTN_L2;
            p->cross = d.BTN_CROSS;
            p->start = d.BTN_START;
            p->select = d.BTN_SELECT;
            p->tri = d.BTN_TRIANGLE;
            p->up = d.BTN_UP || ay < -0.6f;
            p->down = d.BTN_DOWN || ay > 0.6f;
        }
        n++;
    }
    npads = n;
}

/* ======================================================================
 *  JOGO
 * ====================================================================== */
enum { ST_MENU, ST_PLAY, ST_PAUSE };
static int state = ST_MENU, menu_sel = 0, nplayers = 1, mt = 0;

static Snake sn[NSN];
static Pellet pel[NPEL];
static int pelrot = 0;
static int dead_t[MAXP];
static float cam_x[MAXP], cam_y[MAXP];

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

static float radius_of(const Snake *s)
{
    int l = s->len > 240 ? 240 : s->len;
    return 9.0f + l * 0.035f;
}

static void add_pellet(float x, float y, int size)
{
    for (int k = 0; k < NPEL; k++) {
        int i = (pelrot + k) % NPEL;
        if (!pel[i].active) {
            pel[i].x = x; pel[i].y = y; pel[i].size = size; pel[i].active = 1;
            pel[i].col = palette[rand() % 8];
            pelrot = (i + 1) % NPEL;
            return;
        }
    }
}

static void spawn_snake(int i)
{
    Snake *s = &sn[i];
    float x = 400 + frand() * (WORLD - 800), y = 400 + frand() * (WORLD - 800);
    s->ang = frand() * 2 * PI;
    s->tgt = s->ang;
    s->len = 10;
    for (int j = 0; j < s->len; j++) {
        s->x[j] = x - cosf(s->ang) * SP * j;
        s->y[j] = y - sinf(s->ang) * SP * j;
    }
    s->alive = 1; s->respawn = 0; s->boost = 0; s->grow = 0; s->tick = 0;
    s->col = palette[i % 8];
    if (i < MAXP) { cam_x[i] = x; cam_y[i] = y; }
}

static void kill_snake(int i)
{
    Snake *s = &sn[i];
    for (int j = 0; j < s->len; j += 2)
        add_pellet(s->x[j] + (frand() - 0.5f) * 10, s->y[j] + (frand() - 0.5f) * 10, 2);
    s->alive = 0;
    s->respawn = 240;
    if (i < nplayers) {
        dead_t[i] = 0;
        sfx_die();
    } else {
        /* explosao baixinha se o bot morreu perto de um jogador */
        for (int p = 0; p < nplayers; p++) {
            if (!sn[p].alive) continue;
            float dx = s->x[0] - sn[p].x[0], dy = s->y[0] - sn[p].y[0];
            if (dx * dx + dy * dy < 600.0f * 600.0f) { sfx_boom(); break; }
        }
    }
}

static void bot_think(int i)
{
    Snake *s = &sn[i];
    float hx = s->x[0], hy = s->y[0];
    s->tgt += (frand() - 0.5f) * 0.25f;

    if (s->tick % 6 == 0) {
        float best = 380.0f * 380.0f;
        int bi = -1;
        for (int k = 0; k < NPEL; k++) {
            if (!pel[k].active) continue;
            float dx = pel[k].x - hx, dy = pel[k].y - hy, d = dx * dx + dy * dy;
            if (d < best) { best = d; bi = k; }
        }
        if (bi >= 0) s->tgt = atan2f(pel[bi].y - hy, pel[bi].x - hx);
    }

    if (hx < 250 || hx > WORLD - 250 || hy < 250 || hy > WORLD - 250)
        s->tgt = atan2f(WORLD / 2 - hy, WORLD / 2 - hx);

    float px = hx + cosf(s->ang) * 55, py = hy + sinf(s->ang) * 55;
    float rs = radius_of(s);
    for (int o = 0; o < NSN; o++) {
        if (o == i || !sn[o].alive) continue;
        float lim = rs + radius_of(&sn[o]) + 25;
        lim *= lim;
        for (int j = 0; j < sn[o].len; j += 2) {
            float dx = sn[o].x[j] - px, dy = sn[o].y[j] - py;
            if (dx * dx + dy * dy < lim) { s->tgt = s->ang + 1.6f; return; }
        }
    }
}

static void update_snake(int i)
{
    Snake *s = &sn[i];
    if (!s->alive) {
        if (i >= nplayers && --s->respawn <= 0) spawn_snake(i);
        return;
    }
    s->tick++;
    if (i < nplayers) {
        if (pin[i].mag > 0.3f) s->tgt = atan2f(pin[i].ay, pin[i].ax);
        if (pin[i].boost && !s->boost && s->len > 12) sfx_boost();
        s->boost = pin[i].boost;
    } else {
        bot_think(i);
        s->boost = (s->len > 30 && (s->tick / 90) % 5 == 0) ? 1 : 0;
    }

    float d = s->tgt - s->ang;
    while (d > PI) d -= 2 * PI;
    while (d < -PI) d += 2 * PI;
    if (d > 0.075f) d = 0.075f;
    if (d < -0.075f) d = -0.075f;
    s->ang += d;

    float sp = 3.0f;
    if (s->boost && s->len > 12) {
        sp = 5.4f;
        if (s->tick % 10 == 0) {
            s->len--;
            add_pellet(s->x[s->len], s->y[s->len], 1);
        }
    }
    s->x[0] += cosf(s->ang) * sp;
    s->y[0] += sinf(s->ang) * sp;
    for (int j = 1; j < s->len; j++) {
        float dx = s->x[j - 1] - s->x[j], dy = s->y[j - 1] - s->y[j];
        float dd = sqrtf(dx * dx + dy * dy);
        if (dd > SP) {
            float f = (dd - SP) / dd;
            s->x[j] += dx * f;
            s->y[j] += dy * f;
        }
    }

    if (s->x[0] < 0 || s->x[0] > WORLD || s->y[0] < 0 || s->y[0] > WORLD) { kill_snake(i); return; }

    float rs = radius_of(s), er = rs + 12;
    int ate = 0;
    for (int k = 0; k < NPEL; k++) {
        if (!pel[k].active) continue;
        float dx = pel[k].x - s->x[0], dy = pel[k].y - s->y[0];
        if (dx * dx + dy * dy < er * er) {
            pel[k].active = 0;
            s->grow += pel[k].size;
            s->score += pel[k].size * 5;
            ate++;
        }
    }
    if (ate && i < nplayers) sfx_eat(s->score / 5);
    while (s->grow >= 2 && s->len < MAXSEG) {
        s->x[s->len] = s->x[s->len - 1];
        s->y[s->len] = s->y[s->len - 1];
        s->len++;
        s->grow -= 2;
    }
    if (s->len >= MAXSEG) s->grow = 0;

    for (int o = 0; o < NSN; o++) {
        if (o == i || !sn[o].alive) continue;
        float lim = (rs + radius_of(&sn[o])) * 0.75f;
        lim *= lim;
        for (int j = 0; j < sn[o].len; j++) {
            float dx = sn[o].x[j] - s->x[0], dy = sn[o].y[j] - s->y[0];
            if (dx * dx + dy * dy < lim) { kill_snake(i); return; }
        }
    }
}

static void init_game(void)
{
    memset(pel, 0, sizeof(pel));
    for (int k = 0; k < NPEL; k++) add_pellet(frand() * WORLD, frand() * WORLD, 1 + (rand() % 5 == 0));
    for (int i = 0; i < NSN; i++) { sn[i].score = 0; spawn_snake(i); }
    for (int p = 0; p < MAXP; p++) dead_t[p] = 0;