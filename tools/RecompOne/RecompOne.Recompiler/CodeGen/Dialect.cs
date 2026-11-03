using System.Text.RegularExpressions;
using RecompOne.Recompiler.Disasm;

namespace RecompOne.Recompiler.CodeGen;

/// <summary>
/// The spelling of the generated code. Control flow (delay slots, branches, jump
/// tables, local returns, hooks) is shared between the outputs; a dialect supplies
/// how registers, calls, dispatch and single instructions are written.
///
/// C# is the PC output and must stay byte-for-byte what it was. C is the portable
/// output (original Xbox) and targets the interface in docs/recompiler-c-output.md.
/// </summary>
public abstract class Dialect
{
    public static readonly Dialect CSharp = new CSharpDialect();
    public static readonly Dialect C = new CDialect();

    public abstract bool IsC { get; }

    /// <summary>A general-purpose register as an rvalue (r0 is the constant 0).</summary>
    public abstract string Reg(int r);

    /// <summary>The cast to a signed 32-bit integer.</summary>
    public abstract string I32 { get; }

    /// <summary>Call a generated function or hook that takes the CPU context.</summary>
    public abstract string Call(string name);

    /// <summary>Call whatever function lives at a run-time address.</summary>
    public abstract string Dispatch(string address);

    public abstract string GteCondition { get; }

    /// <summary>One non-control-flow instruction, or "" when it has no effect.</summary>
    public abstract string EmitSingle(MipsInstruction i);

    /// <summary>The emitted name of a patch, hook or library replacement target.</summary>
    public abstract string HookName(string target);

    /// <summary>A generated function as other generated code refers to it.</summary>
    public abstract string FunctionName(string className, string emittedName);
}

sealed class CSharpDialect : Dialect
{
    public override bool IsC => false;
    public override string Reg(int r) => InstructionEmitter.R(r);
    public override string I32 => "(int)";
    public override string Call(string name) => $"{name}(c, m);";
    public override string Dispatch(string address) => $"Dispatcher.Call(c, m, {address});";
    public override string GteCondition => "RecompOne.Runtime.Gte.GetCondition()";
    public override string EmitSingle(MipsInstruction i) => InstructionEmitter.EmitSingle(i);
    public override string HookName(string target) => target;
    public override string FunctionName(string className, string emittedName) => $"{className}.{emittedName}";
}

sealed class CDialect : Dialect
{
    public override bool IsC => true;
    public override string Reg(int r) => CInstructionEmitter.R(r);
    public override string I32 => "(int32_t)";
    public override string Call(string name) => $"{name}(c);";
    public override string Dispatch(string address) => $"recomp_dispatch(c, {address});";
    public override string GteCondition => "gte_condition()";
    public override string EmitSingle(MipsInstruction i) => CInstructionEmitter.EmitSingle(i);
    public override string FunctionName(string className, string emittedName) => $"{className}_{emittedName}";

    /// <summary>
    /// "Recompiled.Timing.WaitVBlanks" -> "hook_Timing_WaitVBlanks",
    /// "RecompOne.Runtime.Sdk.LibGpu.DrawOTag" -> "hook_LibGpu_DrawOTag": the class
    /// and member, which is unique across both games' configurations.
    /// </summary>
    public override string HookName(string target)
    {
        var parts = target.Split('.');
        string tail = parts.Length >= 2 ? $"{parts[^2]}_{parts[^1]}" : parts[^1];
        return "hook_" + Regex.Replace(tail, "[^A-Za-z0-9_]", "_");
    }
}
