import sys, struct, collections
sys.path.insert(0, sys.argv[2])
import macho
s=[x for x in macho.load(sys.argv[1]) if x.name==sys.argv[3]][0]
# function starts
off=28; fs=None
magic,cpu,sub,ft,ncmds,szc,fl=struct.unpack_from("<7I",s.d,0)
for _ in range(ncmds):
    cmd,sz=struct.unpack_from("<2I",s.d,off)
    if cmd==0x26: fs=struct.unpack_from("<2I",s.d,off+8)
    off+=sz
dataoff,datasize=fs
text_va=[x for x in s.segs if x[0]=='__TEXT'][0][1]
addr=text_va; starts=[]; i=dataoff
while i<dataoff+datasize:
    v=0;sh=0
    while True:
        b=s.d[i];i+=1;v|=(b&0x7f)<<sh;sh+=7
        if b<0x80:break
    if v==0:break
    addr+=v; starts.append(addr)
ta,tsz,_=s.sects[('__TEXT','__text')]
starts.append(ta+tsz)
starts=sorted(set(a&~1 for a in starts))
size_of={}
for a,b in zip(starts,starts[1:]): size_of[a]=b-a
print("functions:",len(starts)-1,"text bytes:",tsz)
GAME="Alien AppDelegate BGManager BigAlien Bullet EnemyBullet Explosion GameOverState Player PlayerBullet PlayState OrangeAlien UFO YellowAlien ZigguratIOS GibCluster StartState Statistics StatState MoonRock SmallExpEmitter RedAlien TutorialState OptionsState PurpleAlien BigPurpleAlien CreditsState".split()
ENGINE="FlashFunction FlashObject FlashPoint FlashRectangle FlxAnim FlxButton FlxEmitter FlxFade FlxFlash FlxG FlxGLView FlxGame FlxGroup FlxList FlxManagedSprite FlxObject FlxPoint FlxQuadTree FlxQuake FlxRect FlxSave FlxSound FlxSprite FlxState FlxText FlxTouches FlxU FlxTileblock SemiSecretFont SemiSecretText SemiSecretTexture FlixelFont".split()
tot=collections.Counter(); cnt=collections.Counter(); per={}
thumb=collections.Counter()
for c in s.objc_classes():
    grp='game' if c['name'] in GAME else 'engine' if c['name'] in ENGINE else 'sdk'
    n=0
    for nm,ty,imp in c['imethods']+c['cmethods']:
        thumb[imp&1]+=1
        a=imp&~1
        z=size_of.get(a,0); tot[grp]+=z; cnt[grp]+=1; n+=z
    if grp!='sdk': per[c['name']]=(grp,n,len(c['imethods'])+len(c['cmethods']))
print("thumb vs arm method IMPs:",dict(thumb))
for g in ('game','engine','sdk'): print(g, "methods",cnt[g],"bytes",tot[g])
for k,(g,n,m) in sorted(per.items(), key=lambda x:-x[1][1]): print(f"  {g:6} {k:20} {n:7d} bytes  {m:3d} methods")
