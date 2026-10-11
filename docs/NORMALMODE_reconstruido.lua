-- NORMALMODE.LUA do Pump It Up Zero (game/SCRIPT/NORMALMODE.LUA), reconstruído à mão.
--
-- O arquivo original NÃO é criptografado: é Lua 5.0 compilado (bytecode, começa com
-- "\27LuaP"). Este texto foi refeito a partir das instruções, que estão listadas uma a
-- uma em docs/NORMALMODE_listagem.txt (gerado por tools/lua50_list.py).
--
-- Confirmado pelo bytecode: nomes de globais e de funções, todas as constantes e o fluxo.
-- NÃO vem do arquivo (o compilador descarta): nomes de parâmetros e de variáveis locais,
-- comentários e a formatação. Os nomes de parâmetros abaixo são sugestões minhas.
-- Os números de linha entre parênteses são os do fonte original, guardados no bytecode.

require("DEFINE.LUA")   -- JUDGE_NONE=0 PERFECT=1 GREAT=2 GOOD=3 BAD=4 MISS=5 ... JUDGE_USER=9

SCORE_PERFECT          = 1000
SCORE_GREAT            = 500
SCORE_GOOD             = 100
SCORE_BAD              = -200
SCORE_MISS             = -500
SCORE_MISS_LONGNOTE    = -300
SCORE_COMBO            = 0
SCORE_NIGHTMARE_BONUS  = 500000
SCORE_GRADE_S          = 100000
SCORE_GRADE_A          = 0
SCORE_GRADE_B          = 0
SCORE_GRADE_C          = 0
SCORE_GRADE_D          = 0
SCORE_GRADE_F          = 0

GAUGE_MIN      = 0
GAUGE_MAX      = 1000
GAUGE_DEFAULT  = 500
GAUGE_PERFECT  = 12
GAUGE_GREAT    = 10
GAUGE_GOOD     = 0
GAUGE_BAD      = -50
GAUGE_MISS     = -500

-- ZONAS DE JULGAMENTO (a "precisão"). Uma coluna por nível de julgamento do SETUP:
--                     EASY  NORMAL  HARD
-- U = lado de cima da step zone (nota já passou = pisou TARDE), valores negativos.
-- D = lado de baixo (nota ainda não chegou = pisou CEDO), valores positivos.
-- Unidade: Y do jogo, 60 unidades por batida. O executável multiplica cada valor por
-- (BPM do bloco / 120) antes de comparar com a posição da nota (piu 0x0808ecc0); como a
-- nota anda BPM unidades por segundo, em tempo isso dá sempre valor/120 segundos.
PERFECT_ZONE_U = { -12, -10,  -8 }   -- tarde: 100,0 /  83,3 /  66,7 ms
PERFECT_ZONE_D = {   7,   5,   3 }   -- cedo:   58,3 /  41,7 /  25,0 ms
GREAT_ZONE_U   = { -17, -15, -13 }   -- tarde: 141,7 / 125,0 / 108,3 ms
GREAT_ZONE_D   = {  12,  10,   8 }   -- cedo:  100,0 /  83,3 /  66,7 ms
GOOD_ZONE_U    = { -22, -20, -18 }   -- tarde: 183,3 / 166,7 / 150,0 ms
GOOD_ZONE_D    = {  17,  15,  13 }   -- cedo:  141,7 / 125,0 / 108,3 ms
BAD_ZONE_U     = { -27, -25, -23 }   -- tarde: 225,0 / 208,3 / 191,7 ms
BAD_ZONE_D     = {  22,  20,  18 }   -- cedo:  183,3 / 166,7 / 150,0 ms

gauge_factor_min   = 0
gauge_factor_max   = 0
gauge_miss_factor  = 0
gauge_factor       = 0
gauge_meter        = 0
life_gauge_status  = 0
Player             = 0
DefaultHighSpeed   = 1000

-- (linha 63) Valores iniciais da barra de vida. Lê a GLOBAL "level", não o parâmetro.
function ResetData(arg)
    gauge_meter = GAUGE_DEFAULT
    if level == 0 then
        gauge_factor_min  = 200
        gauge_factor_max  = 1000
        gauge_miss_factor = -700
        gauge_factor      = 500
    elseif level == 1 then
        gauge_factor_min  = 0
        gauge_factor_max  = 800
        gauge_miss_factor = -700
        gauge_factor      = 100
    elseif level == 2 then
        gauge_factor_min  = 100
        gauge_factor_max  = 900
        gauge_miss_factor = -700
        gauge_factor      = 300
    else
        gauge_factor_min  = 0
        gauge_factor_max  = 800
        gauge_miss_factor = -700
        gauge_factor      = 100
    end
    DefaultHighSpeed = 1000
end

-- (linha 90) Pontos de um julgamento.
--   judge    = JUDGE_xxx (ver DEFINE.LUA)
--   combo    = combo atual
--   arrows   = (sugestão) quantidade de setas na linha: 3 vale x1.5, 4 ou mais vale x2
--   longnote = 1 quando o MISS é de nota longa
function GetScore(judge, combo, arrows, longnote)
    local score
    if judge == JUDGE_NONE then
        return 0
    end
    if judge == JUDGE_PERFECT then
        score = SCORE_PERFECT
    elseif judge == JUDGE_GREAT then
        score = SCORE_GREAT
    elseif judge == JUDGE_GOOD then
        score = SCORE_GOOD
    elseif judge == JUDGE_BAD then
        score = SCORE_BAD
    elseif judge == JUDGE_MISS then
        if longnote == 1 then
            score = SCORE_MISS_LONGNOTE
        else
            score = SCORE_MISS
        end
    end
    if combo >= 51 then
        if judge == JUDGE_PERFECT then
            score = score + 1000
        elseif judge == JUDGE_GREAT then
            score = score + 1000
        end
    end
    if judge == JUDGE_PERFECT or judge == JUDGE_GREAT then
        if arrows >= 4 then
            score = score * 2
        elseif arrows >= 3 then
            score = score * 1.5
        end
    end
    return score
end

-- (linha 134)
function SetDefaultHighSpeed(speed)
    DefaultHighSpeed = speed
end

-- (linha 139)
function SetPlayerNum(player)
    Player = player
end

-- (linha 143)
function SendMessage(a, b, c, d)
    SendMessageToPlayer(Player, a, b, c, d)
end

-- (linha 147)
function GetLifeGaugeMeter()
    return gauge_meter
end

-- (linha 151)
function GetLifeGaugeStatus()
    return life_gauge_status
end

-- (linha 156) Julgamentos "do usuário" (JUDGE_USER = 9 em diante): itens e minas.
function SetJudge(judge)
    if judge >= JUDGE_USER then
        UserJudge = judge - JUDGE_USER + 1
        if UserJudge == 6 then
            if gauge_meter >= 1000 then
                gauge_meter = 1000
            end
            gauge_meter  = gauge_meter - 500
            gauge_factor = gauge_factor + gauge_miss_factor
        elseif UserJudge == 11 then
            ItemProc(Player, ITEM_LIST_VELOCITY, 0, 0)
        elseif UserJudge == 13 then
            ItemProc(Player, ITEM_LIST_VELOCITY, 4, 0)
        elseif UserJudge == 14 then
            ItemProc(Player, ITEM_LIST_VELOCITY, 1, 0)
        elseif UserJudge == 15 then
            ItemProc(Player, ITEM_LIST_VELOCITY, 2, 0)
        end
    end
end

-- (linha 187) Efeito de cada julgamento na barra de vida.
function SetJudgeGauge(judge)
    if judge == JUDGE_PERFECT then
        gauge_meter  = gauge_meter + GAUGE_PERFECT * gauge_factor / 1000
        gauge_factor = gauge_factor + 20
    elseif judge == JUDGE_GREAT then
        gauge_meter  = gauge_meter + GAUGE_GREAT * gauge_factor / 1000
        gauge_factor = gauge_factor + 16
    elseif judge == JUDGE_GOOD then
        -- nada
    elseif judge == JUDGE_BAD then
        gauge_meter  = gauge_meter + GAUGE_BAD
        gauge_factor = gauge_factor + gauge_miss_factor / 2
    elseif judge == JUDGE_MISS then
        gauge_meter  = gauge_meter + GAUGE_MISS * gauge_meter / 2000 - 20
        gauge_factor = gauge_factor + gauge_miss_factor
    end
    if gauge_factor < gauge_factor_min then
        gauge_factor = gauge_factor_min
    elseif gauge_factor > gauge_factor_max then
        gauge_factor = gauge_factor_max
    end
end

-- (linha 208) Devolve os dois limites (U, D) da zona pedida.
--   judge = JUDGE_PERFECT .. JUDGE_MISS;  level = 0 EASY, 1 NORMAL, 2 HARD
-- O executável chama com judge = 1..4 e o nível do SETUP (piu 0x0808e8c0).
-- MISS_ZONE_U / MISS_ZONE_D não são definidas neste arquivo (o ramo do MISS não é usado).
function GetJudgeZone(judge, level)
    if judge == JUDGE_PERFECT then
        return PERFECT_ZONE_U[level + 1], PERFECT_ZONE_D[level + 1]
    elseif judge == JUDGE_GREAT then
        return GREAT_ZONE_U[level + 1], GREAT_ZONE_D[level + 1]
    elseif judge == JUDGE_GOOD then
        return GOOD_ZONE_U[level + 1], GOOD_ZONE_D[level + 1]
    elseif judge == JUDGE_BAD then
        return BAD_ZONE_U[level + 1], BAD_ZONE_D[level + 1]
    elseif judge == JUDGE_MISS then
        return MISS_ZONE_U[level + 1], MISS_ZONE_D[level + 1]
    end
end

-- (linha 224) vazia no original
function MSGPROC(a, b, c, d)
end

-- (linha 228)
function GetItemSlot(arg)
    return 1
end

-- (linha 233) vazia no original
function OnTick(arg)
end
