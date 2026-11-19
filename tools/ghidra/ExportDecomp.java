// Ghidra headless script: decompile every ZiGGURAT game/engine ObjC method (and the C helpers they
// call) into <outdir>/<layer>/<Class>/<selector>.c plus index.tsv.
// analyzeHeadless <proj_dir> <proj> -import re/Zig2_armv7 -postScript ExportDecomp.java <outdir> -scriptPath tools/ghidra
//@category ZiGGURAT
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

public class ExportDecomp extends GhidraScript {
    static final Set<String> GAME = new HashSet<>(Arrays.asList(("Alien AppDelegate BGManager BigAlien Bullet EnemyBullet Explosion GameOverState Player PlayerBullet PlayState OrangeAlien UFO YellowAlien ZigguratIOS GibCluster StartState Statistics StatState MoonRock SmallExpEmitter RedAlien TutorialState OptionsState PurpleAlien BigPurpleAlien CreditsState").split(" ")));
    static final Set<String> ENGINE = new HashSet<>(Arrays.asList(("FlashFunction FlashObject FlashPoint FlashRectangle FlxAnim FlxButton FlxEmitter FlxFade FlxFlash FlxG FlxGLView FlxGame FlxGroup FlxList FlxManagedSprite FlxObject FlxPoint FlxQuadTree FlxQuake FlxRect FlxSave FlxSound FlxSprite FlxState FlxText FlxTouches FlxU FlxTileblock SemiSecretFont SemiSecretText SemiSecretTexture FlixelFont").split(" ")));

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args.length > 0 ? args[0] : "decomp");
        out.mkdirs();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        FunctionManager fm = currentProgram.getFunctionManager();
        List<String> index = new ArrayList<>();
        Set<Function> helpers = new TreeSet<>(Comparator.comparing(Function::getEntryPoint));
        PrintWriter all = new PrintWriter(new OutputStreamWriter(new FileOutputStream(new File(out, "all_function_names.txt")), StandardCharsets.UTF_8));
        int ok = 0, fail = 0;
        for (Function f : fm.getFunctions(true)) {
            String ns = f.getParentNamespace() == null ? "" : f.getParentNamespace().getName();
            String nm = f.getName();
            all.println(f.getEntryPoint() + "\t" + ns + "\t" + nm);
            String cls = ns, sel = nm;
            // also accept "-[Class sel]" style names
            if (nm.matches("^[-+]\\[\\w+ .+\\]$")) {
                cls = nm.substring(2, nm.indexOf(' '));
                sel = nm.substring(nm.indexOf(' ') + 1, nm.length() - 1);
            }
            String layer = GAME.contains(cls) ? "game" : ENGINE.contains(cls) ? "engine" : null;
            if (layer == null) continue;
            File d = new File(new File(out, layer), cls);
            d.mkdirs();
            String file = sel.replace(":", "_").replaceAll("_+$", "");
            if (file.isEmpty()) file = "_";
            String text;
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            if (r != null && r.decompileCompleted()) { text = r.getDecompiledFunction().getC(); ok++; }
            else { text = "// DECOMPILATION FAILED: " + (r == null ? "null" : r.getErrorMessage()) + "\n"; fail++; }
            long size = f.getBody().getNumAddresses();
            try (Writer w = new OutputStreamWriter(new FileOutputStream(new File(d, file + ".c")), StandardCharsets.UTF_8)) {
                w.write("// [" + cls + " " + sel + "]  ea=" + f.getEntryPoint() + " size=" + size + "\n");
                w.write(text);
            }
            index.add(layer + "\t" + cls + "\t" + sel + "\t" + f.getEntryPoint() + "\t" + size);
            for (Function c : f.getCalledFunctions(monitor)) {
                if (c.getName().startsWith("FUN_") && !c.isThunk()) helpers.add(c);
            }
        }
        all.close();
        File hd = new File(out, "helpers");
        hd.mkdirs();
        for (Function h : helpers) {
            DecompileResults r = di.decompileFunction(h, 120, monitor);
            String text = (r != null && r.decompileCompleted()) ? r.getDecompiledFunction().getC() : "// FAILED\n";
            try (Writer w = new OutputStreamWriter(new FileOutputStream(new File(hd, h.getName() + ".c")), StandardCharsets.UTF_8)) {
                w.write(text);
            }
        }
        Collections.sort(index);
        try (Writer w = new OutputStreamWriter(new FileOutputStream(new File(out, "index.tsv")), StandardCharsets.UTF_8)) {
            for (String s : index) w.write(s + "\n");
        }
        println("EXPORT DONE ok=" + ok + " fail=" + fail + " helpers=" + helpers.size());
    }
}
