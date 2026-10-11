/*
 * zero_mission.c — MISSION STATION do Pump It Up Zero (piu, proc "SELECTMS",
 * classe de 0xdc bytes criada em 0x80794c0).
 *
 *   0x8079530 Begin   BGA/SELECT.DAT [+0x38] e BGA/SELECT3.DAT [+0x3c]; dígitos =
 *                     slots 0x14.., 0x32.. e 0x46.. do SELECT3; banners "M%s1.TGA" do
 *                     BGA/92.DAT (0x807a7f0); lista (0x807afe0) = registros com o
 *                     byte +0xC ligado; cursor inicial 2; contador de 30 s; prévia
 *                     da missão "01"; EFF_SELECT_START.
 *   0x8079d50 Run     prévia (0x807a640: BGA/PREVIEW/M%s1.MOV, senão BGA/000P.MOV;
 *                     AUDIO/D%s.AUD, senão a da 1ª música) 30 quadros depois de
 *                     parar; roda (0x807a920, a mesma conta da CSelect); dígitos
 *                     (0x807b0e0); cenas "RIGHT" (no quadro do giro), "SET",
 *                     "MSTATIONSET" (SELECT3), efeito do movimento, "STEFFECT",
 *                     "2PLEFT", "2PRIGHT", "TIME", "1P_LV", "READY"; tempo < 1 ->
 *                     EFF_START e "RUN M%s1 <modo da 1ª música>".
 *   0x807aba0 entrada C pronto (EFF_SELECT) e, de novo, começa; DL anterior, DR
 *                     próxima (EFF_MOVE / EFF_MOVE_ACC, mesma repetição da CSelect).
 *
 * Tabela 0x081183c0: 32 registros de 0x40 bytes — nome ("CR", "NR", "01".."30"),
 * +0xC liberada, e 3 músicas de 0x10 bytes: { id da música base, bit do modo
 * (0x10 N, 0x20 H, 0x200 C, 0x100 D, 0x400 NM), comandos forçados (numeração
 * própria, traduzida em 0x080536c0 — ver ExSelect_StartZeroMission),
 * ponteiro da função de condição }. O chart é STEP/M<nn><k>.STX; áudio, vídeo e
 * título vêm da música base.
 *
 * FEITO: seleção; as 3 músicas em sequência com modo e comandos; tela de
 * informação; condição de cada música; MRESULT (COMPLETE / FAIL); Next Mission;
 * MCOMPLETE; nome no ranking de missões (ver o segundo bloco, mais abaixo).
 * AINDA NÃO FEITO:
 *   - itens das missões no gameplay (BGA/MICON.DAT: skin_vel, skin_3x, skin_4x,
 *     skin_8x, skin_min) — as condições que exigem "nenhuma mina" passam direto;
 *   - "CONTINUE" depois do FAIL (BGA/CONTINUE.DAT) e a tela de REWARD;
 *   - tela do histórico (BGA/MHIS.DAT), que lê o MRANK.DAT;
 *   - prêmios de algumas missões (0x8054860 no MCOMPLETE) e os pontos (0x805b9b0);
 *   - CR / NR (missões aleatórias, sem .STX próprio): fora da lista;
 *   - liberação por progresso (byte +0xC, salvo no INI): todas as 30 aparecem.
 */
#include "pumpy.h"
#include "bga.h"
#include "movie.h"
#ifndef _WIN32
#include <sys/stat.h>
#endif

enum { MB_SELECT = 0, MB_SELECT3 = 1 };

typedef struct { int baseId; unsigned modeBit; unsigned mods; unsigned condFn; } MsSong;
typedef struct { const char* name; unsigned char open; MsSong song[3]; } MsMission;

/* 0x081183c0 (gerado do binário; condFn = endereço da função de condição no piu) */
static const MsMission k_missions[] = {
    { "CR", 1, { { -1, 0x200, 0x00004, 0x08051980 }, { -1, 0x200, 0x00004, 0x08051980 }, { -1, 0x200, 0x00004, 0x08051980 } } },
    { "NR", 1, { { -1, 0x400, 0x00004, 0x08051980 }, { -1, 0x400, 0x00004, 0x08051980 }, { -1, 0x400, 0x00004, 0x08051980 } } },
    { "01", 1, { { 0xB08, 0x100, 0x00002, 0x08052800 }, { 0x505, 0x20, 0x80002, 0x08052680 }, { 0x303, 0x20, 0x00042, 0x08052800 } } },
    { "02", 1, { { 0xA06, 0x100, 0x10088, 0x08051980 }, { 0xB19, 0x20, 0x20022, 0x080518B0 }, { 0xA05, 0x20, 0x00102, 0x080518B0 } } },
    { "03", 1, { { 0xA09, 0x20, 0x00084, 0x080526A0 }, { 0xC17, 0x200, 0x20008, 0x080528D0 }, { 0xC14, 0x200, 0x40002, 0x08052140 } } },
    { "04", 1, { { 0xB16, 0x200, 0x00208, 0x08052560 }, { 0xA04, 0x200, 0x00440, 0x08052800 }, { 0xC03, 0x200, 0x10004, 0x080518B0 } } },
    { "05", 0, { { 0xB17, 0x100, 0x00008, 0x080525A0 }, { 0x701, 0x100, 0x00008, 0x08052800 }, { 0xA03, 0x100, 0x000A8, 0x080518B0 } } },
    { "06", 0, { { 0x711, 0x200, 0x40102, 0x08051980 }, { 0x505, 0x200, 0x08024, 0x080528D0 }, { 0xB20, 0x200, 0x80024, 0x08052190 } } },
    { "07", 0, { { 0xC06, 0x400, 0x00008, 0x080521D0 }, { 0xC18, 0x400, 0x00008, 0x080525C0 }, { 0xC1818, 0x400, 0x00208, 0x080517D0 } } },
    { "08", 0, { { 0x312, 0x20, 0x40020, 0x08052800 }, { 0x313, 0x20, 0x80028, 0x08051980 }, { 0x318, 0x20, 0x00120, 0x080518B0 } } },
    { "09", 0, { { 0x212, 0x200, 0x00004, 0x08052540 }, { 0xA06, 0x200, 0x00208, 0x08052B60 }, { 0xB16, 0x200, 0x20088, 0x080518B0 } } },
    { "10", 0, { { 0xC1B13, 0x400, 0x08008, 0x08052210 }, { 0x704, 0x400, 0x00208, 0x08052250 }, { 0xC04, 0x400, 0x40008, 0x080518B0 } } },
    { "11", 0, { { 0xC1B18, 0x200, 0x40008, 0x080521B0 }, { 0x204, 0x200, 0x00208, 0x080518B0 }, { 0x712, 0x200, 0x00288, 0x080518B0 } } },
    { "12", 0, { { 0x202, 0x100, 0x00810, 0x080518B0 }, { 0x208, 0x100, 0x40010, 0x080528D0 }, { 0x109, 0x100, 0x00810, 0x08051980 } } },
    { "13", 0, { { 0x108, 0x20, 0x00028, 0x080518B0 }, { 0xB16, 0x20, 0x09002, 0x080518B0 }, { 0x112, 0x20, 0x00030, 0x080518B0 } } },
    { "14", 0, { { 0x805, 0x200, 0x80048, 0x080525E0 }, { 0xA20, 0x200, 0x08008, 0x08052800 }, { 0xC1814, 0x400, 0x00004, 0x08052600 } } },
    { "15", 0, { { 0xC1112, 0x400, 0x00008, 0x080521B0 }, { 0xC20, 0x400, 0x40004, 0x08052640 }, { 0x212, 0x400, 0x00204, 0x08052740 } } },
    { "16", 0, { { 0x736, 0x100, 0x40102, 0x080518B0 }, { 0x711, 0x100, 0x80084, 0x08052620 }, { 0x405, 0x100, 0x08088, 0x08052230 } } },
    { "17", 0, { { 0xA08, 0x200, 0x00284, 0x080522D0 }, { 0x806, 0x400, 0x80048, 0x08052170 }, { 0x802, 0x400, 0x00288, 0x08052290 } } },
    { "18", 0, { { 0x318, 0x20, 0x000A8, 0x080518B0 }, { 0x313, 0x20, 0x40084, 0x08052D20 }, { 0x202, 0x20, 0x000B0, 0x08052400 } } },
    { "19", 0, { { 0xC15, 0x400, 0x00044, 0x08052A80 }, { 0x302, 0x400, 0x20008, 0x08052B60 }, { 0x711, 0x400, 0x00288, 0x08052FC0 } } },
    { "20", 0, { { 0x304, 0x20, 0x800A2, 0x080518B0 }, { 0x308, 0x100, 0x00882, 0x080518B0 }, { 0x205, 0x20, 0x40080, 0x080529A0 } } },
    { "21", 0, { { 0xC02, 0x200, 0x00000, 0x08051A50 }, { 0xA18, 0x400, 0x00208, 0x080534E0 }, { 0xC01, 0x400, 0x00084, 0x08052190 } } },
    { "22", 0, { { 0x213, 0x20, 0x000A8, 0x08052EE0 }, { 0xC21, 0x20, 0x001A0, 0x08052740 }, { 0x303, 0x100, 0x00804, 0x08052660 } } },
    { "23", 0, { { 0xC1204, 0x200, 0x00088, 0x08052600 }, { 0xB17, 0x400, 0x80208, 0x08052540 }, { 0xA10, 0x400, 0x40400, 0x080517D0 } } },
    { "24", 0, { { 0xC1405, 0x400, 0x80004, 0x080521D0 }, { 0xC1C11, 0x400, 0x20008, 0x08052210 }, { 0xC1A02, 0x400, 0x08008, 0x080525C0 } } },
    { "25", 0, { { 0x502, 0x20, 0x10420, 0x080517D0 }, { 0x310, 0x20, 0x80004, 0x08052640 }, { 0xC04, 0x20, 0x00084, 0x08052740 } } },
    { "26", 0, { { 0xA06, 0x400, 0x00008, 0x08052580 }, { 0x301, 0x400, 0x00208, 0x08052700 }, { 0xC1902, 0x400, 0x00084, 0x08053410 } } },
    { "27", 0, { { 0xB20, 0x400, 0x08204, 0x080530A0 }, { 0x303, 0x400, 0x10008, 0x08052800 }, { 0xC1B18, 0x400, 0x40008, 0x08052C40 } } },
    { "28", 0, { { 0xC1205, 0x200, 0x20004, 0x080521B0 }, { 0xC1204, 0x200, 0x80048, 0x08052640 }, { 0xB19, 0x400, 0x00008, 0x08052720 } } },
    { "29", 0, { { 0xC17, 0x100, 0x000C8, 0x08052740 }, { 0x505, 0x100, 0x08208, 0x08053260 }, { 0xC05, 0x100, 0x80004, 0x080526C0 } } },
    { "30", 0, { { 0xC1735, 0x400, 0x00284, 0x08052310 }, { 0xC1C03, 0x400, 0x000C4, 0x08052350 }, { 0xC1C04, 0x400, 0x00288, 0x08052390 } } },
};
#define MS_TOTAL ((int)(sizeof(k_missions) / sizeof(k_missions[0])))

enum { MSS_SELECT, MSS_MOVE, MSS_MOVE_ACC, MSS_START, MSS_TIME, MSS_COUNT };
/* SCRIPT/UI/ZERO/SFX_SELECT.LUA e SFX_GLOBAL.LUA (os mesmos da CSelect) */
static const char* const k_snd[MSS_COUNT] = { "3-2.WAV", "3-2.WAV", "10-2.WAV", "START.WAV", "TIME_LIMIT.WAV" };
static int s_snd[MSS_COUNT] = { -1, -1, -1, -1, -1 };
static void sfx(int k) { if (s_snd[k] >= 0) Audio_Play(s_snd[k], false); }

static bool s_active;                 /* a Station escolheu MISSION */
static int  s_list[MS_TOTAL];         /* [+0x34]: índices em k_missions */
static int  s_count;                  /* +0x24 */
static int  s_tex[MS_TOTAL];          /* banners M%s1.TGA */
static int  s_cursor;                 /* +0x1c */
static bool s_ready;                  /* +4 */
static bool s_previewDone;            /* +5 */
static int  s_spinDir;                /* +6: 0 parado, 1 direita, 2 esquerda */
static int  s_spin;                   /* [0x086281d4] 0..15 */
static int  s_repeat;                 /* [0x086281d0] */
static float s_idle;                  /* +0xd4 */
static int  s_time, s_timePrev;       /* +0x28 / +0x2c */
static uint32_t s_timeTick;           /* +0xd0 */
static float s_previewA;              /* [0x086281d8] */
static int  s_previewPending = -1;
static const char* s_moveFx;          /* +0xc */
static BGALayerSrc s_digTime[11], s_dig1P[11];
static bool s_digOk;
static unsigned s_joined;
static bool s_started;
static uint32_t s_holdStart[2][2];

/* missão em curso */
static int  s_mission = -1;           /* índice em k_missions */
static int  s_song;                   /* 0..2 */
static bool s_pendingStart;           /* a Select reabriu só para começar a próxima música */
static char s_stepName[8];

void ZeroMs_SetActive(bool on) { s_active = on; s_mission = -1; s_song = 0; s_pendingStart = false; }
bool ZeroMs_Active(void) { return s_active; }
bool ZeroMs_InProgress(void) { return s_active && s_mission >= 0; }
const char* ZeroMs_StepName(void) { return ZeroMs_InProgress() ? s_stepName : NULL; }

void ZeroMs_End(void) { s_mission = -1; s_song = 0; s_pendingStart = false; }

bool ZeroMs_NextSong(void) {
    if (s_mission < 0) return false;
    if (s_song >= 2) return false;
    s_song++;
    s_pendingStart = true;
    return true;
}

static int modeIndex(unsigned bit) {   /* 0x807a0f0: 0x100 -d, 0x20 -h, 0x200 -c, 0x400 -nm, senão -n */
    switch (bit) {
    case 0x20:  return 1;
    case 0x200: return 2;
    case 0x100: return 3;
    case 0x400: return 4;
    default:    return 0;
    }
}

static bool startSong(void) {
    const MsMission* m = &k_missions[s_mission];
    const MsSong* sg = &m->song[s_song];
    snprintf(s_stepName, sizeof(s_stepName), "M%s%d", m->name, s_song + 1);
    static const char* const k_arg[5] = { "-n", "-h", "-c", "-d", "-nm" };
    int mi = modeIndex(sg->modeBit);
    Log_Print("ZSELECTMS: RUN %s %s (missão %s, música %d/3, base %X, comandos 0x%05X, condição 0x%08X ainda não aplicada)\n",
              s_stepName, k_arg[mi], m->name, s_song + 1, (unsigned)sg->baseId, sg->mods, sg->condFn);
    return ExSelect_StartZeroMission(sg->baseId, mi, sg->mods, s_joined);
}

static void openPreview(int mi) {
    const MsMission* m = &k_missions[mi];
    char mov[MAX_PATH], aud[MAX_PATH];
    FILE* f;
    snprintf(mov, sizeof(mov), "%s/BGA/PREVIEW/M%s1.MOV", g_game.currentDirectory, m->name);
    if ((f = fopen(mov, "rb")) != NULL) fclose(f);
    else snprintf(mov, sizeof(mov), "%s/BGA/000P.MOV", g_game.currentDirectory);
    snprintf(aud, sizeof(aud), "%s/AUDIO/D%s.AUD", g_game.currentDirectory, m->name);
    if ((f = fopen(aud, "rb")) != NULL) fclose(f);
    else if (m->song[0].baseId != -1) {
        int id = m->song[0].baseId;
        if (id > 0xC0FFF) id -= 0xC1000;                /* 0x807a6a0 */
        snprintf(aud, sizeof(aud), "%s/AUDIO/D%X.AUD", g_game.currentDirectory, (unsigned)id);
    }
    Movie_Close();
    Movie_Open(mov, true);
    BGM_Stop();
    if (BGM_LoadAUDDirect(aud)) BGM_Play(false);
}

void ZeroMs_Enter(void) {
    Title_StopMusic();
    Movie_Close();
    BGM_Stop();
    s_started = false;
    if (s_pendingStart && s_mission >= 0) return;       /* próxima música: começa no 1º Update */

    Resource_ClearBGA();
    if (!Resource_LoadBGAByName("SELECT"))  Log_Print("ZSELECTMS: falha ao carregar BGA\\SELECT.DAT\n");
    if (!Resource_LoadBGAByName("SELECT3")) Log_Print("ZSELECTMS: falha ao carregar BGA\\SELECT3.DAT\n");
    BGA_Reset();

    s_digOk = true;
    for (int d = 0; d < 11; d++) {
        s_digOk &= BGA_GetLayerSrc(MB_SELECT3, 0x14 + d, &s_digTime[d]);
        s_digOk &= BGA_GetLayerSrc(MB_SELECT3, 0x46 + d, &s_dig1P[d]);
    }
    if (!s_digOk) Log_Print("ZSELECTMS: slots de dígito do SELECT3 incompletos\n");

    /* 0x807a7f0: banners do 92.DAT */
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/BGA/92.DAT", g_game.currentDirectory);
    for (int i = 0; i < MS_TOTAL; i++) s_tex[i] = -1;
    if (RES_Open(path)) {
        int ok = 0;
        for (int i = 0; i < MS_TOTAL; i++) {
            char name[16];
            snprintf(name, sizeof(name), "M%s1.TGA", k_missions[i].name);
            s_tex[i] = loadTextureFromRES(name);
            if (s_tex[i] >= 0) ok++;
        }
        RES_Close();
        Log_Print("ZSELECTMS: %d/%d banners do 92.DAT\n", ok, MS_TOTAL);
    } else {
        Log_Print("ZSELECTMS: falha ao abrir '%s'\n", path);
    }

    /* 0x807afe0: lista. Fase 1: as 30 missões numeradas (ver cabeçalho). */
    s_count = 0;
    for (int i = 0; i < MS_TOTAL; i++)
        if (k_missions[i].song[0].baseId != -1) s_list[s_count++] = i;

    for (int k = 0; k < MSS_COUNT; k++)
        if (s_snd[k] < 0) s_snd[k] = Audio_LoadWaveFile(k_snd[k]);

    s_joined = Title_GetJoinedMask() & 3;
    if (s_joined == 0) s_joined = 1;
    s_mission = -1;
    s_song = 0;
    s_cursor = 0;                    /* o original parte do índice 2 da tabela = missão "01" */
    s_ready = false;
    s_previewDone = true;
    s_spinDir = 0;
    s_spin = 0;
    s_repeat = 0;
    s_idle = 0.0f;
    s_time = s_timePrev = 30;
    s_timeTick = timeGetTime();
    s_previewA = 0.0f;
    s_previewPending = s_count > 0 ? s_list[s_cursor] : -1;
    s_moveFx = "RIGHT_EFFECT";
    BGA_SceneSetOffset(MB_SELECT, "RIGHT", 0xF);
    Log_Print("ZSELECTMS: %d missões, P%s\n", s_count, (s_joined & 2) && !(s_joined & 1) ? "2" : "1");
}

/* mesma repetição da CSelect (0x806d4d0 + 0x805c770) */
static bool repeatHit(int p, int k, PadButton b) {
    uint32_t now = timeGetTime();
    if (Input_IsPadHit(p, b)) { s_holdStart[p][k] = now; return true; }
    if (!Input_IsPadDown(p, b)) return false;
    if (now - s_holdStart[p][k] <= 800) return false;
    uint32_t delay = s_repeat < 6 ? 500 : s_repeat < 16 ? 600 : s_repeat < 0x1D ? 700 : 0x2EE;
    s_holdStart[p][k] = now - delay;
    return true;
}

static void move(int dir) {
    sfx(s_repeat < 6 ? MSS_MOVE : MSS_MOVE_ACC);
    s_ready = false;
    s_previewDone = false;
    if (s_count > 0) {
        s_cursor += dir;
        if (s_cursor < 0) s_cursor = s_count - 1;
        if (s_cursor >= s_count) s_cursor = 0;
    }
    s_idle = 0.0f;
    BGA_SceneReset(MB_SELECT, dir > 0 ? "RIGHT" : "LEFT");
    s_moveFx = dir > 0 ? "RIGHT_EFFECT" : "LEFT_EFFECT";
    BGA_SceneReset(MB_SELECT, s_moveFx);
    s_repeat++;
    s_spinDir = dir > 0 ? 1 : 2;
    s_spin = dir > 0 ? 0 : 0xF;
}

/* 0x807aba0 */
static void playerInput(int p) {
    if (!(s_joined & (1u << p))) return;
    if (Input_IsPadHit(p, PAD_C)) {
        sfx(MSS_SELECT);
        if (!s_ready) { s_ready = true; BGA_SceneReset(MB_SELECT3, "READY"); }
        else s_time = 0;
    }
    if (repeatHit(p, 0, PAD_DL)) move(-1);
    if (repeatHit(p, 1, PAD_DR)) move(+1);
}

void ZeroMs_Update(float dt) {
    if (s_pendingStart && s_mission >= 0) {             /* entre as músicas da missão */
        s_pendingStart = false;
        if (!startSong()) { ZeroMs_End(); Game_ChangeState(STATE_GAMEOVER_ENTER); }
        return;
    }
    if (s_started) return;

    if (s_previewPending >= 0) {
        s_previewA -= 0.1f;
        if (s_previewA <= 0.0f) {
            s_previewA = 1.0f;
            openPreview(s_previewPending);
            s_previewPending = -1;
        }
    } else if (Movie_IsOpen()) {
        Movie_Update(dt);
    }

    s_timePrev = s_time;
    if (s_time > 0) s_time = 30 - (int)((timeGetTime() - s_timeTick) / 1000);
    if (s_time != s_timePrev && s_time < 11 && s_time > 0) sfx(MSS_TIME);

    if ((int)s_idle > 0x1D && !s_previewDone) {
        s_previewDone = true;
        if (s_count > 0) s_previewPending = s_list[s_cursor];
    }

    if (s_spinDir == 1) { if (++s_spin >= 0x10) { s_spinDir = 0; s_spin = 0; } }
    else if (s_spinDir == 2) { if (--s_spin < 1) { s_spinDir = 0; s_spin = 0; } }
    s_idle += 1.0f;

    playerInput(0);
    playerInput(1);
    bool hold = false;
    for (int p = 0; p < 2; p++)
        if ((s_joined & (1u << p)) && (Input_IsPadDown(p, PAD_DL) || Input_IsPadDown(p, PAD_DR))) hold = true;
    if (!hold) s_repeat = 0;

    if (s_time < 1 && s_count > 0) {
        s_started = true;
        sfx(MSS_START);
        Movie_Close();
        BGM_Stop();
        s_mission = s_list[s_cursor];
        s_song = 0;
        if (!startSong()) { ZeroMs_End(); s_started = false; s_time = 30; s_timeTick = timeGetTime(); }
    }
}

void ZeroMs_Render(void) {
    if (s_pendingStart || g_game.bgaPicCount < 2) return;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* entrada da roda pela cena "SET" (0x8079f30) */
    int setOff = BGA_SceneOffset(MB_SELECT, "SET");
    int idxOff = 0;
    float py = 0.0f;
    if (setOff < 0x1E) {
        idxOff = -1;
        if (setOff == 0xF) { s_spinDir = 1; idxOff = 0; s_spin = 0; }
        else if (setOff > 0xF) idxOff = 0;
        py = (float)setOff * 1.6666666f - 50.0f;
    }
    ZeroSelect_DrawWheel(s_list, s_count, s_tex, s_cursor, s_spinDir, s_spin, idxOff, 0.0f, py);

    /* 0x807b0e0: nível em branco (dígito 10) e tempo */
    if (s_digOk) {
        int t = s_time < 0 ? 0 : s_time;
        BGA_SetLayerSrc(MB_SELECT3, 0x11, &s_dig1P[10]);
        BGA_SetLayerSrc(MB_SELECT3, 0x10, &s_dig1P[10]);
        BGA_SetLayerSrc(MB_SELECT3, 0x13, &s_digTime[t % 10]);
        BGA_SetLayerSrc(MB_SELECT3, 0x12, &s_digTime[(t / 10) % 10]);
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 640, 0, 480, -500, 500);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Movie_RenderRect(0.0f, 175.0f, 640.0f, 415.0f, 1.0f, s_previewA);

    bool in = BGA_SceneOffset(MB_SELECT, "SET") > 0x1C;
    if (in) BGA_ScenePlayAt(MB_SELECT, "RIGHT", s_spin == 0xF ? 14 : s_spin);
    BGA_ScenePlay(MB_SELECT, "SET", true);
    BGA_ScenePlay(MB_SELECT3, "MSTATIONSET", true);
    if (in) {
        BGA_ScenePlay(MB_SELECT, s_moveFx, true);
        BGA_ScenePlay(MB_SELECT, "STEFFECT", true);
        BGA_ScenePlay(MB_SELECT, "2PLEFT", true);
        BGA_ScenePlay(MB_SELECT, "2PRIGHT", true);
    }
    BGA_ScenePlay(MB_SELECT3, "TIME", true);
    BGA_ScenePlay(MB_SELECT3, "1P_LV", true);
    if (s_ready) BGA_ScenePlay(MB_SELECT3, "READY", true);
}

/* ===========================================================================
 * Fluxo da missão depois da seleção (conferido no piu):
 *
 *   tela de informação   CPlayEngine, 0x8082d30 (com [+0xdd80] = modo missão):
 *       fundo = TITLE/T<M011>E.PNZ (a imagem já traz o objetivo; 0x8081... guarda
 *       a textura em [0x8134694]+0x30); BGA/MICF.DAT: cena "TIMER" no quadro do
 *       relógio (0x80a2f80 = ms * 60 / 1000) e cena "TIME"; dígitos = slots
 *       0x46..0x4f do MICF nos slots 0x3d (dezena) e 0x3e (unidade);
 *       contador (0x5014 - ms) / 1000; ao passar de 6 para 5: EFF_MISSION_READY
 *       (V_READY.WAV); CENTER depois de 2000 ms ou o fim do tempo ->
 *       EFF_MISSION_START (START.WAV) e a música começa.
 *   fim da música        "MRESULT" no lugar do "GRADE" (0x8083bf0): sem Dance Grade.
 *   MRESULT              0x8078f80 / 0x8079110: mesmo fundo + cena "COMPLETE" ou
 *       "FAIL" do MICF; o resultado é a função de condição da música
 *       ([registro]+0xC). COMPLETE: depois de 3000 ms -> "NEXTSTAGE 1/2" nas duas
 *       primeiras músicas, "MCOMPLETE" na terceira. FAIL: fim da cena -> "CONTINUE".
 *   NEXTSTAGE            0x8077bb0: BGA/NMISSION.MOV + EFF_TRY_NEXT_MISSION
 *       (MISSION_NEXT.WAV) até o vídeo acabar -> "RUN M<nn><k> <modo>".
 *   MCOMPLETE            0x807bf60: BGA/M_CLEAR.MOV + WAVE/MCOMPLETE.WAV -> "NAMEINPUT".
 *
 * Campos do jogador usados pelas condições (0x80517d0..0x80534e0):
 *   +0x17c razão da nota · +0x20 PERFECT · +0x24 GREAT · +0x28 GOOD · +0x2c BAD ·
 *   +0x30 MISS · +0x174 max combo · +0x34 contador que tem de ser 0 em várias
 *   missões (provável: minas pisadas; os itens ainda não existem no projeto, então
 *   aqui vale sempre 0).
 * A razão usada é a do projeto (Result_ExRatio); a de +0x17c não foi conferida.
 *
 * AINDA NÃO FEITO: "CONTINUE" (BGA/CONTINUE.DAT) — o FAIL cai no GAMEOVER.
 * ========================================================================= */
float Result_ExRatio(int p);   /* result.c */
int Gameplay_MissionMines(int p);   /* gameplay.c: minas pisadas na música */

typedef struct { float ratio; int perfect, great, good, bad, miss, combo, mines; } MsStats;

static bool condEval(unsigned fn, const MsStats* s) {
    #define A      (s->ratio >= 0.95f)             /* 0x80518b0 */
    #define B      (s->ratio >= 0.90f)             /* 0x8051980 */
    #define D      (s->ratio >= 0.75f)             /* 0x8051a50 */
    #define MLT(n) (s->miss < (n))                 /* 0x8051cc0 */
    #define MEQ(n) (s->miss == (n))                /* 0x8051c00 */
    #define CGE(n) (s->combo >= (n))               /* 0x8051f00 */
    #define CEQ(n) (s->combo == (n))               /* 0x8051fc0 */
    #define CLT(n) (s->combo < (n))                /* 0x8052080 */
    #define NOMINE (s->mines == 0)                 /* [+0x34] == 0 */
    int total = s->perfect + s->great + s->good + s->bad + s->miss;
    switch (fn) {
    case 0x080517D0: return s->ratio >= 1.0f && s->miss == 0;
    case 0x080518B0: return A;
    case 0x08051980: return B;
    case 0x08051A50: return D;
    case 0x08051B20: return A && s->miss != 0;
    case 0x08052140: return A && MLT(10);
    case 0x08052170: return MLT(5);
    case 0x08052190: return MLT(15);
    case 0x080521B0: return MLT(20);
    case 0x080521D0: return MLT(30);
    case 0x080521F0: return MLT(40);
    case 0x08052210: return MLT(50);
    case 0x08052230: return MEQ(0);
    case 0x08052250: return MLT(20) && CLT(154);
    case 0x08052290: return MLT(20) && CGE(250);
    case 0x080522D0: return MLT(20) && CGE(300);
    case 0x08052310: return MLT(35) && CGE(333);
    case 0x08052350: return MLT(30) && CGE(555);
    case 0x08052390: return MLT(25) && CGE(777);
    case 0x080523D0: return A && CLT(154);
    case 0x08052400: return A && CGE(101);
    case 0x08052430: return total != 0 && (float)s->combo / (float)total >= 0.75f;
    case 0x08052520: return CGE(510);
    case 0x08052540: return CGE(500);
    case 0x08052560: return CGE(480);
    case 0x08052580: return CGE(450);
    case 0x080525A0: return CGE(350);
    case 0x080525C0: return CGE(300);
    case 0x080525E0: return CGE(280);
    case 0x08052600: return CGE(250);
    case 0x08052620: return CGE(240);
    case 0x08052640: return CGE(200);
    case 0x08052660: return CGE(150);
    case 0x08052680: return CEQ(72);
    case 0x080526A0: return CEQ(108);
    case 0x080526C0: return CEQ(123);
    case 0x080526E0: return CEQ(300);
    case 0x08052700: return CEQ(339);
    case 0x08052720: return CEQ(473);
    case 0x08052740: return total == s->combo;
    case 0x08052800: return A && NOMINE;
    case 0x080528D0: return B && NOMINE;
    case 0x080529A0: return MLT(7) && NOMINE;
    case 0x08052A80: return MLT(10) && NOMINE;
    case 0x08052B60: return MLT(15) && NOMINE;
    case 0x08052C40: return MLT(20) && NOMINE;
    case 0x08052D20: return MEQ(0) && NOMINE;
    case 0x08052EE0: return CGE(150) && NOMINE;
    case 0x08052FC0: return CGE(200) && NOMINE;
    case 0x080530A0: return CGE(345) && NOMINE;
    case 0x08053180: return CEQ(300) && NOMINE;
    case 0x08053260: return s->good == 3 && s->bad == 2 && s->miss == 1 && NOMINE;
    case 0x08053340: return s->good == 5 && s->bad == 3 && s->miss == 1;
    case 0x08053410: return s->good + s->bad + s->miss < 10;
    case 0x080534E0: return s->good + s->bad + s->miss < 11;
    default:
        Log_Print("ZMISSION: condição 0x%08X desconhecida\n", fn);
        return D;
    }
    #undef A
    #undef B
    #undef D
    #undef MLT
    #undef MEQ
    #undef CGE
    #undef CEQ
    #undef CLT
    #undef NOMINE
}

enum { MFS_READY, MFS_START, MFS_NEXT, MFS_CLEAR, MFS_COUNT };
static const char* const k_flowSnd[MFS_COUNT] = { "V_READY.WAV", "START.WAV", "MISSION_NEXT.WAV", "MCOMPLETE.WAV" };
static int s_flowSnd[MFS_COUNT] = { -1, -1, -1, -1 };
static void flowSfx(int k) {
    if (s_flowSnd[k] < 0) s_flowSnd[k] = Audio_LoadWaveFile(k_flowSnd[k]);
    if (s_flowSnd[k] >= 0) { Audio_Stop(s_flowSnd[k]); Audio_Play(s_flowSnd[k], false); }
}
void ZeroMs_PlayNextSound(void) { flowSfx(MFS_NEXT); }   /* EFF_TRY_NEXT_MISSION */

/* ---- tela de informação (antes da música) -------------------------------- */
static uint32_t s_infoStart;
static int  s_infoSecs;
static bool s_infoOk, s_infoDone;
static BGALayerSrc s_infoDig[10];

void ZeroMs_InfoEnter(void) {
    s_infoOk = Resource_LoadBGAByName("MICF");
    if (!s_infoOk) Log_Print("ZMISSION: falha ao carregar BGA\\MICF.DAT\n");
    BGA_Reset();
    if (s_infoOk)
        for (int d = 0; d < 10; d++) s_infoOk &= BGA_GetLayerSrc(0, 0x46 + d, &s_infoDig[d]);
    s_infoStart = timeGetTime();
    s_infoSecs = 0x5014 / 1000;
    s_infoDone = false;
}

/* true quando a tela terminou (a música pode começar) */
bool ZeroMs_InfoUpdate(void) {
    if (s_infoDone) return true;
    uint32_t el = timeGetTime() - s_infoStart;
    int prev = s_infoSecs;
    s_infoSecs = el >= 0x5014 ? 0 : (int)((0x5014u - el) / 1000u);
    if (s_infoSecs < 6 && prev > 5) flowSfx(MFS_READY);
    bool out = el > 0x5014;
    if (el > 2000)
        for (int p = 0; p < 2; p++)
            if ((s_joined & (1u << p)) && Input_IsPadHit(p, PAD_C)) out = true;
    if (!out) return false;
    flowSfx(MFS_START);
    s_infoDone = true;
    return true;
}

void ZeroMs_InfoRender(void) {
    if (!s_infoOk || s_infoDone) return;
    uint32_t el = timeGetTime() - s_infoStart;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    BGA_ScenePlayAt(0, "TIMER", (int)((double)el * 60.0 / 1000.0));
    BGA_SetLayerSrc(0, 0x3e, &s_infoDig[s_infoSecs % 10]);
    BGA_SetLayerSrc(0, 0x3d, &s_infoDig[(s_infoSecs / 10) % 10]);
    BGA_ScenePlay(0, "TIME", true);
}

/* ---- ranking de missões: /SETTINGS/MRANK.DAT (CMissionRankManager) --------
 * 0x805bbe0 Load / 0x805bd90 Save / 0x805bec0 padrão / 0x805bf30 insere.
 * Arquivo de 0x1d4c4 bytes: u32 Adler-32 (0x80a29f0) dos 120000 bytes seguintes,
 * que são 10000 registros de 12 bytes { char nome[4]; s32 missão; s32 0 }.
 * Padrão: nome "ZERO", missão = índice, terceiro campo -1. A inserção (0x8071a..,
 * NAMEINPUT com a estação 2) empurra tudo uma posição e grava na frente:
 * missão = número ("01".."30"), -2 para "CR" e -1 para "NR". */
#define MRANK_RECS 10000
#define MRANK_SIZE (4 + MRANK_RECS * 12)
static bool s_rankPending;
static int  s_rankMission;

static uint32_t mrankAdler(const uint8_t* d, uint32_t n) {
    uint32_t a = 1, b = 0;
    for (uint32_t i = 0; i < n; i++) { a = (a + d[i]) % 0xfff1u; b = (b + a) % 0xfff1u; }
    return a + b * 0x10000u;
}
static void mrankPut32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

bool ZeroMs_RankPending(void) { return s_rankPending; }

void ZeroMs_RankSave(const char* name) {
    if (!s_rankPending) return;
    s_rankPending = false;
    uint8_t* img = (uint8_t*)malloc(MRANK_SIZE);
    if (!img) return;
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/SETTINGS/MRANK.DAT", g_game.currentDirectory);
    bool ok = false;
    FILE* f = fopen(path, "rb");
    if (f) {
        ok = fread(img, 1, MRANK_SIZE, f) == MRANK_SIZE;
        fclose(f);
        if (ok) {
            uint32_t sum = (uint32_t)img[0] | ((uint32_t)img[1] << 8) | ((uint32_t)img[2] << 16) | ((uint32_t)img[3] << 24);
            ok = sum == mrankAdler(img + 4, MRANK_RECS * 12);
        }
    }
    if (!ok) {                                           /* 0x805bec0 */
        memset(img, 0, MRANK_SIZE);
        for (int i = 0; i < MRANK_RECS; i++) {
            uint8_t* r = img + 4 + i * 12;
            memcpy(r, "ZERO", 4);
            mrankPut32(r + 4, (uint32_t)i);
            mrankPut32(r + 8, 0xffffffffu);
        }
    }
    memmove(img + 4 + 12, img + 4, (MRANK_RECS - 1) * 12);   /* 0x805bf30 */
    memcpy(img + 4, name, 4);
    mrankPut32(img + 8, (uint32_t)s_rankMission);
    mrankPut32(img + 12, 0);
    mrankPut32(img, mrankAdler(img + 4, MRANK_RECS * 12));

    char dir[MAX_PATH];
    snprintf(dir, sizeof(dir), "%s/SETTINGS", g_game.currentDirectory);
#ifdef _WIN32
    CreateDirectoryA(dir, NULL);
#else
    mkdir(dir, 0755);
#endif
    f = fopen(path, "wb");
    if (f) { fwrite(img, 1, MRANK_SIZE, f); fclose(f); }
    else Log_Print("ZMISSION: não foi possível gravar '%s'\n", path);
    Log_Print("ZMISSION: ranking de missões: %c%c%c%c, missão %d\n", name[0], name[1], name[2], name[3], s_rankMission);
    free(img);
}

/* ---- MRESULT e MCOMPLETE (depois da música) ------------------------------ */
static int  s_resPhase;       /* 0 COMPLETE/FAIL, 1 M_CLEAR.MOV */
static bool s_resCleared;
static uint32_t s_resStart;
static int  s_resTex = -1;

void ZeroMs_ResultEnter(void) {
    const MsSong* sg = &k_missions[s_mission].song[s_song];
    Gameplay_DoubleToOwner();
    Gameplay_ExScoreSync();
    int p = (g_game.activePlayerMask & 1) ? 0 : 1;
    MsStats st;
    st.ratio   = Result_ExRatio(p);
    st.perfect = g_game.stats.perfectCount[p];
    st.great   = g_game.stats.greatCount[p];
    st.good    = g_game.stats.goodCount[p];
    st.bad     = g_game.stats.badCount[p];
    st.miss    = g_game.stats.missCount[p];
    st.combo   = (int)g_game.stats.maxCombo[p];
    st.mines   = Gameplay_MissionMines(p);
    s_resCleared = condEval(sg->condFn, &st);
    Log_Print("ZMISSION: %s -> %s (condição 0x%08X; razão %.3f, P %d G %d Gd %d B %d M %d, combo %d)\n",
              s_stepName, s_resCleared ? "COMPLETE" : "FAIL", sg->condFn,
              st.ratio, st.perfect, st.great, st.good, st.bad, st.miss, st.combo);

    /* fundo: a mesma imagem da tela de informação */
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/TITLE/T%sE.PNZ", g_game.currentDirectory, s_stepName);
    s_resTex = Resource_LoadPNZ(path);
    BGA_SceneReset(0, "COMPLETE");
    BGA_SceneReset(0, "FAIL");
    s_resPhase = 0;
    s_resStart = timeGetTime();
}

void ZeroMs_ResultUpdate(float dt) {
    if (g_game.state == STATE_DANCE_GRADE_ENTER) {
        Game_ChangeState(STATE_DANCE_GRADE_DISPLAY);
        return;
    }
    if (s_resPhase == 1) {                              /* MCOMPLETE */
        Movie_Update(dt);
        if (!Movie_IsOpen() || Movie_HasEnded()) {
            Movie_Close();
            /* NAMEINPUT com [obj+1] = 1 (0x807c2e0): o nome vai para o ranking de missões */
            s_rankPending = true;
            s_rankMission = atoi(k_missions[s_mission].name);
            if (strcmp(k_missions[s_mission].name, "CR") == 0) s_rankMission = -2;
            if (strcmp(k_missions[s_mission].name, "NR") == 0) s_rankMission = -1;
            ZeroMs_End();
            Resource_ClearBGA();
            Game_ChangeState(STATE_NAMEINPUT);
        }
        return;
    }
    if (s_resCleared) {
        if (timeGetTime() - s_resStart <= 3000) return;
        if (ZeroMs_NextSong()) {                        /* "NEXTSTAGE 1" / "NEXTSTAGE 2" */
            Resource_ClearBGA();
            Game_ChangeState(STATE_STAGE_TRANSITION);
            return;
        }
        /* terceira música: "MCOMPLETE" */
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/BGA/M_CLEAR.MOV", g_game.currentDirectory);
        Movie_Close();
        if (!Movie_Open(path, false)) Log_Print("ZMISSION: '%s' não abriu\n", path);
        flowSfx(MFS_CLEAR);
        s_resPhase = 1;
        return;
    }
    if (BGA_SceneDone(0, "FAIL")) {                     /* "CONTINUE" ainda não existe */
        ZeroMs_End();
        Resource_ClearBGA();
        Game_ChangeState(STATE_GAMEOVER_ENTER);
    }
}

void ZeroMs_ResultRender(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 640, 0, 480, -500, 500);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if (s_resPhase == 1) { Movie_Render(); return; }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (s_resTex >= 0) Texture_Draw(s_resTex, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    if (g_game.bgaPicCount > 0) BGA_ScenePlay(0, s_resCleared ? "COMPLETE" : "FAIL", true);
}
