# IDAPython batch script: decompile all game + engine ObjC methods to re/decomp/<Class>/<selector>.c
# Usage: idat.exe -A -c -L<log> -S"tools/ida_export.py <outdir>" re/Zig2_armv7
import os, re, sys
import idc, idaapi, idautils, ida_auto, ida_hexrays, ida_pro, ida_funcs, ida_name

GAME = set("Alien AppDelegate BGManager BigAlien Bullet EnemyBullet Explosion GameOverState Player PlayerBullet PlayState OrangeAlien UFO YellowAlien ZigguratIOS GibCluster StartState Statistics StatState MoonRock SmallExpEmitter RedAlien TutorialState OptionsState PurpleAlien BigPurpleAlien CreditsState".split())
ENGINE = set("FlashFunction FlashObject FlashPoint FlashRectangle FlxAnim FlxButton FlxEmitter FlxFade FlxFlash FlxG FlxGLView FlxGame FlxGroup FlxList FlxManagedSprite FlxObject FlxPoint FlxQuadTree FlxQuake FlxRect FlxSave FlxSound FlxSprite FlxState FlxText FlxTouches FlxU FlxTileblock SemiSecretFont SemiSecretText SemiSecretTexture FlixelFont".split())

out = idc.ARGV[1] if len(idc.ARGV) > 1 else "decomp"
ida_auto.auto_wait()
if not ida_hexrays.init_hexrays_plugin():
    print("NO HEXRAYS"); ida_pro.qexit(2)

pat = re.compile(r"^([-+])\[(\w+)(?:\((\w+)\))? (.+)\]$")
index = []
callees = set()
n_ok = n_fail = 0
for ea in idautils.Functions():
    name = ida_name.get_name(ea)
    dn = ida_name.demangle_name(name, 0) or name
    m = pat.match(dn) or pat.match(name)
    if not m:
        continue
    kind, cls, cat, sel = m.groups()
    layer = "game" if cls in GAME else "engine" if cls in ENGINE else None
    if not layer:
        continue
    d = os.path.join(out, layer, cls)
    os.makedirs(d, exist_ok=True)
    fn = (("C_" if kind == "+" else "") + sel.replace(":", "_")).rstrip("_") or "_"
    try:
        cf = ida_hexrays.decompile(ea)
        text = str(cf)
        n_ok += 1
    except Exception as e:
        text = "// DECOMPILATION FAILED: %s\n" % e
        n_fail += 1
    f = ida_funcs.get_func(ea)
    size = f.end_ea - f.start_ea
    with open(os.path.join(d, fn + ".c"), "w", encoding="utf-8") as fh:
        fh.write("// %s[%s %s]  ea=0x%X size=%d\n" % (kind, cls, sel, ea, size))
        fh.write(text)
    index.append("%s\t%s\t%s%s\t0x%X\t%d" % (layer, cls, kind, sel, ea, size))
    # record non-objc callees (C helpers) for later export
    for item in idautils.FuncItems(ea):
        for x in idautils.CodeRefsFrom(item, 0):
            cn = ida_name.get_name(x)
            if cn.startswith("sub_"):
                callees.add(x)

hd = os.path.join(out, "helpers")
os.makedirs(hd, exist_ok=True)
for ea in sorted(callees):
    try:
        text = str(ida_hexrays.decompile(ea))
    except Exception as e:
        text = "// FAILED %s" % e
    with open(os.path.join(hd, "sub_%X.c" % ea), "w", encoding="utf-8") as fh:
        fh.write(text)

with open(os.path.join(out, "index.tsv"), "w") as fh:
    fh.write("\n".join(sorted(index)))
print("DONE ok=%d fail=%d helpers=%d" % (n_ok, n_fail, len(callees)))
idc.save_database(idc.get_idb_path(), 0)
ida_pro.qexit(0)
