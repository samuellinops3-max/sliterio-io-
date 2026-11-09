/* AUDIO (sintetizado em tempo real: efeitos + musica, sem arquivos) */
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