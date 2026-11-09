/* ---------- render ---------- */
static void render_view(int p, int vx, int vw)
{
    float sc = (float)H / 720.0f * (nplayers == 2 ? 0.85f : 1.0f);
    float hw = vx + vw / 2.0f, hh = H / 2.0f;
    float camx = cam_x[p], camy = cam_y[p];
    int ui = (int)(4.0f * H / 720.0f + 0.5f);
    if (ui < 3) ui = 3;

    clx0 = vx; clx1 = vx + vw;
    fill_rect(vx, 0, vw, H, 0x000E1420);

    /* fora da arena */
    int sx0 = (int)((0 - camx) * sc + hw), sx1 = (int)((WORLD - camx) * sc + hw);
    int sy0 = (int)((0 - camy) * sc + hh), sy1 = (int)((WORLD - camy) * sc + hh);
    u32 red = 0x00380E12;
    if (sx0 > 0) fill_rect(0, 0, sx0, H, red);
    if (sx1 < (int)W) fill_rect(sx1, 0, W - sx1, H, red);
    if (sy0 > 0) fill_rect(0, 0, W, sy0, red);
    if (sy1 < (int)H) fill_rect(0, sy1, W, H - sy1, red);

    /* pontos de fundo para dar sensacao de movimento */
    float gx0 = floorf((camx - (vw / 2.0f) / sc) / 100.0f) * 100.0f;
    float gy0 = floorf((camy - hh / sc) / 100.0f) * 100.0f;
    for (float gx = gx0; gx < camx + (vw / 2.0f) / sc; gx += 100.0f) {
        if (gx < 0 || gx > WORLD) continue;
        for (float gy = gy0; gy < camy + hh / sc; gy += 100.0f) {
            if (gy < 0 || gy > WORLD) continue;
            fill_rect((int)((gx - camx) * sc + hw), (int)((gy - camy) * sc + hh), 4, 4, 0x00203047);
        }
    }

    for (int k = 0; k < NPEL; k++) {
        if (!pel[k].active) continue;
        int px = (int)((pel[k].x - camx) * sc + hw), py = (int)((pel[k].y - camy) * sc + hh);
        circle(px, py, (int)((4 + pel[k].size * 2) * sc), pel[k].col);
    }

    for (int i = NSN - 1; i >= 0; i--) {
        Snake *s = &sn[i];
        if (!s->alive) continue;
        int r = (int)(radius_of(s) * sc);
        u32 dark = (s->col >> 1) & 0x007F7F7F;
        /* desenha de 2 em 2 segmentos (mais rapido, visual igual) */
        int start = (s->len - 1) & ~1;
        for (int j = start; j >= 0; j -= 2) {
            int px = (int)((s->x[j] - camx) * sc + hw), py = (int)((s->y[j] - camy) * sc + hh);
            circle(px, py, r, (j % 6 < 3) ? s->col : dark);
        }
        int hx = (int)((s->x[0] - camx) * sc + hw), hy = (int)((s->y[0] - camy) * sc + hh);
        for (int e = -1; e <= 1; e += 2) {
            int ex = hx + (int)(cosf(s->ang + e * 0.7f) * r * 0.6f);
            int ey = hy + (int)(sinf(s->ang + e * 0.7f) * r * 0.6f);
            circle(ex, ey, r * 35 / 100 + 1, 0x00FFFFFF);
            circle(ex + (int)(cosf(s->ang) * r * 0.15f), ey + (int)(sinf(s->ang) * r * 0.15f), r * 18 / 100 + 1, 0x00000000);
        }
    }

    /* HUD */
    char buf[32];
    draw_text("PONTOS", vx + 24, 20, ui, 0x00FFFFFF);
    if (nplayers == 2) {
        sprintf(buf, "J%d", p + 1);
        draw_text(buf, vx + 24 + text_width("PONTOS ", ui) + ui, 20, ui, sn[p].col);
    }
    sprintf(buf, "%d", sn[p].score);
    draw_text(buf, vx + 24, 20 + 7 * ui, ui + 2, 0x00FFD23F);
    if (nplayers == 1)
        draw_text("X TURBO   START PAUSA   TRIANGULO MUSICA", vx + 24, (int)H - 8 * ui, ui, 0x00809BBF);
    else
        draw_text("X TURBO", vx + 24, (int)H - 8 * ui, ui, 0x00809BBF);

    if (!sn[p].alive) {
        int cx = vx + vw / 2;
        int bw = text_width("VOCE MORREU", ui * 2) + 80, bh = 16 * ui + 40;
        fill_rect(cx - bw / 2, (int)H / 2 - bh / 2, bw, bh, 0x00000000);
        draw_text_c("VOCE MORREU", cx, (int)H / 2 - bh / 2 + 24, ui * 2, 0x00FF4D4D);
        if (dead_t[p] > 60)
            draw_text_c("APERTE X", cx, (int)H / 2 + ui * 3, ui, 0x00FFFFFF);
    }
}

static void render_game(void)
{
    if (nplayers == 1) {
        render_view(0, 0, (int)W);
    } else {
        int half = (int)W / 2;
        render_view(0, 0, half);
        render_view(1, half, (int)W - half);
        clx0 = 0; clx1 = (int)W;
        fill_rect(half - 2, 0, 4, H, 0x00000000);
    }
    clx0 = 0; clx1 = (int)W;
}

static void render_menu(void)
{
    int ui = (int)(4.0f * H / 720.0f + 0.5f);
    if (ui < 3) ui = 3;
    int cx = (int)W / 2;
    mt++;
    clx0 = 0; clx1 = (int)W;
    fill_rect(0, 0, W, H, 0x000E1420);

    /* cobrinha decorativa */
    for (int i = 0; i < 28; i++) {
        float t = mt * 0.05f - i * 0.28f;
        int px = cx - 330 + i * 24;
        int py = (int)(H * 0.40f + sinf(t) * 28.0f);
        circle(px, py, 16, (i % 6 < 3) ? 0x0066FF66 : 0x00338A33);
        if (i == 27) {
            circle(px + 6, py - 7, 6, 0x00FFFFFF); circle(px + 8, py - 7, 3, 0x00000000);
            circle(px + 6, py + 7, 6, 0x00FFFFFF); circle(px + 8, py + 7, 3, 0x00000000);
        }
    }

    draw_text_c("COBRA ARENA", cx, (int)(H * 0.12f), ui * 3, 0x0066FF66);

    int y0 = (int)(H * 0.55f);
    int ox = cx - 190;
    draw_text(menu_sel == 0 ? "> 1 JOGADOR" : "  1 JOGADOR", ox, y0, ui * 2,
              menu_sel == 0 ? 0x00FFD23F : 0x00809BBF);
    draw_text(menu_sel == 1 ? "> 2 JOGADORES" : "  2 JOGADORES", ox, y0 + 12 * ui, ui * 2,
              menu_sel == 1 ? 0x00FFD23F : 0x00809BBF);

    if (menu_sel == 1 && npads < 2)
        draw_text_c("CONECTE O SEGUNDO CONTROLE", cx, y0 + 24 * ui, ui + 1, 0x00FF4D4D);

    draw_text_c("CIMA BAIXO ESCOLHE   X CONFIRMA", cx, (int)H - 12 * ui, ui, 0x00809BBF);
    draw_text_c(music_on ? "TRIANGULO: MUSICA LIGADA" : "TRIANGULO: MUSICA DESLIGADA",
                cx, (int)H - 6 * ui, ui, 0x00809BBF);
}

static void render_pause(void)
{
    int ui = (int)(4.0f * H / 720.0f + 0.5f);
    if (ui < 3) ui = 3;
    int cx = (int)W / 2;
    int bw = 520, bh = 20 * ui + 40;
    fill_rect(cx - bw / 2, (int)H / 2 - bh / 2, bw, bh, 0x00000000);
    draw_text_c("PAUSADO", cx, (int)H / 2 - bh / 2 + 20, ui * 2, 0x00FFD23F);
    draw_text_c("START CONTINUA", cx, (int)H / 2 + ui * 1, ui, 0x00FFFFFF);
    draw_text_c("SELECT MENU", cx, (int)H / 2 + ui * 8, ui, 0x00FFFFFF);
}