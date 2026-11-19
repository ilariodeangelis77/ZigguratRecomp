// List every function that references a given ObjC selector (via __objc_selrefs).
// analyzeHeadless ... -process Zig2_armv7 -noanalysis -readOnly -postScript FindSelRefs.java <outfile> <selector>...
//@category ZiGGURAT
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class FindSelRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Memory mem = currentProgram.getMemory();
        MemoryBlock selrefs = mem.getBlock("__objc_selrefs");
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            for (int i = 1; i < a.length; i++) {
                String sel = a[i];
                w.println("== " + sel);
                for (Address p = selrefs.getStart(); p.compareTo(selrefs.getEnd()) < 0; p = p.add(4)) {
                    Address target = toAddr(mem.getInt(p) & 0xffffffffL);
                    String s;
                    try {
                        byte[] buf = new byte[sel.length() + 1];
                        mem.getBytes(target, buf);
                        s = new String(buf, 0, sel.length(), "UTF-8");
                        if (buf[sel.length()] != 0 || !s.equals(sel)) continue;
                    } catch (Exception e) { continue; }
                    for (Reference r : getReferencesTo(p)) {
                        Function f = getFunctionContaining(r.getFromAddress());
                        w.println("  " + r.getFromAddress() + "  " + (f == null ? "?" : f.getName(true)));
                    }
                }
            }
        }
    }
}
