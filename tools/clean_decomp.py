"""Make Ghidra ARM/softfp pseudo-C more readable for porting.
Usage: python -I tools/clean_decomp.py <in_dir> <out_dir>
Rewrites (purely textual, semantics-preserving for reading):
  CONCAT44(hi, x)                 -> x          (hi half is register-pair noise for scalar VFP ops)
  FloatVectorMult/Add/Sub/Div(a,b,..) -> (a * b) etc.
  FixedToFP(x, ...)               -> (float)(x)
  FPToFixed(x, ...)               -> (int)(x)
  PTR_s_sel_name_00xxxxxx         -> @sel_name
  PTR__OBJC_CLASS___Name_00xxxxxx -> Name ;  PTR_Name_00xxxxxx (class refs) -> Name
  *(int *)PTR_ivar_00xxxxxx       -> OFF(ivar)  (engine ivar offsets)
  param_1 -> self ; param_2 -> _cmd
and drops variable declarations and `uVarN = (undefined4)((ulonglong)... >> 0x20);` noise lines."""
import os, re, sys

DECL = re.compile(r'^\s*(undefined[0-9]*|ID|int|uint|float|double|char|bool|SEL|ulonglong|longlong|short|ushort|byte|objc_super|CGPoint|code|void|OFHighScore)\s*\**\s*[a-zA-Z_][a-zA-Z0-9_]*(\s*\[[0-9]+\])?;\s*$')
NOISE = re.compile(r'^\s*\w+ = \((undefined4|int|float)\)\(\(ulonglong\)\w+ >> 0x20\);\s*$')


def find_args(s, start):
    """s[start] == '(' ; return (list_of_arg_strings, end_index_after_close)."""
    depth, i, args, cur = 0, start, [], []
    while i < len(s):
        c = s[i]
        if c == '(':
            depth += 1
            if depth > 1:
                cur.append(c)
        elif c == ')':
            depth -= 1
            if depth == 0:
                args.append(''.join(cur).strip())
                return args, i + 1
            cur.append(c)
        elif c == ',' and depth == 1:
            args.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    return None, start


FUNCS = {
    'CONCAT44': lambda a: a[1],
    'FloatVectorMult': lambda a: '(%s * %s)' % (a[0], a[1]),
    'FloatVectorAdd': lambda a: '(%s + %s)' % (a[0], a[1]),
    'FloatVectorSub': lambda a: '(%s - %s)' % (a[0], a[1]),
    'FloatVectorDiv': lambda a: '(%s / %s)' % (a[0], a[1]),
    'FixedToFP': lambda a: '(float)(%s)' % a[0],
    'FPToFixed': lambda a: '(int)(%s)' % a[0],
    'FloatVectorNeg': lambda a: '(-%s)' % a[0],
    'FloatVectorAbs': lambda a: 'fabsf(%s)' % a[0],
}
FUNC_RE = re.compile(r'\b(' + '|'.join(FUNCS) + r')\(')


def rewrite_funcs(s):
    for _ in range(50):
        m = FUNC_RE.search(s)
        if not m:
            break
        # innermost-first is not required; process leftmost, recursion handles nesting via loop
        args, end = find_args(s, m.end() - 1)
        if args is None:
            break
        s = s[:m.start()] + FUNCS[m.group(1)](args) + s[end:]
    return s


def clean_text(t):
    out = []
    for line in t.split('\n'):
        if DECL.match(line) or NOISE.match(line):
            continue
        out.append(line)
    t = '\n'.join(out)
    # join multi-line statements so nested calls can be rewritten
    t = re.sub(r'\n\s{6,}', ' ', t)
    t = rewrite_funcs(t)
    t = re.sub(r'PTR_s_(\w+?)_00[0-9a-f]{6}', r'@\1', t)
    t = re.sub(r'\(ID\)PTR__OBJC_CLASS___(\w+?)_00[0-9a-f]{6}', r'\1', t)
    t = re.sub(r'PTR__OBJC_CLASS___(\w+?)_00[0-9a-f]{6}', r'\1', t)
    t = re.sub(r'\*\(int \*\)PTR_(\w+?)_00[0-9a-f]{6}', r'OFF(\1)', t)
    t = re.sub(r'\(ID\)PTR_(\w+?)_00[0-9a-f]{6}', r'\1', t)
    t = re.sub(r'\bparam_1\b', 'self', t)
    t = re.sub(r'\bparam_2\b', '_cmd', t)
    t = re.sub(r'\(ulonglong\)', '', t)
    return t


def main(src, dst):
    n = 0
    for dp, _, files in os.walk(src):
        for fn in files:
            if not fn.endswith('.c'):
                continue
            p = os.path.join(dp, fn)
            rel = os.path.relpath(p, src)
            o = os.path.join(dst, rel)
            os.makedirs(os.path.dirname(o), exist_ok=True)
            open(o, 'w', encoding='utf-8').write(clean_text(open(p, encoding='utf-8').read()))
            n += 1
    print('cleaned', n)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
