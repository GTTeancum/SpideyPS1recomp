#!/usr/bin/env bash
# Run PCSX-Redux with no window: run.sh NAME.lua [timeout-seconds]
# NAME.lua is a file in this folder. pcsx-redux.exe is only a launcher; run
# pcsx-redux.main itself. With -cli it opens a console window unless its output is
# already redirected, so start it from a windowless cmd with output sent to a file.
# On timeout, and always at the end, any copy from this folder is killed.
script="Z:/Programming/SpideyPS1Recomp/tools/redux/$1"
powershell -NoProfile -Command "
\$app = 'Z:\Programming\SpideyPS1Recomp\private\work\redux\app'
\$si = New-Object System.Diagnostics.ProcessStartInfo
\$si.FileName = 'cmd.exe'
\$si.WorkingDirectory = \$app
\$si.Arguments = '/c \"\"' + \$app + '\pcsx-redux.main\" -portable -cli -interpreter -debugger -logfile ..\last.log -iso \"Z:/Programming/SpideyPS1Recomp/Spider-Man (USA).bin\" -exec \"pcall(dofile, [[$script]])\" -run > ..\stdout.txt 2>&1\"'
\$si.UseShellExecute = \$false
\$si.CreateNoWindow = \$true
\$p = [System.Diagnostics.Process]::Start(\$si)
if (-not \$p.WaitForExit(${2:-900}000)) { 'timeout' }
Get-Process pcsx-redux.main,pcsx-redux -ErrorAction SilentlyContinue | Where-Object { \$_.Path -like \"\$app\*\" } | Stop-Process -Force
'exit=' + \$p.ExitCode
"
