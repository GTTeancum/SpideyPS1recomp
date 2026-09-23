using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using RecompOne.Recompiler.Analysis;
using RecompOne.Recompiler.CodeGen;
using RecompOne.Recompiler.Config;
using RecompOne.Recompiler.Disasm;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;
using System.Reflection;

// Setup, conditional replacement, original body, original cleanup and epilogue.
uint[] words=[0x27BDFFF8,0xAFBF0004,0x24100001,0x26100002,0x26100004,0x8FBF0004,0x27BD0008,0x03E00008,0];
var ins=words.Select((w,i)=>new MipsInstruction(w,0x1000u+(uint)i*4)).ToArray();
var apply=typeof(OverlayWriter).GetMethod("ApplyPatches",BindingFlags.Static|BindingFlags.NonPublic)!;
int checks=0;
try
{
    foreach(bool replacement in new[]{false,true})
    {
        var f=new MipsFunction {Start=0x1000,End=0x1024,Instructions=ins,EmittedName="Run"};
        var patch=new PatchEntry {Mode="instruction_branch",Address="100C",ResumeAddress="1010",Target="Fixture.Hook"};
        apply.Invoke(null,new object[]{new List<MipsFunction>{f},new[]{patch}});
        // Re-applying cannot duplicate a hook.
        apply.Invoke(null,new object[]{new List<MipsFunction>{f},new[]{patch}});
        var ctx=new FunctionContext {FuncStart=f.Start,FuncEnd=f.End};
        string text="using RecompOne.Runtime.Context; using RecompOne.Runtime.Memory; public static class Fixture { public static int Calls; public static bool Hook(CpuContext c, IMemory m) { Calls++; return "+(replacement?"true":"false")+"; }"+FunctionEmitter.Emit(f,ctx)+"}";
        var refs=((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")!).Split(Path.PathSeparator).Append(typeof(CpuContext).Assembly.Location).Distinct().Select(p=>MetadataReference.CreateFromFile(p));
        var compilation=CSharpCompilation.Create("MovieHook"+replacement,[CSharpSyntaxTree.ParseText(text)],refs,new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));
        using var output=new MemoryStream();var result=compilation.Emit(output);
        if(!result.Success) throw new Exception(string.Join("\n",result.Diagnostics));
        var type=Assembly.Load(output.ToArray()).GetType("Fixture")!;var cpu=new CpuContext {SP=0x80011000,RA=0x80012000};
        type.GetMethod("Run")!.Invoke(null,[cpu,new PSMemory(0x800000)]);
        if(cpu.S0!=(replacement?5:7) || cpu.SP!=0x80011000 || cpu.RA!=0x80012000 || (int)type.GetField("Calls")!.GetValue(null)!=1) throw new Exception("branch/setup/cleanup/stack mismatch");
        checks++;
    }
    foreach((string from,string to) in new[]{("1000","1000"),("1020","1010"),("100C","1020"),("9000","1010"),("100C","9000")})
    {
        var f=new MipsFunction {Start=0x1000,End=0x1024,Instructions=ins,EmittedName="Run"};
        bool rejected=false;try {apply.Invoke(null,new object[]{new List<MipsFunction>{f},new[]{new PatchEntry{Mode="instruction_branch",Address=from,ResumeAddress=to,Target="Fixture.Hook"}}});}
        catch(TargetInvocationException e) when(e.InnerException is InvalidDataException){rejected=true;}
        if(!rejected) throw new Exception("unsafe branch accepted: "+from+" -> "+to);checks++;
    }
    Console.WriteLine($"{checks} executable emitter checks passed; not gameplay.");return 0;
}
finally {foreach(var i in ins)i.Dispose();}
