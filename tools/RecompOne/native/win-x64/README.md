# Windows native runtime payload

Release x64 Microsoft Visual C++ runtime, version 14.44.35112, copied from Visual Studio 2022 Community's `VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT` redistributable directory. Copyright Microsoft Corporation. These unmodified components remain subject to the Microsoft Visual Studio redistribution license; they are not covered by this repository's source license.

Redistribution reference: https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution

`Runtime.props` includes the three required libraries only when the effective target is `win-x64`. An explicit `RuntimeIdentifier` takes precedence; ordinary builds without a RID use the SDK host RID for native payload selection. Linux targets do not copy these Windows libraries. `BundledNativeRuntime` loads the extracted Windows copies by absolute path before GLFW, native file dialogs, or OpenAL initialize. No separate VC++ installer or loose DLLs are required in the Windows download. Windows, UCRT and graphics-driver libraries are still system dependencies.

When updating, use Microsoft's release x64 redistributable directory, update all three files together, inspect native imports, publish both games, and verify each running process loaded these libraries from its own bundle extraction directory. Never copy debug runtimes or System32 files into the release.
