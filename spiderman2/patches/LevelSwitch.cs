using System;
using System.Text;
using System.Text.RegularExpressions;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>
/// Boots straight into a chosen level by selecting its retail descriptor, with
/// archive-name redirection retained for targets outside the campaign table.
///
/// The current campaign name also selects level flags and transition behavior.
/// Rewriting e1m0 files alone leaves that state on the tutorial even when E1M3 or
/// E1M4 assets load. Selecting the descriptor before the native stage lookup keeps
/// those resources and their native metadata together.
///
///     SPIDEY_LEVEL=e3m1     start in episode 3, mission 1
///
/// For targets without a descriptor, the first non-demo level prefix becomes the
/// archive fallback source. Only that prefix is rewritten, leaving subsequent native
/// stage transitions alone.
/// </summary>
public static class LevelSwitch
{
    // Scratch for the rewritten name. Above the overlay region, inside the 8 MB the port
    // runs with, and never allocated from -- see OverlayPatches for why that space exists.
    const uint Scratch = 0x802B0000;

    // func_8001A85C calls func_8001A700 with this nine-byte trigger-name buffer.
    // The table has 24 campaign descriptors, each 20 bytes, with a name pointer at +4.
    const uint CurrentLevelName = 0x800B3184;
    const uint LoadDescriptorReturn = 0x8001A92C;
    const uint LevelDescriptors = 0x800A3638;
    const int LevelDescriptorCount = 24;
    const int LevelNameCapacity = 9;

    static readonly Regex LevelName = new(@"^(e\d+m\d+|tr\d{2}|wr\d{2}|xt\d{2}|dem\d)(.*)$", RegexOptions.IgnoreCase);
    static readonly Regex CampaignTrigger = new(@"^e\d+m\d+_t$", RegexOptions.IgnoreCase);

    static string _target;
    static string _source;
    static bool _descriptorSelected;
    public static int Rewrites { get; private set; }

    public static void Install()
    {
        var want = Environment.GetEnvironmentVariable("SPIDEY_LEVEL");
        if (!string.IsNullOrWhiteSpace(want))
        {
            _target = want.Trim().ToLowerInvariant();
            Console.WriteLine($"[level] starting in '{_target}'");
        }

    }

    /// <summary>
    /// Change only the stage loader's first campaign descriptor selection. The caller
    /// guard leaves save previews and menu lookups alone; the native loader applies
    /// the descriptor's flags and all subsequent transition behavior itself.
    /// </summary>
    public static void SelectDescriptor(CpuContext c, IMemory m)
    {
        if (_descriptorSelected || _target == null || c.A0 != CurrentLevelName ||
            c.RA != LoadDescriptorReturn) return;

        string targetName = _target + "_t";
        if (targetName.Length >= LevelNameCapacity || !CampaignTrigger.IsMatch(targetName) ||
            !HasDescriptor(m, targetName)) return;

        string currentName = ReadCString(m, CurrentLevelName);
        if (!CampaignTrigger.IsMatch(currentName)) return;

        if (!string.Equals(currentName, targetName, StringComparison.OrdinalIgnoreCase))
        {
            WriteCString(m, CurrentLevelName, targetName);
            Console.WriteLine($"[level] descriptor {currentName} -> {targetName}");
        }

        // Native filenames now follow the selected descriptor. Do not bind a later
        // stage's first lookup as a fallback source and redirect the campaign back.
        _descriptorSelected = true;
    }

    /// <summary>
    /// Called first thing from the CdWadFind pre-hook. Returns the name the game should
    /// actually look up, having pointed a0 at it when it changed.
    /// </summary>
    public static string Redirect(CpuContext c, IMemory m, string name)
    {
        if (name == null) return name;


        if (_target == null || _descriptorSelected) return name;

        var hit = LevelName.Match(name);
        if (!hit.Success) return name;

        string prefix = hit.Groups[1].Value, rest = hit.Groups[2].Value;

        // Never bind to a demo. The attract loop plays dem1..dem4 while the title screen
        // is up, so on a normal boot the first level prefix the game asks for belongs to
        // the attract demo, not to the game the player is about to start -- binding there
        // would redirect the demo and leave the real level alone. Demos are still
        // redirectable if asked for by name.
        if (_source == null && prefix.StartsWith("dem", StringComparison.OrdinalIgnoreCase)
                            && !_target.StartsWith("dem", StringComparison.OrdinalIgnoreCase))
            return name;

        // Bind to whichever level the game asks for first and redirect only that one.
        _source ??= prefix.ToLowerInvariant();
        if (!string.Equals(prefix, _source, StringComparison.OrdinalIgnoreCase)) return name;

        // Follow the case the game used, in case the archive compare is case sensitive.
        string swapped = char.IsUpper(prefix[0]) ? _target.ToUpperInvariant() : _target;
        return Rewrite(c, m, name, swapped + rest);
    }

    static string Rewrite(CpuContext c, IMemory m, string from, string to)
    {
        WriteCString(m, Scratch, to);
        c.A0 = Scratch;

        Rewrites++;
        Console.WriteLine($"[level] {from} -> {to}");
        return to;
    }

    static bool HasDescriptor(IMemory m, string triggerName)
    {
        for (int i = 0; i < LevelDescriptorCount; i++)
        {
            uint nameAddress = m.ReadU32(LevelDescriptors + (uint)(i * 20 + 4));
            if (nameAddress == 0) continue;
            if (string.Equals(ReadCString(m, nameAddress), triggerName,
                              StringComparison.OrdinalIgnoreCase))
                return true;
        }
        return false;
    }

    static string ReadCString(IMemory m, uint address)
    {
        var text = new StringBuilder();
        for (int i = 0; i < LevelNameCapacity; i++)
        {
            byte value = m.ReadU8(address + (uint)i);
            if (value == 0) return text.ToString();
            text.Append((char)value);
        }
        return string.Empty;
    }

    static void WriteCString(IMemory m, uint address, string text)
    {
        byte[] bytes = Encoding.ASCII.GetBytes(text);
        for (int i = 0; i < bytes.Length; i++) m.WriteU8(address + (uint)i, bytes[i]);
        m.WriteU8(address + (uint)bytes.Length, 0);
    }
}
