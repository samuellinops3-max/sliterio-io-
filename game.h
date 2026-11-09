/* JOGO */
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
}

static void update_game(void)
{
    for (int i = 0; i < NSN; i++) update_snake(i);

    int count = 0;
    for (int k = 0; k < NPEL; k++) count += pel[k].active;
    for (int n = 0; n < 2 && count < NPEL - 40; n++, count++)
        add_pellet(frand() * WORLD, frand() * WORLD, 1 + (rand() % 5 == 0));

    for (int p = 0; p < nplayers; p++) {
        if (sn[p].alive) {
            cam_x[p] = sn[p].x[0];
            cam_y[p] = sn[p].y[0];
        } else {
            dead_t[p]++;
            if (dead_t[p] > 60 && PRESS(p, cross)) {
                spawn_snake(p);
                sn[p].score = 0;
                sfx_start();
            }
        }
    }
}

static void update_menu(void)
{
    if ((PRESS(0, up) || PRESS(1, up)) && menu_sel != 0)     { menu_sel = 0; sfx_menu(); }
    if ((PRESS(0, down) || PRESS(1, down)) && menu_sel != 1) { menu_sel = 1; sfx_menu(); }

    if (PRESS(0, cross) || PRESS(0, start) || PRESS(1, cross) || PRESS(1, start)) {
        if (menu_sel == 1 && npads < 2) { sfx_error(); return; }   /* precisa de 2 controles */
        nplayers = menu_sel + 1;
        init_game();
        state = ST_PLAY;
        sfx_start();
    }
}