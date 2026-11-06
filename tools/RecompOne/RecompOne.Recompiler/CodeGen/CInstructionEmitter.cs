using RecompOne.Recompiler.Disasm;

namespace RecompOne.Recompiler.CodeGen;

/// <summary>
/// Single MIPS instructions as C, instruction for instruction the same semantics as
/// <see cref="InstructionEmitter.EmitSingle"/>, including its GTE vertex provenance
/// (precise projected positions and depths for the renderer): register tags follow
/// the C# output's MoveGpr/SetDerived rules, every other register write clears its
/// tag, loads and word stores carry tags through RAM (c->t[], see the runtime's
/// recomp.h). The C output has no 60 FPS vertex identities.
/// </summary>
public static class CInstructionEmitter
{
    public static string R(int r) => r == 0 ? "0u" : $"c->r[{r}]";

    /// <summary>A register's vertex tag as an rvalue.</summary>
    static string T(int r) => r == 0 ? "0" : $"c->t[{r}]";
    static string Clear(int r) => $" c->t[{r}] = 0;";
    static string Move(int d, int s) => $" c->t[{d}] = {T(s)};";
    static string Pick(int d, int a, int b) => $" c->t[{d}] = vtag_pick({T(a)}, {T(b)});";

    static string Addr(int rs, short imm)
    {
        if (rs == 0) return $"0x{(uint)(int)imm:X8}u";
        if (imm == 0) return R(rs);
        if (imm > 0) return $"({R(rs)} + 0x{(uint)imm:X}u)";
        return $"({R(rs)} - 0x{unchecked((uint)(-(int)imm)):X}u)";
    }

    static string Cop0Read(int rd) => rd switch
    {
        8 => "c->bad_vaddr",
        12 => "c->sr",
        13 => "c->cause",
        14 => "c->epc",
        15 => "c->prid",
        _ => $"0u /* COP0[{rd}] */"
    };

    static string Cop0Write(int rd, string val) => rd switch
    {
        8 => $"c->bad_vaddr = {val};",
        12 => $"c->sr = {val};",
        13 => $"c->cause = {val};",
        14 => $"c->epc = {val};",
        15 => $"c->prid = {val};",
        _ => $"/* MTC0 r{rd} ignored */"
    };

    public static string EmitSingle(MipsInstruction i)
    {
        uint op = i.Word >> 26;
        uint fn = i.Word & 0x3F;
        int rs = i.Rs, rt = i.Rt, rd = i.Rd, sa = i.Sa;
        short imm = i.ImmS;
        ushort immU = i.ImmU;
        string RS = R(rs), RT = R(rt), RD = R(rd);

        if (op == 0)
        {
            return (int)fn switch
            {
                0 => rd == 0 ? "" : sa == 0 ? $"{RD} = {RT};" + Move(rd, rt) : $"{RD} = {RT} << {sa};" + Move(rd, rt),
                2 => rd == 0 ? "" : $"{RD} = {RT} >> {sa};" + Move(rd, rt),
                3 => rd == 0 ? "" : $"{RD} = (uint32_t)((int32_t){RT} >> {sa});" + Move(rd, rt),
                4 => rd == 0 ? "" : $"{RD} = {RT} << ({RS} & 31u);" + Pick(rd, rt, rs),
                6 => rd == 0 ? "" : $"{RD} = {RT} >> ({RS} & 31u);" + Pick(rd, rt, rs),
                7 => rd == 0 ? "" : $"{RD} = (uint32_t)((int32_t){RT} >> ({RS} & 31u));" + Pick(rd, rt, rs),
                8 => "",
                9 => "",
                12 => "bios_syscall(c);",
                13 => "bios_break(c);",
                16 => rd == 0 ? "" : $"{RD} = c->hi;" + Clear(rd),
                17 => $"c->hi = {RS};",
                18 => rd == 0 ? "" : $"{RD} = c->lo;" + Clear(rd),
                19 => $"c->lo = {RS};",
                24 => $"{{ int64_t r_ = (int64_t)(int32_t){RS} * (int32_t){RT}; c->lo = (uint32_t)r_; c->hi = (uint32_t)((uint64_t)r_ >> 32); }}",
                25 => $"{{ uint64_t r_ = (uint64_t){RS} * {RT}; c->lo = (uint32_t)r_; c->hi = (uint32_t)(r_ >> 32); }}",
                26 => rt == 0 ? "c->lo = 0u; c->hi = 0u;" : $"if ({RT} != 0u) {{ if ((int32_t){RS} == INT32_MIN && (int32_t){RT} == -1) {{ c->lo = 0x80000000u; c->hi = 0u; }} else {{ c->lo = (uint32_t)((int32_t){RS} / (int32_t){RT}); c->hi = (uint32_t)((int32_t){RS} % (int32_t){RT}); }} }}",
                27 => rt == 0 ? "c->lo = 0u; c->hi = 0u;" : $"if ({RT} != 0u) {{ c->lo = {RS} / {RT}; c->hi = {RS} % {RT}; }}",
                32 or 33 => rd == 0 ? "" : $"{RD} = {RS} + {RT};" + Pick(rd, rs, rt),
                34 or 35 => rd == 0 ? "" : $"{RD} = {RS} - {RT};" + Pick(rd, rs, rt),
                36 => rd == 0 ? "" : $"{RD} = {RS} & {RT};" + Pick(rd, rs, rt),
                37 => rd == 0 ? "" : rs == 0 ? $"{RD} = {RT};" + Move(rd, rt) : rt == 0 ? $"{RD} = {RS};" + Move(rd, rs) : $"{RD} = {RS} | {RT};" + Pick(rd, rs, rt),
                38 => rd == 0 ? "" : $"{RD} = {RS} ^ {RT};" + Pick(rd, rs, rt),
                39 => rd == 0 ? "" : $"{RD} = ~({RS} | {RT});" + Pick(rd, rs, rt),
                42 => rd == 0 ? "" : $"{RD} = (int32_t){RS} < (int32_t){RT} ? 1u : 0u;" + Clear(rd),
                43 => rd == 0 ? "" : $"{RD} = {RS} < {RT} ? 1u : 0u;" + Clear(rd),
                _ => Unknown(i, $"SPECIAL fn=0x{fn:X2}")
            };
        }

        if (op == 1) return "";

        if (op == 16)
        {
            uint cop0rs = (i.Word >> 21) & 0x1F;
            if (cop0rs == 0) return rt == 0 ? "" : $"{RT} = {Cop0Read(rd)};" + Clear(rt);
            if (cop0rs == 4) return Cop0Write(rd, RT);
            if (cop0rs == 16 && fn == 16) return "c->sr = (c->sr & ~0xFu) | ((c->sr >> 2) & 0xFu);";
            return $"/* COP0 rs={cop0rs} */";
        }

        if (op == 18)
        {
            uint cop2rs = (i.Word >> 21) & 0x1F;
            if (cop2rs == 8) return "";
            if (((i.Word >> 25) & 1) == 1) return $"gte_execute(0x{i.Word:X8}u);";
            return cop2rs switch
            {
                0 => rt == 0 ? "" : $"{RT} = gte_read_data_t(c, {rt}, {rd});",
                2 => rt == 0 ? "" : $"{RT} = gte_read_control({rd});" + Clear(rt),
                4 => $"gte_write_data_t({rd}, {RT}, {T(rt)});",
                6 => $"gte_write_control({rd}, {RT});",
                _ => $"/* COP2 rs={cop2rs} */"
            };
        }

        if (op is 2 or 3 or 4 or 5 or 6 or 7) return "";

        return (int)op switch
        {
            8 or 9 => rt == 0 ? "" : rs == 0 ? $"{RT} = 0x{unchecked((uint)(int)imm):X8}u;" + Clear(rt) : imm >= 0 ? $"{RT} = {RS} + 0x{(uint)imm:X}u;" + Move(rt, rs) : $"{RT} = {RS} - 0x{unchecked((uint)(-(int)imm)):X}u;" + Move(rt, rs),
            10 => rt == 0 ? "" : $"{RT} = (int32_t){RS} < {(int)imm} ? 1u : 0u;" + Clear(rt),
            11 => rt == 0 ? "" : $"{RT} = {RS} < 0x{(uint)(int)imm:X8}u ? 1u : 0u;" + Clear(rt),
            12 => rt == 0 ? "" : $"{RT} = {RS} & 0x{immU:X4}u;" + Move(rt, rs),
            13 => rt == 0 ? "" : immU == 0 ? $"{RT} = {RS};" + Move(rt, rs) : $"{RT} = {RS} | 0x{immU:X4}u;" + Move(rt, rs),
            14 => rt == 0 ? "" : $"{RT} = {RS} ^ 0x{immU:X4}u;" + Move(rt, rs),
            15 => rt == 0 ? "" : $"{RT} = 0x{(uint)immU << 16:X8}u;" + Clear(rt),
            32 => rt == 0 ? "" : $"{{ uint32_t a_ = {Addr(rs, imm)}; {RT} = (uint32_t)(int32_t)(int8_t)mem_r8(a_); c->t[{rt}] = vtag_containing(a_); }}",
            33 => rt == 0 ? "" : $"{{ uint32_t a_ = {Addr(rs, imm)}; {RT} = (uint32_t)(int32_t)(int16_t)mem_r16(a_); c->t[{rt}] = vtag_containing(a_); }}",
            34 => rt == 0 ? "" : $"{RT} = mem_lwl({RT}, {Addr(rs, imm)});" + Clear(rt),
            35 => rt == 0 ? "" : $"{RT} = mem_r32_t(c, {rt}, {Addr(rs, imm)});",
            36 => rt == 0 ? "" : $"{{ uint32_t a_ = {Addr(rs, imm)}; {RT} = mem_r8(a_); c->t[{rt}] = vtag_containing(a_); }}",
            37 => rt == 0 ? "" : $"{{ uint32_t a_ = {Addr(rs, imm)}; {RT} = mem_r16(a_); c->t[{rt}] = vtag_containing(a_); }}",
            38 => rt == 0 ? "" : $"{RT} = mem_lwr({RT}, {Addr(rs, imm)});" + Clear(rt),
            40 => $"mem_w8({Addr(rs, imm)}, (uint8_t){RT});",
            41 => $"mem_w16({Addr(rs, imm)}, (uint16_t){RT});",
            42 => $"mem_swl({Addr(rs, imm)}, {RT});",
            43 => $"mem_w32_t({Addr(rs, imm)}, {RT}, {T(rt)});",
            46 => $"mem_swr({Addr(rs, imm)}, {RT});",
            50 => $"gte_load_word({rt}, {Addr(rs, imm)});",
            58 => $"gte_store_word({Addr(rs, imm)}, {rt});",
            _ => Unknown(i, $"op=0x{op:X2}")
        };
    }

    static string Unknown(MipsInstruction i, string desc)
    {
        Console.WriteLine($"[Unknown] {desc} word=0x{i.Word:X8} @ 0x{i.Vram:X8}");
        return $"/* UNKNOWN OP {desc} word=0x{i.Word:X8} @ 0x{i.Vram:X8} */";
    }
}
