# Builds the editor target (with -Build) and runs the photo tour, one editor at a time on this machine:
# the 8 GB of video memory holds one game instance, so this waits while any other UnrealEditor runs,
# then shoots the requested views and prints the photo-mode lines from the log. Works from any
# checkout or worktree of the project; several sessions can share the machine through it.
#
#   powershell -File Tools\photo.ps1 -Build -Shots street,walker,house      # some views (~1 min)
#   powershell -File Tools\photo.ps1                                        # the whole tour (~2 min)
#   powershell -File Tools\photo.ps1 -Shots car -Extra -DTGMemReport        # with a memory report
param([string]$Shots = "", [switch]$Build, [string]$Extra = "")
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$engine = "C:\Program Files\Epic Games\UE_5.8\Engine"
if ($Build) {
    & "$engine\Build\BatchFiles\Build.bat" DustToGloryEditor Win64 Development "-Project=$root\DustToGlory.uproject" -WaitMutex -NoHotReloadFromIDE 2>&1 |
        Select-String -Pattern "error|warning C|Result:" | ForEach-Object { $_.Line -replace "^.*Source.DustToGlory.", "" }
    if ($LASTEXITCODE -ne 0) { "BUILD FAILED"; exit 1 }
}
# Another session's editor may be up: wait for it, with a little jitter so two waiters do not start together.
while (Get-Process -Name UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) { Start-Sleep -Seconds (10 + (Get-Random -Maximum 10)) }
$arguments = @("$root\DustToGlory.uproject", "-game", "-windowed", "-ResX=1600", "-ResY=900", "-log", "-nosplash", "-DTGPhoto")
if ($Shots) { $arguments += "-DTGPhotoShots=$Shots" }
# Several flags in one -Extra string are passed one by one: as one argument they reached the engine as -DTGSettle="10 -DTGSeed=7".
if ($Extra) { $arguments += ($Extra -split " ") }
& "$engine\Binaries\Win64\UnrealEditor.exe" @arguments
Select-String -Path "$root\Saved\Logs\DustToGlory.log" -Pattern "Photo mode: |LogDTG: Warning|LogDTG: Error|Fatal" |
    ForEach-Object { $line = $_.Line -replace "^.*LogDTG: ", "" -replace "Photo mode: .*/Saved/Photos/", "Photo mode: "; $line.Substring(0, [Math]::Min(220, $line.Length)) }
"Photos in $root\Saved\Photos"
