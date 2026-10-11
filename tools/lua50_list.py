"""Listagem legivel de um .LUA compilado (Lua 5.0) do Pump It Up Zero.
Uso: lua50_list.py <arquivo.LUA>
Cada instrucao sai com os operandos resolvidos (nomes de globais e constantes).
"""
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lua50_dump as L

K = L.MAXSTACK

def fmtk(v):
    if v is None: return 'nil'
    if isinstance(v, float):
        return str(int(v)) if v == int(v) else repr(v)
    return '"%s"' % v

def listing(f, name, out, depth=0):
    k = f['k']
    ind = '    ' * depth
    rk = lambda x: fmtk(k[x - K]) if x >= K else 'R%d' % x
    out.append('%s-- %s  (linha %d do fonte original, %d parametros: %s)' % (
        ind, name, f['line'], f['npar'], ', '.join('R%d' % i for i in range(f['npar'])) or '-'))
    code = f['code']
    for pc, ins in enumerate(code):
        op, a, b, c, bx = L.dec(ins)
        n = L.OPS[op] if op < len(L.OPS) else 'OP%d' % op
        sbx = bx - 131071
        if n == 'MOVE': t = 'R%d = R%d' % (a, b)
        elif n == 'LOADK': t = 'R%d = %s' % (a, fmtk(k[bx]))
        elif n == 'LOADBOOL': t = 'R%d = %s' % (a, 'true' if b else 'false') + ('; pula 1' if c else '')
        elif n == 'LOADNIL': t = 'R%d..R%d = nil' % (a, b)
        elif n == 'GETGLOBAL': t = 'R%d = %s' % (a, k[bx])
        elif n == 'SETGLOBAL': t = '%s = R%d' % (k[bx], a)
        elif n == 'GETTABLE': t = 'R%d = R%d[%s]' % (a, b, rk(c))
        elif n == 'SETTABLE': t = 'R%d[%s] = %s' % (a, rk(b), rk(c))
        elif n == 'NEWTABLE': t = 'R%d = {}' % a
        elif n in ('ADD', 'SUB', 'MUL', 'DIV', 'POW'):
            t = 'R%d = %s %s %s' % (a, rk(b), {'ADD': '+', 'SUB': '-', 'MUL': '*', 'DIV': '/', 'POW': '^'}[n], rk(c))
        elif n == 'UNM': t = 'R%d = -R%d' % (a, b)
        elif n == 'NOT': t = 'R%d = not R%d' % (a, b)
        elif n == 'CONCAT': t = 'R%d = R%d .. ... .. R%d' % (a, b, c)
        elif n == 'JMP': t = 'vai para %d' % (pc + 1 + sbx)
        elif n in ('EQ', 'LT', 'LE'):
            sym = {'EQ': '==', 'LT': '<', 'LE': '<='}[n]
            t = 'se (%s %s %s) %s entao pula a proxima instrucao' % (rk(b), sym, rk(c), '~=' if a else '==')
            t = 'se (%s %s %s) for %s, pula a proxima instrucao' % (rk(b), sym, rk(c), 'falso' if a else 'verdadeiro')
        elif n == 'TEST': t = 'se R%d for %s: R%d = R%d; senao pula a proxima' % (b, 'verdadeiro' if c else 'falso', a, b)
        elif n in ('CALL', 'TAILCALL'):
            args = 'ate o topo' if b == 0 else ', '.join('R%d' % (a + 1 + i) for i in range(b - 1))
            rets = 'varios' if c == 0 else (', '.join('R%d' % (a + i) for i in range(c - 1)) or 'nada')
            t = '%s = R%d(%s)' % (rets, a, args) if rets != 'nada' else 'R%d(%s)' % (a, args)
        elif n == 'RETURN':
            t = 'retorna ' + ('ate o topo' if b == 0 else (', '.join('R%d' % (a + i) for i in range(b - 1)) or '(nada)'))
        elif n == 'SETLIST': t = 'R%d[1..%d] = R%d..R%d   (preenche a tabela)' % (a, bx % 32 + 1, a + 1, a + bx % 32 + 1)
        elif n == 'CLOSURE': t = 'R%d = funcao #%d' % (a, bx)
        else: t = 'A=%d B=%d C=%d' % (a, b, c)
        out.append('%s  %3d  %-9s %s' % (ind, pc, n, t))
    for i, p in enumerate(f['p']):
        out.append('')
        listing(p, 'funcao #%d' % i, out, depth)

def main():
    b = open(sys.argv[1], 'rb').read()
    r = L.R(b); r.p = 4 + 10 + 8
    f = L.func(r)
    out = []
    listing(f, 'bloco principal', out)
    print('\n'.join(out))

main()
