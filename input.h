/* CONTROLES */
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