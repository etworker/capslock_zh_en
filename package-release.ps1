param(
    [string]$Version = ""
)

if (-not $Version) {
    $Version = git describe --tags --abbrev=0 2>$null
    if (-not $Version) {
        $Version = "v0.0.0"
    }
}

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$OutDir = Join-Path $RepoRoot "release"

Write-Host "Packaging version $Version for release ..." -ForegroundColor Cyan

# Ensure output directory
if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

# ─── C ───
$cFiles = @(
    (Join-Path $RepoRoot "bin\c\install.bat"),
    (Join-Path $RepoRoot "bin\c\uninstall.bat"),
    (Join-Path $RepoRoot "bin\c\CapsLockZhEn.exe")
)
$cZip = Join-Path $OutDir "capslock_zh_en_c-$Version.zip"
if ((Test-Path $cFiles[0]) -and (Test-Path $cFiles[2])) {
    Compress-Archive -Path $cFiles -DestinationPath $cZip -Force
    Write-Host "  [OK] $cZip"
} else {
    Write-Host "  [SKIP] C build files not found" -ForegroundColor Yellow
}

# ─── C# ───
$csFiles = @(
    (Join-Path $RepoRoot "bin\csharp\install.bat"),
    (Join-Path $RepoRoot "bin\csharp\uninstall.bat"),
    (Join-Path $RepoRoot "bin\csharp\CapsLockZhEn.exe")
)
$csZip = Join-Path $OutDir "capslock_zh_en_csharp-$Version.zip"
if ((Test-Path $csFiles[0]) -and (Test-Path $csFiles[2])) {
    Compress-Archive -Path $csFiles -DestinationPath $csZip -Force
    Write-Host "  [OK] $csZip"
} else {
    Write-Host "  [SKIP] C# build files not found" -ForegroundColor Yellow
}

# ─── AHK ───
$ahkFiles = @(
    (Join-Path $RepoRoot "ahk\install.bat"),
    (Join-Path $RepoRoot "ahk\uninstall.bat"),
    (Join-Path $RepoRoot "ahk\CapsLockZhEn.ahk")
)
$ahkZip = Join-Path $OutDir "capslock_zh_en_ahk-$Version.zip"
if (Test-Path $ahkFiles[2]) {
    Compress-Archive -Path $ahkFiles -DestinationPath $ahkZip -Force
    Write-Host "  [OK] $ahkZip"
} else {
    Write-Host "  [SKIP] AHK script not found" -ForegroundColor Yellow
}

Write-Host "Done." -ForegroundColor Cyan
