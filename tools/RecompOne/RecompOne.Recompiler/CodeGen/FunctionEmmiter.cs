using System.Text;
using RecompOne.Recompiler.Analysis;
using RecompOne.Recompiler.Disasm;

namespace RecompOne.Recompiler.CodeGen;

public static class FunctionEmitter
{
    public static string Emit(MipsFunction func, FunctionContext ctx)
    {
        var sb = new StringBuilder();
        var instrs = func.Instructions;
        
        // Conditional instruction hooks may only resume inside the same function.
        // Add their labels before delay-slot placement is computed.
        foreach (var hooks in func.InstructionBranchHooks.Values)
            foreach (var hook in hooks) ctx.Labels.Add(hook.Resume);

        //a delay slot of an unconditional transfer is emitted only inline (before the
        // jump) and skipped here but  for the edge case where that same instruction is also a branch target it needs to
        // also be emitted at its ""natural(?)"" position (and the label too) so jumps to it need to be into the following
        //  instructions instead of into the preceding jump in that case it shouldnt be skiped, otherwise it will be emmited in the wrong location and cause crashes
        // on the functions with this edge case
        var dsIdx = new HashSet<int>();
        for (int i = 0; i < instrs.Length - 1; i++)
            if (instrs[i].HasDelaySlot && InstructionEmitter.SkipDelaySlot(instrs[i])
                && !ctx.Labels.Contains(instrs[i + 1].Vram))
                dsIdx.Add(i + 1);

        var lang = ctx.Lang;
        string name = lang.IsC ? ctx.FunctionName(func.EmittedName) : func.EmittedName;
        const string ind = "        ";
        string noInline = lang.IsC ? "" : "    [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.NoInlining)]";
        string Signature(string n) => lang.IsC ? $"void {n}(CpuContext *c)" : $"    public static void {n}(CpuContext c, IMemory m)";
        void NoInline() { if (!lang.IsC) sb.AppendLine(noInline); }

        if (func.IsStub)
        {
            NoInline();
            sb.AppendLine(lang.IsC ? $"{Signature(name)} {{ (void)c; }}" : $"    public static void {name}(CpuContext c, IMemory m) {{ }}");
            return sb.ToString();
        }
        bool hooked = func.PreHookTargets.Count > 0 || func.PostHookTargets.Count > 0 || ctx.SpAudit;

        if (func.IsPatch)
        {
            NoInline();
            string patch = lang.HookName(func.PatchTarget);
            if (!hooked)
            {
                sb.AppendLine(lang.IsC
                    ? $"{Signature(name)} {{ {lang.Call(patch)} }}"
                    : $"    public static void {name}(CpuContext c, IMemory m) => {func.PatchTarget}(c, m);");
                return sb.ToString();
            }
            sb.AppendLine(Signature(name));
            sb.AppendLine(lang.IsC ? "{" : "    {");
            EmitHooks(sb, func, $"        {lang.Call(patch)}", ctx);
            sb.AppendLine(lang.IsC ? "}" : "    }");
            return sb.ToString();
        }
        if (hooked)
        {
            if (lang.IsC) sb.AppendLine($"static {Signature(name + "_Impl")};");
            NoInline();
            sb.AppendLine(Signature(name));
            sb.AppendLine(lang.IsC ? "{" : "    {");
            EmitHooks(sb, func, $"        {lang.Call(name + "_Impl")}", ctx);
            sb.AppendLine(lang.IsC ? "}" : "    }");
            name += "_Impl";
        }

        NoInline();
        sb.AppendLine(lang.IsC && hooked ? $"static {Signature(name)}" : Signature(name));
        sb.AppendLine(lang.IsC ? "{" : "    {");
        if (ctx.CallRing && !lang.IsC)
            sb.AppendLine($"        RecompOne.Runtime.Diagnostics.CallRing.Enter(0x{func.Start:X8}u);");
        if (ctx.Debug && !lang.IsC)
            sb.AppendLine($"        System.Console.WriteLine(\"{func.EmittedName} @ {func.OverlayName} @ 0x{func.Start:X8}\");");

        for (int i = 0; i < instrs.Length; i++)
        {
            if (dsIdx.Contains(i)) continue;

            var instr = instrs[i];

            if (ctx.Labels.Contains(instr.Vram))
                sb.AppendLine($"        L{instr.Vram:X8}: ;");

            // A pose-ready observer runs at the instruction's normal location,
            // after its label, without changing native control flow or RA.
            if (func.InstructionHookTargets.TryGetValue(instr.Vram, out var instructionHooks))
                foreach (string target in instructionHooks)
                    sb.AppendLine($"{ind}{lang.Call(lang.HookName(target))}");

            if (func.InstructionBranchHooks.TryGetValue(instr.Vram, out var branchHooks))
                foreach (var hook in branchHooks)
                    sb.AppendLine(lang.IsC
                        ? $"{ind}if ({lang.HookName(hook.Target)}(c)) goto L{hook.Resume:X8};"
                        : $"{ind}if ({hook.Target}(c, m)) goto L{hook.Resume:X8};");

            if (instr.Vram != func.Start && ctx.InteriorHooks.TryGetValue(instr.Vram, out var interiorHook))
            {
                // Hand-written renderer routines have overlapping symbol ranges.
                // A local jump into another function must still run its hooks.
                // Resume the native RA continuation rather than returning from the
                // enclosing managed method and skipping its remaining work/epilogue.
                sb.AppendLine($"{ind}{lang.Call(interiorHook)}");
                sb.AppendLine($"{ind}switch ({lang.Reg(31)})");
                sb.AppendLine($"{ind}{{");
                foreach (uint back in ctx.LocalReturns.OrderBy(a => a))
                    sb.AppendLine($"{ind}    case 0x{back:X8}u: goto L{back:X8};");
                sb.AppendLine($"{ind}    default: return;");
                sb.AppendLine($"{ind}}}");
                continue;
            }

            if (instr.HasDelaySlot)
            {
                var delaySlot = i + 1 < instrs.Length ? instrs[i + 1] : null;
                InstructionEmitter.EmitWithDelaySlot(sb, instr, delaySlot, ctx, ind);
            }else
            {
                string line = lang.EmitSingle(instr);
                if (!string.IsNullOrEmpty(line))
                    sb.AppendLine(ctx.Trail(instr, $"{ind}{line}"));
            }
        }

        if (FallsThrough(instrs))
        {
            uint target = ctx.SkipNopPadding(func.End);
            if (ctx.KnownFunctions.TryGetValue(target, out var fallthroughName))
                sb.AppendLine($"{ind}{lang.Call(fallthroughName)}");
            else
                sb.AppendLine($"{ind}{lang.Dispatch($"0x{target:X8}u")}");
        }

        sb.AppendLine(lang.IsC ? "}" : "    }");
        return sb.ToString();
    }

    static void EmitHooks(StringBuilder sb, MipsFunction func, string body, FunctionContext ctx = null)
    {
        var lang = ctx?.Lang ?? Dialect.CSharp;
        bool audit = ctx != null && ctx.SpAudit && !lang.IsC;
        if (audit)
        {
            sb.AppendLine("        uint __sp0 = c.SP;");
            sb.AppendLine("        var __r0 = RecompOne.Runtime.Diagnostics.SpAudit.Snapshot(c);");
        }
        foreach (var pre in func.PreHookTargets)
            sb.AppendLine(lang.IsC
                ? $"        if (!{lang.HookName(pre)}(c)) return;"
                : $"        if (!RecompOne.Runtime.Context.PreHook.Run({pre}, c, m)) return;");
        sb.AppendLine(body);
        foreach (var post in func.PostHookTargets)
            sb.AppendLine($"        {lang.Call(lang.HookName(post))}");
        if (audit)
        {
            sb.AppendLine($"        RecompOne.Runtime.Diagnostics.SpAudit.Check(0x{func.Start:X8}u, \"{func.EmittedName}\", __sp0, c.SP);");
            sb.AppendLine($"        RecompOne.Runtime.Diagnostics.SpAudit.CheckRegs(0x{func.Start:X8}u, \"{func.EmittedName}\", __r0, c);");
            sb.AppendLine("        RecompOne.Runtime.Diagnostics.SpAudit.Release(__r0);");
        }
    }

    //some hand-written asm (crt0 stubs, etc) has no jr/j/branch at its declared end at all and just
    // runs straight into the next symbol if a jal at the end is a call, not a fall-through, so its left alone
    static bool FallsThrough(MipsInstruction[] instrs)
    {
        if (instrs.Length == 0) return false;

        int idx = instrs.Length - 1;
        if (instrs.Length >= 2 && instrs[idx - 1].HasDelaySlot) idx--;

        var ctrl = instrs[idx];
        if (ctrl.IsReturn || ctrl.IsJump || ctrl.IsRegisterJump || ctrl.IsUnconditionalBranch) return false;
        if (ctrl.IsFunctionCall) return false;
        return true;
    }
}
