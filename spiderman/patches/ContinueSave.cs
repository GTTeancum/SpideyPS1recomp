#nullable enable
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using RecompOne.Runtime;
using RecompOne.Runtime.Context;
using RecompOne.Runtime.Hardware;
using RecompOne.Runtime.Memory;

namespace Recompiled;

/// <summary>Restore Continue once at startup, and remember successful native saves.</summary>
public static class ContinueSave
{
    const string FileName = "BASLUS-00875SPD";
    const uint Live = 0x800A5688, Buffer = 0x800967CC;
    const int SlotSize = 0xB4, SlotCount = 8, Scores = 0x7A0, ScoreSize = 0x242;
    static IMemory? _started;
    static int _pendingSlot = -1;

    public sealed record SavedSlot(int Slot, string Sha256, long SavedUtcTicks);

    public static uint Checksum(ReadOnlySpan<byte> slot)
    {
        uint sum = 0;
        for (int offset = 4; offset < SlotSize; offset += 4)
            sum = unchecked(((sum << 1) | (sum >> 31)) + BinaryPrimitives.ReadUInt32LittleEndian(slot[offset..]));
        return sum | 1;
    }

    static bool Valid(ReadOnlySpan<byte> slot)
    {
        if (slot.Length != SlotSize || BinaryPrimitives.ReadUInt32LittleEndian(slot) != Checksum(slot)) return false;
        // Continue needs a level/checkpoint; a checksum alone also accepts unrelated data.
        if (slot[0x78] == 0 || slot[4] == 0) return false;
        int end = slot[4..0xD].IndexOf((byte)0);
        return end > 0 && slot[0xD..0x3F].IndexOf((byte)0) >= 0;
    }

    static byte[]? ReadFile(MemoryCard card)
    {
        if (!card.Enabled || !File.Exists(card.FilePath) || new FileInfo(card.FilePath).Length != MemoryCard.CardSize) return null;
        // Read the mounted image. No formatting, flushing, or writes during autoload.
        int first = card.Find(FileName);
        if (first == 0 || card.FileSize(first) < 0xA00) return null;
        int[] chain = card.Chain(first);
        if (chain.Length == 0 || chain.Distinct().Count() != chain.Length || chain.Length * 0x2000 < card.FileSize(first)) return null;
        byte[] data = new byte[0xA00];
        for (int i = 0; i < data.Length; i++) data[i] = card.ReadByte(chain, i);
        return data[0] == 'S' && data[1] == 'C' ? data : null;
    }

    static List<SavedSlot> ReadHistory(MemoryCard card)
    {
        try
        {
            return (JsonSerializer.Deserialize<List<SavedSlot?>>(File.ReadAllText(card.FilePath + ".continue.json")) ?? [])
                .Where(x => x != null && x.Slot >= 0 && x.Slot < SlotCount &&
                    x.Sha256?.Length == 64 && x.SavedUtcTicks > 0)
                .Select(x => x!).ToList();
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or JsonException) { return []; }
    }

    static string Hash(ReadOnlySpan<byte> slot) => Convert.ToHexString(SHA256.HashData(slot));

    public static int SelectSlot(byte[] data, IEnumerable<SavedSlot> history)
    {
        int chosen = -1;
        long newest = long.MinValue;
        for (int i = 0; i < SlotCount; i++)
        {
            ReadOnlySpan<byte> slot = data.AsSpan(0x200 + i * SlotSize, SlotSize);
            if (!Valid(slot)) continue;
            string hash = Hash(slot);
            long saved = history.Where(x => x.Slot == i && x.Sha256 == hash)
                .Select(x => x.SavedUtcTicks).DefaultIfEmpty(0).Max();
            // Legacy PS1 cards have no slot timestamps. Use the last occupied slot
            // only when no matching history exists, and explicitly log that fallback.
            if (saved >= newest) { newest = saved; chosen = i; }
        }
        return chosen;
    }

    /// <summary>Pre-hook at the title-shell entry, before selecting/loading its model.</summary>
    public static void Startup(CpuContext c, IMemory m)
    {
        if (ReferenceEquals(_started, m)) return;
        _started = m;
        _pendingSlot = -1;
        // An in-process return to the title must never replace current progress.
        if (m.ReadU8(Live + 0x78) != 0) return;
        try
        {
            (MemoryCard card, byte[] data, int slot, long time)? best = null;
            foreach (var card in new[] { Runtime.CardA, Runtime.CardB })
            {
                byte[]? data = ReadFile(card);
                if (data == null) continue;
                var history = ReadHistory(card);
                int slot = SelectSlot(data, history);
                if (slot < 0) continue;
                string hash = Hash(data.AsSpan(0x200 + slot * SlotSize, SlotSize));
                long time = history.Where(x => x.Slot == slot && x.Sha256 == hash)
                    .Select(x => x.SavedUtcTicks).DefaultIfEmpty(0).Max();
                if (best == null || time > best.Value.time) best = (card, data, slot, time);
            }
            if (best == null) return;
            var selected = best.Value;
            int offset = 0x200 + selected.slot * SlotSize;
            // Same save structure and high-score table as the native Load Game path.
            for (uint i = 0; i < SlotSize; i++) m.WriteU8(Live + i, selected.data[offset + i]);
            for (uint i = 0; i < ScoreSize; i++) m.WriteU8(0x800A51D0 + i, selected.data[Scores + i]);
            var registers = c.Snapshot();
            try { SpiderMan.func_8006D1C0(c, m); }
            finally { c.Restore(registers); }
            string level = Encoding.ASCII.GetString(selected.data, offset + 4, 9).TrimEnd('\0');
            Console.WriteLine($"[continue] autoload slot {selected.slot + 1}: {level}" +
                (selected.time == 0 ? " (legacy card: last occupied slot; no save-time history)" : " (most recently saved)"));
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        { Console.Error.WriteLine($"[continue] unable to read save: {e.Message}"); }
    }

    // Native save has two copy paths: overwriting a named slot and entering a new name.
    public static void CaptureOverwrite(CpuContext c, IMemory m) => Capture(c.A0);
    public static void CaptureNew(CpuContext c, IMemory m) => Capture(c.V0);
    static void Capture(uint destination)
    {
        uint offset = destination - (Buffer + 0x200);
        if (offset < SlotCount * SlotSize && offset % SlotSize == 0) _pendingSlot = (int)(offset / SlotSize);
    }

    /// <summary>Post-hook after native card write. Only persist metadata for disk-confirmed data.</summary>
    public static void Saved(CpuContext c, IMemory m)
    {
        int slot = _pendingSlot;
        _pendingSlot = -1;
        if (c.V0 != 0 || slot < 0) return;
        MemoryCard card = m.ReadU8(0x800967B6) == '1' ? Runtime.CardB : Runtime.CardA;
        if (!card.Enabled) return;
        try
        {
            // The BIOS can report completion after a failed host write. Check actual disk bytes.
            byte[]? data = ReadFile(new MemoryCard(card.FilePath));
            if (data == null) return;
            byte[] saved = data.AsSpan(0x200 + slot * SlotSize, SlotSize).ToArray();
            if (!Valid(saved)) return;
            for (uint i = 0; i < SlotSize; i++)
                if (saved[i] != m.ReadU8(Buffer + 0x200 + (uint)(slot * SlotSize) + i)) return;
            var history = ReadHistory(card);
            history.RemoveAll(x => x.Slot == slot);
            history.Add(new(slot, Hash(saved), DateTime.UtcNow.Ticks));
            string path = card.FilePath + ".continue.json";
            File.WriteAllText(path + ".tmp", JsonSerializer.Serialize(history));
            File.Move(path + ".tmp", path, true);
            Console.WriteLine($"[continue] recorded latest saved slot {slot + 1}");
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        { Console.Error.WriteLine($"[continue] save intact; could not record slot order: {e.Message}"); }
    }
}
