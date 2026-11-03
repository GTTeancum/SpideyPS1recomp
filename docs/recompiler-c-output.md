# RecompOne C output

RecompOne writes either C# (the PC port, default) or portable C (the original Xbox
port). Both come from the same analysis and configuration; only the code writer
differs.

```bash
dotnet tools/RecompOne/RecompOne.Recompiler/bin/Release/net10.0/recompone.dll \
    config/spiderman.json --c ../xbox/generated/spiderman
```

Run from the game folder (`spiderman/` or `spiderman2/`). `--c <dir>` writes C there
instead of C# to the configured output; nothing else changes. The output is
derived from the player's disc and is not committed (`xbox/generated/` is ignored).

## What stays shared

`CodeGen/Dialect.cs` supplies the spelling; everything else is common:

- analysis, function detection, jump tables, collisions, patches and SDK
  replacements;
- control flow in `InstructionEmitter.EmitWithDelaySlot` and `FunctionEmitter.Emit`:
  delay slots, branches, jump tables, local returns, interior and branch hooks.

Single instructions have two writers: `InstructionEmitter.EmitSingle` (C#, with the
GTE provenance the PC's 60 FPS presentation needs) and `CInstructionEmitter` (C,
plain loads, stores and assignments; the Xbox port has no 60 FPS). An instruction
semantics fix belongs in both.

The C# output must stay byte-for-byte identical: after changing the code writer,
regenerate both games and compare with the previous `generated/` folders.

## Files

| File | Contents |
| --- | --- |
| `{Game}.h` | prototypes of every function and hook, overlay and game externs |
| `{Game}_{overlay}.c` | the overlay's functions and its address table |
| `{Game}_game.c` | boot executable, entry point, overlay list |

`{Game}` is the configured game name made identifier-safe (`SpiderMan`,
`SpiderMan2`). Every symbol carries it, so both games link into one program and
game symbols such as `memcpy` never meet the C library's.

## Runtime interface

The generated code includes only `recomp.h`, which the platform provides
(`xbox/runtime/recomp.h`):

- `CpuContext` with `r[32]`, `hi`, `lo` and the COP0 fields `sr`, `cause`, `epc`,
  `bad_vaddr`, `prid`. Generated functions are `void f(CpuContext *c)`.
- Memory: `mem_r8/16/32` (zero-extended), `mem_w8/16/32`, `mem_lwl/lwr/swl/swr`.
  Byte and halfword sign extension is written into the generated code.
- `recomp_dispatch(c, address)` calls the function at a run-time address (jump
  registers, function pointers, overlay entry points).
- `bios_syscall`, `bios_break`.
- GTE: `gte_execute(word)`, `gte_read_data/control(reg)`,
  `gte_write_data/control(reg, value)`, `gte_condition()`. LWC2/SWC2 are written as
  `gte_write_data(rt, mem_r32(a))` / `mem_w32(a, gte_read_data(rt))`.
- `RecompFunction`, `RecompOverlay`, `RecompGame` describe the tables.

### Hooks

Configuration targets become C functions named `hook_{Class}_{Member}`:
`Recompiled.Timing.WaitVBlanks` -> `hook_Timing_WaitVBlanks`,
`RecompOne.Runtime.Sdk.LibGpu.DrawOTag` -> `hook_LibGpu_DrawOTag`.

| Use | C signature | Meaning |
| --- | --- | --- |
| replacement, SDK reimplementation | `void hook(CpuContext *c)` | runs instead of the function |
| `post`, `instruction` | `void hook(CpuContext *c)` | runs after the function / at the instruction |
| `pre` | `int hook(CpuContext *c)` | 0 skips the original function |
| `instruction_branch` | `int hook(CpuContext *c)` | non-zero jumps to the resume address |

The generated header lists every hook with its configuration target. The PC's
post-generation text transforms (`tools/patch_costume_viewer.py`,
`tools/patch_texture_registry.py`) edit C# and do not apply to the C output; the
Xbox port needs its own equivalents.
