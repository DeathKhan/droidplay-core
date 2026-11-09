<# : DroidPlay for Windows: install or update (a batch file that runs the PowerShell below it, so a double-click works)
@echo off
powershell -NoProfile -ExecutionPolicy Bypass -Command "& ([scriptblock]::Create((Get-Content -LiteralPath '%~f0' -Raw -Encoding UTF8))) %*"
set code=%errorlevel%
if not "%code%"=="0" pause
exit /b %code%
#>
# DroidPlay for Windows (droidnative's desktop app, scripts/nxnative_android.py): installs or updates it for this user, then opens it.
#   Install DroidPlay.cmd              the newest droidnative release, into %LOCALAPPDATA%\DroidPlay; run it again to update
#   Install DroidPlay.cmd uninstall    removes that folder and the shortcuts; the games, the Android SDK and the tools the app installed stay
#   DROIDPLAY_REF=<branch or tag>      installs that instead of the newest release (testing one before it is released)
#   DROIDPLAY_NO_LAUNCH=1              installs without opening the app at the end (an install over SSH, which has no desktop to show it on)
# uv (astral.sh) brings Python and the app's modules from the PEP 723 block at the top of the app; the app's Settings > Install what's missing brings the rest.
$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue'
$Repo = 'enjoyerofslop/droidnative'
$Home_ = Join-Path $env:LOCALAPPDATA 'DroidPlay'; $Src = Join-Path $Home_ 'droidnative'; $App = Join-Path $Src 'scripts\nxnative_android.py'
$Bin = Join-Path $env:USERPROFILE '.local\bin'; $Uv = Join-Path $Bin 'uv.exe'; $Uvw = Join-Path $Bin 'uvw.exe'
$Links = @((Join-Path ([Environment]::GetFolderPath('Programs')) 'DroidPlay.lnk'), (Join-Path ([Environment]::GetFolderPath('Desktop')) 'DroidPlay.lnk'))
function Say($m) { Write-Host "DroidPlay: $m" }

if ($args -contains 'uninstall') {
    foreach ($l in $Links) { Remove-Item -LiteralPath $l -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $Home_ -Recurse -Force -ErrorAction SilentlyContinue
    Say "removed $Home_ and its shortcuts. Your games and their builds, the Android SDK ($env:LOCALAPPDATA\Android\Sdk) and the tools the app installed ($env:LOCALAPPDATA\nxnative) stay; delete those by hand if you want them gone."
    exit 0
}

# what to install: a branch or tag named in DROIDPLAY_REF, else the newest release
if ($env:DROIDPLAY_REF) { $ref = $env:DROIDPLAY_REF; $zip = "https://github.com/$Repo/archive/$ref.zip"; $fixed = $false }
else {
    $ref = (Invoke-RestMethod "https://api.github.com/repos/$Repo/releases/latest" -Headers @{ 'User-Agent' = 'droidplay-install' }).tag_name
    $zip = "https://github.com/$Repo/archive/refs/tags/$ref.zip"; $fixed = $true
}
$stamp = Join-Path $Home_ 'version.txt'; $have = if (Test-Path $stamp) { (Get-Content $stamp -Raw).Trim() } else { '' }
New-Item -ItemType Directory -Force $Home_ | Out-Null

if (-not ($fixed -and $have -eq $ref -and (Test-Path $App))) {
    Say ($(if ($have) { "updating from $have to $ref" } else { "installing $ref" }) + ' (droidnative, about 125 MB)')
    $dl = Join-Path $Home_ 'download.zip'; $tmp = Join-Path $Home_ 'download'
    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    Invoke-WebRequest $zip -OutFile $dl -Headers @{ 'User-Agent' = 'droidplay-install' }
    Add-Type -AssemblyName System.IO.Compression.FileSystem; [IO.Compression.ZipFile]::ExtractToDirectory($dl, $tmp)
    $top = Get-ChildItem -LiteralPath $tmp -Directory | Select-Object -First 1
    if (-not (Test-Path (Join-Path $top.FullName 'scripts\nxnative_android.py'))) { throw "$zip holds no scripts\nxnative_android.py" }
    if (Test-Path $Src) { Remove-Item -LiteralPath $Src -Recurse -Force }
    Move-Item -LiteralPath $top.FullName $Src
    Remove-Item -LiteralPath $tmp, $dl -Recurse -Force -ErrorAction SilentlyContinue
    Set-Content -LiteralPath $stamp $ref
} else { Say "$ref is installed" }

if (-not (Test-Path $Uv)) {
    Say 'installing uv (astral.sh), which brings Python'
    $env:UV_NO_MODIFY_PATH = '1'; Invoke-RestMethod https://astral.sh/uv/install.ps1 | Invoke-Expression
}
Say 'getting Python and the app''s modules ready (the first time downloads about 150 MB)'
& $Uv sync --quiet --script $App; if ($LASTEXITCODE) { throw "uv could not prepare the app's Python (exit $LASTEXITCODE)" }
$ico = Join-Path $Home_ 'DroidPlay.ico'
$env:QT_QPA_PLATFORM = 'offscreen'   # the icon is drawn without a window: no display needed, and no DPI warning
& $Uv run --quiet --script $App --write-icon $ico; $code = $LASTEXITCODE; Remove-Item Env:QT_QPA_PLATFORM
if ($code) { throw "the app could not write its icon (exit $code)" }

$sh = New-Object -ComObject WScript.Shell
foreach ($l in $Links) {
    $s = $sh.CreateShortcut($l); $s.TargetPath = $Uvw; $s.Arguments = "run --gui-script `"$App`""; $s.WorkingDirectory = $Src
    $s.IconLocation = $ico; $s.Description = 'DroidPlay for Windows: pack your Switch games for Android'; $s.Save()
}
Say "ready: DroidPlay is in the Start menu and on the desktop. Opening it; in its Settings, pick your prod.keys, then Install what's missing."
if (-not $env:DROIDPLAY_NO_LAUNCH) { Start-Process -FilePath $Uvw -ArgumentList @('run', '--gui-script', "`"$App`"") -WorkingDirectory $Src }
