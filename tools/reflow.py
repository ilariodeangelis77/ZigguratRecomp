"""Re-indent joined pseudo-C: newline after ; { } (outside parentheses)."""
import sys
src = open(sys.argv[1], encoding='utf-8').read()
out, depth, par, line = [], 0, 0, ''
def flush():
    global line
    s = line.strip()
    if s: out.append('  ' * depth + s)
    line = ''
for ch in src:
    if ch == '\n':
        flush(); continue
    if ch == '(':
        par += 1
    elif ch == ')':
        par -= 1
    if par == 0 and ch == '{':
        line += ' {'; flush(); depth += 1; continue
    if par == 0 and ch == '}':
        flush(); depth = max(0, depth - 1); line = '}'; flush(); continue
    line += ch
    if par == 0 and ch == ';':
        flush()
flush()
print('\n'.join(out))
