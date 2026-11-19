// iOS armv7 uses the softfp ABI (float args/returns in core registers).
// Apply __stdcall_softfp to every function so the decompiler reads floats from r0-r3/stack.
//@category ZiGGURAT
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class SetSoftfp extends GhidraScript {
    @Override
    public void run() throws Exception {
        int n = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            try { f.setCallingConvention("__stdcall_softfp"); n++; } catch (Exception e) { }
        }
        for (Function f : currentProgram.getFunctionManager().getExternalFunctions()) {
            try { f.setCallingConvention("__stdcall_softfp"); } catch (Exception e) { }
        }
        println("softfp applied to " + n + " functions");
    }
}
