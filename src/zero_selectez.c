/*
 * zero_selectez.c — CSelectEz do Pump It Up Zero (piu, proc "SELECTEZ"): a seleção
 * de músicas da EASY STATION.
 *
 *   0x8075270 Begin   BGA/SELECTEZ.DAT [+0x58] e BGA/SKINEZ.DAT [+0x5c]; dígitos =
 *                     slots 0x1e..0x27 do SKINEZ; banners do BGA/91E.DAT (91H com o
 *                     idioma 0); fundo BGA/BA%02d.MOV (estágio + 1) em laço; prévia
 *                     AUDIO/D%X.AUD da música do cursor; EFF_SELECT_START.
 *                     No 1º estágio: skin 0 e velocidade 1.
 *   0x8075000         banners: "%XS1.TGA", "%XS2.TGA" e "%XTV.TGA" de cada música.
 *   0x8076ac0         lista: os 20 registros de 0x0811cb40 (id, nível 1..3); a C90
 *                     (tutorial) só entra no 1º estágio.
 *   0x8075750 Run     fundo, banners (0x8076360), dígitos (0x80769d0), cenas "1P" /
 *                     "2P", "START", "TIME_BEGIN"; com "START" >= 90: "LEFT STOP",
 *                     "RIGHT STOP", "LEFT"/"RIGHT" ou "selection stop" + "TV STOP",
 *                     "CENTER STOP", "CENTER", "SPEED Xn", "SKINn", "LVn", "TIME",
 *                     entrada dos dois jogadores; "READY"; prévia 30 quadros depois
 *                     de parar; tempo < 1 -> EFF_START_EZ e "RUN %X -p".
 *   0x8076420 entrada UL velocidade 1 -> 2 -> 3 -> 1 (EFF_SPEED); UR skin (EFF_SKIN);
 *                     C pronto (EFF_SELECT) e, de novo, começa; DL próxima música,
 *                     DR anterior (EFF_MOVE); quem não entrou: C entra (0x8076810).
 *
 * "-p" = chart PRACTICE (seção 0 do .STX).
 *
 * Ainda não feito: o tutorial ("TUTO C90 -p"; a C90 fica fora da lista), o vídeo de
 * introdução (BGA/EZ_INTRO_?.MOV) e as telas próprias da estação (EZ_NST, EZ_GAMEOVER,
 * GRADEEZ, EZ_GRADU): depois da música o fluxo segue pelas telas do ARCADE.
 */
#include "pumpy.h"
#include "bga.h"
#include "movie.h"

enum { EB_SELECT = 0, EB_SKIN = 1 };

typedef struct { int id; int level; } EzSong;
/* 0x0811cb40 (sem a C90, ver cabeçalho) */
static const EzSong k_ezSongs[] = {
    { 0x103, 3 }, { 0x112, 3 }, { 0x204, 2 }, { 0x303, 3 }, { 0x308, 1 }, { 0x405, 3 },
    { 0x505, 3 }, { 0x708, 1 }, { 0x809, 2 }, { 0x818, 2 }, { 0x906, 2 }, { 0xA10, 1 },
    { 0xB01, 1 }, { 0xB14, 1 }, { 0xB19, 3 }, { 0xC09, 2 }, { 0xC17, 1 }, { 0xC12, 2 },
    { 0xC22, 3 },
};
#define EZ_COUNT ((int)(sizeof(k_ezSongs) / sizeof(k_ezSongs[0])))

int ZeroEz_SongCount(void) { return EZ_COUNT; }
int ZeroEz_SongId(int i) { return (i >= 0 && i < EZ_COUNT) ? k_ezSongs[i].id : 0; }
int ZeroEz_SongLevel(int i) { return (i >= 0 && i < EZ_COUNT) ? k_ezSongs[i].level : 0; }

static const int k_slotS1[10] = { 4, 6, 10, 12, 14, 16, 15, 13, 11, 7 };          /* 0x0811cd60 */
static const int k_slotS2[10] = { 22, 24, 26, 28, 30, 32, 31, 29, 27, 25 };       /* 0x0811cda0 */
static const char* const k_skinScene[5]  = { "SKIN1", "SKIN2", "SKIN3", "SKIN4", "SKIN5" };   /* 0x0811cd20 */
static const char* const k_speedScene[4] = { " ", "SPEED X1", "SPEED X2", "SPEED X3" };      /* 0x0811cd34 */
static const char* const k_levelScene[4] = { " ", "LV1", "LV2", "LV3" };                     /* 0x0811cd44 */

enum { EZS_MOVE, EZS_SKIN, EZS_SPEED, EZS_SELECT, EZS_JOIN, EZS_START, EZS_TIME, EZS_COUNT };
/* SCRIPT/UI/EZ/SFX_SELECT.LUA e SFX_GLOBAL.LUA */
static const char* const k_snd[EZS_COUNT] = {
    "EZ_MOVE.WAV", "EZ_SKIN.WAV", "EZ_SPEED.WAV", "EZ_SELECT.WAV", "EZ_JOIN.WAV",
    "EZ_START.WAV", "TIME_LIMIT.WAV",
};
static int s_snd[EZS_COUNT] = { -1, -1, -1, -1, -1, -1, -1 };
static void sfx(int k) { if (s_snd[k] >= 0) Audio_Play(s_snd[k], false); }

static bool s_active;                 /* a Station escolheu EASY */
static int  s_tex[EZ_COUNT][3];       /* S1, S2, TV */
static BGALayerSrc s_srcS1, s_srcS2, s_srcTV;   /* moldes: o objeto original de cada slot */
static bool s_srcOk;
static BGALayerSrc s_digit[10];       /* +0x60.. */
static bool s_digitOk;
static int  s_cursor;                 /* +0x14 */
static int  s_skin;                   /* +0xb8: 0..4 */
static int  s_speed = 1;              /* +0xbc: 1..3 */
static bool s_previewOn;              /* +0x94 */
static bool s_ready;                  /* +0x95 */
static int  s_move;                   /* +0x96: 0 parado, 1 LEFT, 2 RIGHT */
static float s_idle;                  /* +0x98 */
static int  s_time, s_timePrev;       /* +0xa4 / +0xa8 */
static int  s_timeTotal;              /* +0xac: quadros da cena "TIME" */
static uint32_t s_timeStart;          /* +0xa0: relógio, parte com "START" no quadro 90 */
static bool s_timeRunning;
static unsigned s_joined;
static bool s_started;

void ZeroEz_SetActive(bool on) { s_active = on; }
bool ZeroEz_Active(void) { return s_active; }

static int wrap(int i) {              /* 0x8076950 */
    while (i < 0) i += EZ_COUNT;
    while (i >= EZ_COUNT) i -= EZ_COUNT;
    return i;
}

static void setSlotTex(int slot, const BGALayerSrc* base, int tex) {
    if (!s_srcOk || tex < 0) return;
    BGALayerSrc s = *base;
    s.texId = tex;
    BGA_SetLayerSrc(EB_SELECT, slot, &s);
}

/* 0x806a2f0 (parte do áudio): AUDIO/D%X.AUD, com a base como reserva */
static void playPreview(int id) {
    char aud[MAX_PATH];
    Song_FindFile(id, "%s/AUDIO/D%s.AUD", false, aud, sizeof(aud));
    BGM_Stop();
    if (BGM_LoadAUDDirect(aud)) BGM_Play(false);
}

void ZeroEz_Enter(void) {
    Title_StopMusic();
    Movie_Close();
    BGM_Stop();
    Resource_ClearBGA();
    if (!Resource_LoadBGAByName("SELECTEZ")) Log_Print("ZSELECTEZ: falha ao carregar BGA\\SELECTEZ.DAT\n");
    if (!Resource_LoadBGAByName("SKINEZ"))   Log_Print("ZSELECTEZ: falha ao carregar BGA\\SKINEZ.DAT\n");
    BGA_Reset();

    s_digitOk = true;
    for (int d = 0; d < 10; d++) s_digitOk &= BGA_GetLayerSrc(EB_SKIN, 0x1e + d, &s_digit[d]);
    if (!s_digitOk) Log_Print("ZSELECTEZ: slots de dígito do SKINEZ incompletos\n");
    s_srcOk = BGA_GetLayerSrc(EB_SELECT, k_slotS1[0], &s_srcS1) &&
              BGA_GetLayerSrc(EB_SELECT, k_slotS2[0], &s_srcS2) &&
              BGA_GetLayerSrc(EB_SELECT, 0x15, &s_srcTV);
    if (!s_srcOk) Log_Print("ZSELECTEZ: slots de banner do SELECTEZ incompletos\n");

    /* 0x8075000: banners do 91E.DAT (idioma do INI +0xBB1 == 0 -> 91H; o projeto usa ENG) */
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/BGA/91E.DAT", g_game.currentDirectory);
    for (int i = 0; i < EZ_COUNT; i++) s_tex[i][0] = s_tex[i][1] = s_tex[i][2] = -1;
    if (RES_Open(path)) {
        static const char* const k_suffix[3] = { "S1", "S2", "TV" };
        int ok = 0;
        for (int i = 0; i < EZ_COUNT; i++)
            for (int k = 0; k < 3; k++) {
                char name[24];
                snprintf(name, sizeof(name), "%X%s.TGA", (unsigned)k_ezSongs[i].id, k_suffix[k]);
                s_tex[i][k] = loadTextureFromRES(name);
                if (s_tex[i][k] >= 0) ok++;
            }
        RES_Close();
        Log_Print("ZSELECTEZ: %d/%d banners do 91E.DAT\n", ok, EZ_COUNT * 3);
    } else {
        Log_Print("ZSELECTEZ: falha ao abrir '%s'\n", path);
    }

    for (int k = 0; k < EZS_COUNT; k++)
        if (s_snd[k] < 0) s_snd[k] = Audio_LoadWaveFile(k_snd[k]);

    s_joined = Title_GetJoinedMask() & 3;
    if (s_joined == 0) s_joined = 1;

    /* estágio (0-based): o Loading decrementa stageCount a cada música */
    int stage = g_game.isBonusSong ? 3 : 3 - g_game.stageCount;
    if (stage < 0) stage = 0;
    if (stage > 3) stage = 3;
    if (stage == 0) { s_skin = 0; s_speed = 1; }        /* [+0xb8] = 0, [+0xbc] = 1 */

    s_cursor = 0;                                        /* 0x805ade0 */
    s_previewOn = true;
    s_ready = false;
    s_move = 0;
    s_idle = 0.0f;
    s_started = false;
    s_timeTotal = 0;                                     /* lido na 1ª passada do desenho */
    s_time = s_timePrev = 99;
    s_timeRunning = false;

    /* slots 9 e 0x15 = TV do 1º registro */
    setSlotTex(9, &s_srcTV, s_tex[0][2]);
    setSlotTex(0x15, &s_srcTV, s_tex[0][2]);

    snprintf(path, sizeof(path), "%s/BGA/BA%02d.MOV", g_game.currentDirectory, stage + 1);
    if (!Movie_Open(path, true)) Log_Print("ZSELECTEZ: '%s' não abriu\n", path);
    playPreview(k_ezSongs[s_cursor].id);
    Log_Print("ZSELECTEZ: estágio %d, %d músicas, P%s\n", stage + 1, EZ_COUNT,
              (s_joined & 3) == 3 ? "1+P2" : (s_joined & 2) ? "2" : "1");
}

/* 0x8076810: entrada tardia */
static void lateJoin(int p) {
    if (!Coin_HasCredit()) return;
    sfx(EZS_JOIN);
    Coin_ConsumeCredit();
    s_joined |= (1u << p);
    Title_SetJoinedMask(s_joined);
}

static void moveTo(int cursor, int dir) {
    s_idle = 0.0f;
    s_ready = false;
    s_previewOn = false;
    s_cursor = cursor;
    s_move = dir;
    BGA_SceneReset(EB_SELECT, dir == 1 ? "LEFT" : "RIGHT");
    setSlotTex(0x15, &s_srcTV, s_tex[s_cursor][2]);
    sfx(EZS_MOVE);
    int lv = k_ezSongs[s_cursor].level;
    if (lv >= 1 && lv <= 3) BGA_SceneReset(EB_SELECT, k_levelScene[lv]);
}

/* 0x8076420 */
static void playerInput(int p) {
    if (!(s_joined & (1u << p))) {
        if (Input_IsPadHit(p, PAD_C)) lateJoin(p);
        return;
    }
    if (Input_IsPadHit(p, PAD_UL)) {
        s_ready = false;
        if (++s_speed > 3) s_speed = 1;
        BGA_SceneReset(EB_SELECT, k_speedScene[s_speed]);
        sfx(EZS_SPEED);
    }
    if (Input_IsPadHit(p, PAD_UR)) {
        s_ready = false;
        s_skin = (s_skin + 1) % 5;
        BGA_SceneReset(EB_SKIN, k_skinScene[s_skin]);
        sfx(EZS_SKIN);
    }
    if (Input_IsPadHit(p, PAD_C)) {
        if (!s_ready) {
            sfx(EZS_SELECT);
            BGA_SceneReset(EB_SELECT, "CENTER");
            BGA_SceneReset(EB_SKIN, "READY");
            s_ready = true;
        } else {
            s_time = 0;
        }
    }
    if (Input_IsPadHit(p, PAD_DL)) moveTo(s_cursor + 1 >= EZ_COUNT ? 0 : s_cursor + 1, 1);   /* 0x8076990 */
    if (Input_IsPadHit(p, PAD_DR)) moveTo(s_cursor - 1 < 0 ? EZ_COUNT - 1 : s_cursor - 1, 2); /* 0x80769b0 */
}

static void startGame(void) {
    if (s_started) return;
    s_started = true;
    sfx(EZS_START);
    /* velocidade: [+0xbc] 2 -> 2X, 3 -> 3X (o 4 -> 4X do original não é alcançável) */
    int speed = (s_speed == 2) ? 2 : (s_speed == 3) ? 3 : 1;
    /* skin: [+0xb8] 1..4 -> 2..5, senão 0 */
    static const int k_skin[5] = { 0, 2, 3, 4, 5 };
    Zero_SetSkinIndex(k_skin[s_skin]);
    Log_Print("ZSELECTEZ: RUN %X -p (nível %d, velocidade %dX, skin %d)\n",
              (unsigned)k_ezSongs[s_cursor].id, k_ezSongs[s_cursor].level, speed, k_skin[s_skin]);
    Movie_Close();
    BGM_Stop();
    if (!ExSelect_StartZeroEz(k_ezSongs[s_cursor].id, k_ezSongs[s_cursor].level, s_joined, speed))
        s_started = false;
}

void ZeroEz_Update(float dt) {
    if (s_started) return;
    if (Movie_IsOpen()) Movie_Update(dt);

    /* relógio: parte quando "START" chega ao quadro 90 */
    int startOff = BGA_SceneOffset(EB_SELECT, "START");
    if (!s_timeRunning && startOff >= 0x5a && s_timeTotal > 0) {
        s_timeRunning = true;
        s_timeStart = timeGetTime();
    }
    if (s_timeRunning) {
        s_timePrev = s_time;
        if (s_time > 0)
            s_time = s_timeTotal / 0x1e - (int)((timeGetTime() - s_timeStart) / 1000);
        if (s_time != s_timePrev && s_time < 11 && s_time > 0) sfx(EZS_TIME);
        playerInput(0);
        playerInput(1);
    }

    /* 30 quadros parado: prévia da música do cursor */
    if ((int)s_idle > 0x1d && !s_previewOn) {
        s_previewOn = true;
        playPreview(k_ezSongs[s_cursor].id);
    }
    s_idle += 1.0f;

    if (s_timeRunning && s_time < 1) startGame();
}

void ZeroEz_Render(void) {
    if (g_game.bgaPicCount < 2) return;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 640, 0, 480, -500, 500);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Movie_Render();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (s_timeTotal <= 0) {                              /* 0x80a18f0(SKINEZ, "TIME") */
        s_timeTotal = BGA_SceneLength(EB_SKIN, "TIME");
        if (s_timeTotal <= 0) s_timeTotal = 60 * 0x1e;   /* reserva: 60 s */
    }

    /* 0x8076360: banners em volta do cursor */
    int off = (s_move == 1) ? -1 : 0;
    for (int i = 0; i < 10; i++) {
        int k = wrap(s_cursor + (off - i) + 5);
        setSlotTex(k_slotS1[i], &s_srcS1, s_tex[k][0]);
        setSlotTex(k_slotS2[i], &s_srcS2, s_tex[k][1]);
    }
    /* 0x80769d0: dígitos do tempo (dezena no slot 0x1a, unidade no 0x1b) */
    if (s_digitOk) {
        int t = s_time < 0 ? 0 : (s_time > 99 ? 99 : s_time);
        if (!s_timeRunning) t = s_timeTotal / 0x1e;
        if (t > 99) t = 99;
        BGA_SetLayerSrc(EB_SKIN, 0x1b, &s_digit[t % 10]);
        BGA_SetLayerSrc(EB_SKIN, 0x1a, &s_digit[(t / 10) % 10]);
    }

    if (s_joined & 1) BGA_ScenePlay(EB_SELECT, "1P", true);
    if (s_joined & 2) BGA_ScenePlay(EB_SELECT, "2P", true);
    BGA_ScenePlay(EB_SELECT, "START", true);
    BGA_ScenePlay(EB_SKIN, "TIME_BEGIN", true);

    if (BGA_SceneOffset(EB_SELECT, "START") >= 0x5a) {
        BGA_ScenePlay(EB_SELECT, "LEFT STOP", true);
        BGA_ScenePlay(EB_SELECT, "RIGHT STOP", true);
        if (s_move == 1 || s_move == 2) {
            const char* n = (s_move == 1) ? "LEFT" : "RIGHT";
            BGA_ScenePlay(EB_SELECT, n, true);
            if (BGA_SceneDone(EB_SELECT, n)) s_move = 0;
        } else {
            BGA_ScenePlay(EB_SELECT, "selection stop", true);
            BGA_ScenePlay(EB_SELECT, "TV STOP", true);
        }
        BGA_ScenePlay(EB_SELECT, "CENTER STOP", true);
        BGA_ScenePlay(EB_SELECT, "CENTER", true);
        BGA_ScenePlay(EB_SELECT, k_speedScene[s_speed], true);
        BGA_ScenePlay(EB_SKIN, k_skinScene[s_skin], true);
        int lv = k_ezSongs[s_cursor].level;
        if (lv >= 1 && lv <= 3) BGA_ScenePlay(EB_SELECT, k_levelScene[lv], true);
        /* "TIME" no quadro proporcional ao tempo decorrido */
        int f = 0;
        if (s_timeRunning) {
            float sec = (float)(timeGetTime() - s_timeStart) / 1000.0f;
            f = (int)((float)s_timeTotal * (sec / (float)(s_timeTotal / 0x1e)));
            if (f < 0) f = 0;
            if (f >= s_timeTotal) f = s_timeTotal - 1;
        }
        BGA_ScenePlayAt(EB_SKIN, "TIME", f);
    }
    if (s_ready) BGA_ScenePlay(EB_SKIN, "READY", true);
}
