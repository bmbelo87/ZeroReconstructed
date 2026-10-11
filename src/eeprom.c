#include "pumpy.h"

/* Persistência no formato do PUMPY.EXE (EEPROM do gabinete).
 *
 * O original guarda uma imagem binária de 2048 bytes em "c:\pumpprex3.ini"
 * (fread(...,1,0x800,...) em 0x4067a0; defaults em 0x405150; gravação em 0x405190).
 * Aqui o arquivo é "pumpprex3.ini" na pasta do jogo. Layout (offset = endereço - 0xd38858):
 *
 *   +0x68C  ident  0x312E3358 ("X3.1")           0xd38ee4
 *   +0x690  checksum Adler-32 dos 8 bytes de +0x7E8   0xd38ee8   (0x4195d0 -> 0x419570)
 *   +0x7E8  GAME MODE   (0=NORMAL, 1=EVENT)       0xd39040
 *   +0x7E9  LEVEL       (0=EASY 1=NORMAL 2=HARD)  0xd39041
 *   +0x7EA  STAGE BREAK (0=OFF, 1..4 stages)      0xd39042  default 2
 *   +0x7EB  LANGUAGE                              0xd39043  default 1
 *   +0x7EC  DEMO SOUND  (0=ON)                    0xd39044
 *   +0x7ED  SHOW HELP                             0xd39045
 *   +0x7EE  COIN 1 (moedas por crédito)           0xd39046  default 5
 *   +0x7EF  COIN 2                                0xd39047  default 1
 *   +0x7F0  bookkeeping COIN 1  (u32)             0xd39048  (tela BOOKKEEPING 0x406360)
 *   +0x7F4  bookkeeping COIN 2  (u32)             0xd3904c
 *   +0x7F8  total de moedas     (u32)             0xd39050
 *   +0x7FC  bookkeeping SERVICE (u32)             0xd39054
 *
 * O restante da imagem (ranking etc.) fica como está: é preservado ao regravar.
 *
 * Diferença deliberada: o default do COIN 1 é 0 (FREE PLAY) em vez de 5, senão
 * um arquivo novo deixaria o jogo sem crédito num PC sem moedeiro. */

#define EEP_SIZE       0x800
#define EEP_IDENT      0x68C
#define EEP_CHKSUM     0x690
#define EEP_SETTINGS   0x7E8
#define EEP_SETLEN     8
#define EEP_IDENT_VAL  0x312E3358u   /* "X3.1" */

#define O_MODE      0x7E8
#define O_LEVEL     0x7E9
#define O_STAGEBRK  0x7EA
#define O_LANG      0x7EB
#define O_DEMO      0x7EC
#define O_HELP      0x7ED
#define O_COIN1     0x7EE
#define O_COIN2     0x7EF
#define O_COIN1TOT  0x7F0
#define O_COIN2TOT  0x7F4
#define O_COINTOT   0x7F8
#define O_SVCTOT    0x7FC

#define EEP_DEFAULT_COIN1  0   /* FREE PLAY (o original vem com 5) */

static uint8_t g_eep[EEP_SIZE];

static uint32_t eepGet32(int off) {
    return (uint32_t)g_eep[off] | ((uint32_t)g_eep[off + 1] << 8) |
           ((uint32_t)g_eep[off + 2] << 16) | ((uint32_t)g_eep[off + 3] << 24);
}

static void eepPut32(int off, uint32_t v) {
    g_eep[off]     = (uint8_t)(v);
    g_eep[off + 1] = (uint8_t)(v >> 8);
    g_eep[off + 2] = (uint8_t)(v >> 16);
    g_eep[off + 3] = (uint8_t)(v >> 24);
}

/* Adler-32 com valor inicial 1 (PUMPY.EXE 0x419570: módulo 0xFFF1). */
static uint32_t eepAdler32(const uint8_t* d, int n) {
    uint32_t a = 1, b = 0;
    for (int i = 0; i < n; i++) {
        a = (a + d[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

static void eepStamp(void) {
    eepPut32(EEP_IDENT, EEP_IDENT_VAL);
    eepPut32(EEP_CHKSUM, eepAdler32(&g_eep[EEP_SETTINGS], EEP_SETLEN));
}

/* PUMPY.EXE 0x405150: tudo 0xFF, depois os defaults de 0x404f40/0x404fa0 e os totais zerados. */
static void eepDefaults(void) {
    memset(g_eep, 0xFF, sizeof(g_eep));
    g_eep[O_MODE]     = 0;
    g_eep[O_LEVEL]    = 1;
    g_eep[O_STAGEBRK] = 2;
    g_eep[O_LANG]     = 1;
    g_eep[O_DEMO]     = 0;
    g_eep[O_HELP]     = 0;
    g_eep[O_COIN1]    = EEP_DEFAULT_COIN1;
    g_eep[O_COIN2]    = 1;
    eepPut32(O_COIN1TOT, 0);
    eepPut32(O_COIN2TOT, 0);
    eepPut32(O_COINTOT, 0);
    eepPut32(O_SVCTOT, 0);
    eepStamp();
}

/* era: static void eepPath(...) -> "%s/pumpprex3.ini" (arquivo próprio desta imagem).
 * Pedido do usuário: um arquivo só, PIUZERO.INI, no lugar de pumpprex3.ini e
 * PIUEXCEED2.INI. Ver SaveFile_Read / SaveFile_Write abaixo. */

/* ---------------------------------------------------------------------------
 * PIUZERO.INI do port: um contêiner com as duas imagens de 2048 bytes.
 *   +0x00  "PIUZRCMP"            magia (8 bytes)
 *   +0x08  u32 versão (1)
 *   +0x0C  u32 seções presentes  (bit 0 = seção 0, bit 1 = seção 1)
 *   +0x10  seção 0: SETUP / bookkeeping (eeprom.c, layout do PUMPY.EXE)
 *   +0x810 seção 1: ranking / contagens (eeprom_x2.c, layout do PIU32.EXE)
 * NÃO é o formato do PIUZERO.INI do piu original (o objeto de 0x1010 bytes de
 * 0x08059d10), que o projeto ainda não implementa: só o nome é o mesmo. Um
 * PIUZERO.INI sem a magia (ex.: o do jogo original) é guardado como
 * PIUZERO.INI.orig antes da primeira gravação, nunca sobrescrito.
 * Migração: seção ausente é lida do arquivo antigo (pumpprex3.ini /
 * PIUEXCEED2.INI), se existir; os arquivos antigos não são apagados.
 * ------------------------------------------------------------------------- */
#define SAVEFILE_NAME    "PIUZERO.INI"
#define SAVEFILE_MAGIC   "PIUZRCMP"
#define SAVEFILE_HDR     16
#define SAVEFILE_SECTION 2048
#define SAVEFILE_TOTAL   (SAVEFILE_HDR + 2 * SAVEFILE_SECTION)

static const char* const k_saveLegacy[2] = { "pumpprex3.ini", "PIUEXCEED2.INI" };

/* Lê o contêiner inteiro. false = ausente ou sem a magia (*foreign = existe, mas não é nosso). */
static bool saveFileLoad(uint8_t* img, bool* foreign) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", g_game.currentDirectory, SAVEFILE_NAME);
    if (foreign) *foreign = false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t got = fread(img, 1, SAVEFILE_TOTAL, f);
    fclose(f);
    if (got == SAVEFILE_TOTAL && memcmp(img, SAVEFILE_MAGIC, 8) == 0) return true;
    if (foreign) *foreign = true;
    return false;
}

/* Seção idx (0 ou 1) -> buf (2048 bytes). false = não há dado salvo (use os padrões). */
bool SaveFile_Read(int idx, uint8_t* buf) {
    uint8_t img[SAVEFILE_TOTAL];
    if (idx < 0 || idx > 1) return false;
    if (saveFileLoad(img, NULL) && (img[0x0C] & (1u << idx))) {
        memcpy(buf, img + SAVEFILE_HDR + idx * SAVEFILE_SECTION, SAVEFILE_SECTION);
        return true;
    }
    /* migração: arquivo antigo desta seção */
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/%s", g_game.currentDirectory, k_saveLegacy[idx]);
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t got = fread(buf, 1, SAVEFILE_SECTION, f);
    fclose(f);
    if (got != SAVEFILE_SECTION) return false;
    Log_Print("SAVE: secao %d importada de '%s' (passa a ficar em %s)\n", idx, k_saveLegacy[idx], SAVEFILE_NAME);
    SaveFile_Write(idx, buf);
    return true;
}

/* buf (2048 bytes) -> seção idx; a outra seção é preservada. */
bool SaveFile_Write(int idx, const uint8_t* buf) {
    uint8_t img[SAVEFILE_TOTAL];
    char path[MAX_PATH];
    bool foreign = false;
    if (idx < 0 || idx > 1) return false;
    snprintf(path, sizeof(path), "%s/%s", g_game.currentDirectory, SAVEFILE_NAME);
    if (!saveFileLoad(img, &foreign)) {
        if (foreign) {   /* não é nosso: guarda antes de criar o do port */
            char bak[MAX_PATH];
            snprintf(bak, sizeof(bak), "%s.orig", path);
            if (rename(path, bak) != 0) {
                Log_Print("SAVE: '%s' nao e do port e nao foi possivel renomear; nada gravado\n", path);
                return false;
            }
            Log_Print("SAVE: '%s' nao e do port; guardado como '%s'\n", path, bak);
        }
        memset(img, 0, sizeof(img));
        memcpy(img, SAVEFILE_MAGIC, 8);
        img[0x08] = 1;
    }
    memcpy(img + SAVEFILE_HDR + idx * SAVEFILE_SECTION, buf, SAVEFILE_SECTION);
    img[0x0C] |= (uint8_t)(1u << idx);
    FILE* f = fopen(path, "wb");
    if (!f) {
        Log_Print("SAVE: cannot write '%s'\n", path);
        return false;
    }
    fwrite(img, 1, sizeof(img), f);
    fclose(f);
    return true;
}

static void eepToGame(void) {
    g_game.svcGameMode     = g_eep[O_MODE];
    g_game.optionDifficulty = g_eep[O_LEVEL];
    g_game.optionToggle1   = g_eep[O_STAGEBRK];
    g_game.svcLangOption   = g_eep[O_LANG];
    g_game.svcDemoSound    = g_eep[O_DEMO];
    g_game.optionToggle2   = g_eep[O_HELP];
    g_game.svcCoin1        = g_eep[O_COIN1];
    g_game.svcCoin2        = g_eep[O_COIN2];
    g_game.svcCoin1Total   = (int)eepGet32(O_COIN1TOT);
    g_game.svcCoin2Total   = (int)eepGet32(O_COIN2TOT);
    g_game.svcCoinTotal    = (int)eepGet32(O_COINTOT);
    g_game.svcServiceTotal = (int)eepGet32(O_SVCTOT);
}

static void eepFromGame(void) {
    g_eep[O_MODE]     = (uint8_t)g_game.svcGameMode;
    g_eep[O_LEVEL]    = (uint8_t)g_game.optionDifficulty;
    g_eep[O_STAGEBRK] = (uint8_t)g_game.optionToggle1;
    g_eep[O_LANG]     = (uint8_t)g_game.svcLangOption;
    g_eep[O_DEMO]     = (uint8_t)g_game.svcDemoSound;
    g_eep[O_HELP]     = (uint8_t)g_game.optionToggle2;
    g_eep[O_COIN1]    = (uint8_t)g_game.svcCoin1;
    g_eep[O_COIN2]    = (uint8_t)g_game.svcCoin2;
    eepPut32(O_COIN1TOT, (uint32_t)g_game.svcCoin1Total);
    eepPut32(O_COIN2TOT, (uint32_t)g_game.svcCoin2Total);
    eepPut32(O_COINTOT,  (uint32_t)g_game.svcCoinTotal);
    eepPut32(O_SVCTOT,   (uint32_t)g_game.svcServiceTotal);
    eepStamp();
}

/* Grava a imagem (PUMPY.EXE 0x405190). */
void Eeprom_Save(void) {
    eepFromGame();
    /* era: fopen("pumpprex3.ini", "wb") + fwrite(g_eep) */
    SaveFile_Write(0, g_eep);
}

/* Lê e valida a imagem (PUMPY.EXE 0x4067a0) e aplica em g_game.
 * Retorno: 1 = arquivo válido; 0 = arquivo inválido (resetado, com o aviso do original);
 *          -1 = arquivo ausente (defaults em g_game; quem chamou decide migrar/gravar). */
int Eeprom_Load(void) {
    Log_Print("EEPROM: loading %s (secao 0)\n", SAVEFILE_NAME);

    /* era: fopen("pumpprex3.ini", "rb") + fread(g_eep) */
    if (!SaveFile_Read(0, g_eep)) {
        eepDefaults();
        eepToGame();
        return -1;
    }

    if (eepGet32(EEP_IDENT) != EEP_IDENT_VAL) {
        Log_Print("Warning - ident mismatch\n");
        eepDefaults();
        eepToGame();
        Eeprom_Save();               /* o original reseta e regrava neste caso */
        return 0;
    }
    if (eepGet32(EEP_CHKSUM) != eepAdler32(&g_eep[EEP_SETTINGS], EEP_SETLEN)) {
        Log_Print("Warning - EEPROM chksum err\n");
        eepDefaults();               /* e não regrava, como o original */
        eepToGame();
        return 0;
    }
    eepToGame();
    return 1;
}
