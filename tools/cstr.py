import sys
sys.path.insert(0, sys.argv[2])
import macho
s=[x for x in macho.load(sys.argv[1]) if x.name=='armv7'][0]
a,sz,fo=s.sects[('__TEXT','__cstring')]
raw=s.d[fo:fo+sz]
out=open(sys.argv[3],'w',encoding='utf-8')
for i,t in enumerate(raw.split(b'\0')):
    out.write(f"{i}\t{t.decode('utf-8','replace')}\n")
