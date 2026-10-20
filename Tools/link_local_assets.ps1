# Links the gitignored, machine-local folders of the main checkout into this worktree, so a worktree can
# build and run the game without downloading gigabytes again: the Fab packs (City Sample vehicles and
# crowd, Megascans), the DLSS and Streamline plugins, the imported models and mannequins, and the
# derived-data cache, so shaders are not compiled a second time. Directory junctions (no admin needed);
# the main checkout owns the files. Never run `git clean -x` in a worktree that has these.
#
# From the worktree's root:   powershell -File Tools\link_local_assets.ps1 [-Main <main checkout>]
param([string]$Main = "C:\Users\kenny\Documents\Dust-to-Glory")
$ErrorActionPreference = "Stop"
$here = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ($here.TrimEnd("\") -ieq (Resolve-Path $Main).Path.TrimEnd("\")) { "This is the main checkout; nothing to link."; exit 0 }
$folders = @(
    "Content\CitySampleVehicles", "Content\CitySampleCrowd", "Content\Fab", "Content\Megascans",
    "Content\Characters", "Content\Art\Models",
    "Plugins\DLSS", "Plugins\StreamlineCore", "Plugins\StreamlineDLSSG", "Plugins\StreamlineNGXCommon", "Plugins\StreamlineReflex",
    "DerivedDataCache")
foreach ($folder in $folders) {
    $source = Join-Path $Main $folder
    $target = Join-Path $here $folder
    if (-not (Test-Path $source)) { "skip (not in the main checkout): $folder"; continue }
    if (Test-Path $target) { "already there: $folder"; continue }
    New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
    cmd /c mklink /J "$target" "$source" | Out-Null
    "linked: $folder"
}
