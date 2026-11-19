// Dump disassembly (with resolved float literal-pool values) of functions at given addresses.
// analyzeHeadless <proj_dir> <proj> -process Zig2_armv7 -noanalysis -postScript DumpAsm.java <outfile> <addr>...
//@category ZiGGURAT
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import java.io.*;

public class DumpAsm extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            for (int i = 1; i < a.length; i++) {
                Address addr = toAddr(a[i]);
                Function f = getFunctionContaining(addr);
                w.println("==== " + (f == null ? a[i] : f.getName(true)) + " @ " + addr);
                if (f == null) continue;
                for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                    StringBuilder extra = new StringBuilder();
                    for (int op = 0; op < ins.getNumOperands(); op++) {
                        for (Object o : ins.getOpObjects(op)) {
                            if (o instanceof Address) {
                                Address ref = (Address) o;
                                try {
                                    int bits = currentProgram.getMemory().getInt(ref);
                                    extra.append(String.format("  [%s]=0x%08x (f=%g)", ref, bits, Float.intBitsToFloat(bits)));
                                } catch (Exception e) { /* not mapped */ }
                            }
                        }
                    }
                    for (Reference r : ins.getReferencesFrom()) {
                        Address ref = r.getToAddress();
                        if (ref.isMemoryAddress()) {
                            try {
                                int bits = currentProgram.getMemory().getInt(ref);
                                extra.append(String.format("  ->%s=0x%08x (f=%g)", ref, bits, Float.intBitsToFloat(bits)));
                            } catch (Exception e) { }
                        }
                    }
                    w.println(ins.getAddress() + "  " + ins + extra);
                }
            }
        }
    }
}
