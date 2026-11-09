<# : DroidPlay for Windows: install or update (a batch file that runs the PowerShell below it, so a double-click works)
@echo off
powershell -NoProfile -ExecutionPolicy Bypass -Command "& ([scriptblock]::Create((Get-Content -LiteralPath '%~f0' -Raw -Encoding UTF8))) %*"
set code=%errorlevel%
if not "%code%"=="0" pause
exit /b %code%
#>
# DroidPlay for Windows (DroidPlay Core's desktop app, scripts/nxnative_android.py): installs or updates it for this user, then opens it.
#   Install DroidPlay.cmd              the newest DroidPlay Core release, into %LOCALAPPDATA%\DroidPlay; run it again to update
#   Install DroidPlay.cmd uninstall    removes that folder and the shortcuts; the games, the Android SDK and the tools the app installed stay
#   DROIDPLAY_REF=<branch or tag>      installs that instead of the newest release (testing one before it is released)
#   DROIDPLAY_NO_LAUNCH=1              installs without opening the app at the end (an install over SSH, which has no desktop to show it on)
# uv (astral.sh) brings Python and the app's modules from the PEP 723 block at the top of the app; the app's Settings > Install what's missing brings the rest.
$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue'
$Repo = 'enjoyerofslop/droidplay-core'
# the source's folder keeps the name it had before the project was renamed from droidnative (2026-11-09), so installs made before then update in place
$Home_ = Join-Path $env:LOCALAPPDATA 'DroidPlay'; $Src = Join-Path $Home_ 'droidnative'; $App = Join-Path $Src 'scripts\nxnative_android.py'
$Bin = Join-Path $env:USERPROFILE '.local\bin'; $Uv = Join-Path $Bin 'uv.exe'; $Uvw = Join-Path $Bin 'uvw.exe'
$Links = @((Join-Path ([Environment]::GetFolderPath('Programs')) 'DroidPlay.lnk'), (Join-Path ([Environment]::GetFolderPath('Desktop')) 'DroidPlay.lnk'))
function Say($m) { Write-Host "DroidPlay: $m" }
$Ua = 'droidplay-install'; $Tar = Join-Path $env:SystemRoot 'System32\tar.exe'   # Windows' own tar: a Git or MSYS tar earlier on PATH reads C: as a host name

# Downloads $url to $out with Windows' curl (Windows 10 1803 and later), which shows how far it has got and how fast. A release asset (its $size known) carries on
# from where a cut-off download stopped, on the next try or the next run of this file: on a slow or unsteady connection that is what gets it done at all.
function Fetch($url, $out, $size) {
    $curl = Join-Path $env:SystemRoot 'System32\curl.exe'
    if (-not (Test-Path $curl)) { Invoke-WebRequest $url -OutFile $out -Headers @{ 'User-Agent' = $Ua }; return }
    for ($try = 1; ; $try++) {
        if ($size -and (Test-Path $out)) {
            $len = (Get-Item -LiteralPath $out).Length
            if ($len -eq $size) { return }
            if ($len -gt $size) { Remove-Item -LiteralPath $out -Force }
        }
        $resume = if ($size) { @('-C', '-') } else { @() }
        # a download that stalls (under 1 KB/s for a minute) is cut and tried again, rather than left hanging
        & $curl -L --fail --retry 3 --connect-timeout 30 --speed-limit 1024 --speed-time 60 -A $Ua @resume -o $out $url
        $code = $LASTEXITCODE
        if ($code -eq 0) { return }
        $again = if ($size) { 'Run Install DroidPlay.cmd again to carry on from where it stopped.' } else { 'Run Install DroidPlay.cmd again.' }
        if ($code -eq 22 -or $try -ge 10) { throw "the download of $url stopped (curl exit $code). $again" }   # 22: an HTTP error, which waiting won't fix
        Say "the download stopped (curl exit $code); trying again"; Start-Sleep 5
    }
}

# Stops when DroidPlay runs from its folder (its window, or a build it started: their command lines name the folder). Replacing or removing the folder under it
# would delete the files of a program that is using them, and Windows lets that happen file by file before it refuses the folder itself, leaving it half gone.
$Open = 'DroidPlay is open, or one of its builds is running. Close it, then run Install DroidPlay.cmd again.'
function Stop-IfOpen {
    $me = @($PID, (Get-CimInstance Win32_Process -Filter "ProcessId=$PID").ParentProcessId)   # this file's own PowerShell and cmd, if it is run from that folder
    $busy = @(Get-CimInstance Win32_Process | Where-Object { $me -notcontains $_.ProcessId -and $_.CommandLine -and $_.CommandLine.IndexOf($Src, [StringComparison]::OrdinalIgnoreCase) -ge 0 })
    if ($busy) { throw $Open }
}

if ($args -contains 'uninstall') {
    Stop-IfOpen
    foreach ($l in $Links) { Remove-Item -LiteralPath $l -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $Home_ -Recurse -Force -ErrorAction SilentlyContinue
    Say "removed $Home_ and its shortcuts. Your games and their builds, the Android SDK ($env:LOCALAPPDATA\Android\Sdk) and the tools the app installed ($env:LOCALAPPDATA\nxnative) stay; delete those by hand if you want them gone."
    exit 0
}

# what to install: a branch or tag named in DROIDPLAY_REF, else the newest release. A release carries DroidPlay Core's source as assets (release-assets.yml), which
# GitHub serves from its download network and lets a download resume: droidplay-core-<version>.tar.xz (about 55 MB) where Windows' tar reads xz (11's does), else
# the .zip (about 90 MB). A branch, or a release without them, comes as the archive GitHub makes of it (about 130 MB, slower, and a cut-off download starts over).
$rel = $null
if ($env:DROIDPLAY_REF) { $ref = $env:DROIDPLAY_REF }
else { $rel = Invoke-RestMethod "https://api.github.com/repos/$Repo/releases/latest" -Headers @{ 'User-Agent' = $Ua }; $ref = $rel.tag_name }
$asset = $null
if ($rel) {
    $xz = (Test-Path $Tar) -and [bool](& $Tar --version | Select-String liblzma)
    $kinds = @(if ($xz) { 'tar.xz' }) + 'zip'
    # droidnative-<version>: the name of the assets before the rename, which a release made before it carries
    foreach ($k in $kinds) { foreach ($p in 'droidplay-core', 'droidnative') { $asset = @($rel.assets | Where-Object { $_.name -like "$p-*.$k" })[0]; if ($asset) { break } }; if ($asset) { break } }
}
$stamp = Join-Path $Home_ 'version.txt'; $have = if (Test-Path $stamp) { (Get-Content $stamp -Raw).Trim() } else { '' }
New-Item -ItemType Directory -Force $Home_ | Out-Null

if (-not ($rel -and $have -eq $ref -and (Test-Path $App))) {
    if ($asset) { $url = $asset.browser_download_url; $name = $asset.name; $size = $asset.size; $mb = [math]::Ceiling($size / 1MB) }
    else {
        $url = if ($rel) { "https://github.com/$Repo/archive/refs/tags/$ref.zip" } else { "https://github.com/$Repo/archive/$ref.zip" }
        $name = 'droidplay-core.zip'; $size = $null; $mb = 130
    }
    Stop-IfOpen
    Say ($(if ($have) { "updating from $have to $ref" } else { "installing $ref" }) + " (DroidPlay Core, about $mb MB)")
    # downloads keep a folder of their own, where a cut-off one waits for the next run to finish it; any other there (an older release's) goes
    $cache = Join-Path $Home_ 'downloads'; $dl = Join-Path $cache $name; $tmp = Join-Path $Home_ 'download'
    New-Item -ItemType Directory -Force $cache | Out-Null
    Get-ChildItem -LiteralPath $cache -File | Where-Object Name -ne $name | Remove-Item -Force
    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    Fetch $url $dl $size
    if ($asset.digest -match '^sha256:([0-9a-f]{64})$' -and (Get-FileHash -LiteralPath $dl -Algorithm SHA256).Hash -ne $Matches[1]) {
        Remove-Item -LiteralPath $dl -Force; throw "$name doesn't match the release's checksum, so it was deleted. Run Install DroidPlay.cmd again to download it again."
    }
    New-Item -ItemType Directory -Force $tmp | Out-Null
    if ($name -like '*.tar.xz') { & $Tar -xf $dl -C $tmp; if ($LASTEXITCODE) { throw "tar could not unpack $name (exit $LASTEXITCODE)" } }
    else { Add-Type -AssemblyName System.IO.Compression.FileSystem; [IO.Compression.ZipFile]::ExtractToDirectory($dl, $tmp) }
    $top = Get-ChildItem -LiteralPath $tmp -Directory | Select-Object -First 1
    if (-not (Test-Path (Join-Path $top.FullName 'scripts\nxnative_android.py'))) { throw "$name holds no scripts\nxnative_android.py" }
    # the old copy is moved aside whole, then deleted once the new one is in: a program that started during the download, or one Stop-IfOpen can't see, holds
    # the folder, so the move fails before anything is deleted (and a download of the release's assets stays for the next run)
    $old = Join-Path $Home_ 'droidnative.old'
    Remove-Item -LiteralPath $old -Recurse -Force -ErrorAction SilentlyContinue
    if (Test-Path $Src) { try { Rename-Item -LiteralPath $Src -NewName 'droidnative.old' } catch { throw $Open } }
    try { Move-Item -LiteralPath $top.FullName $Src }
    catch { if (Test-Path $old) { Rename-Item -LiteralPath $old -NewName 'droidnative' }; throw }   # the old copy goes back rather than leave none
    Remove-Item -LiteralPath $old, $tmp, $dl -Recurse -Force -ErrorAction SilentlyContinue
    Set-Content -LiteralPath $stamp $ref
} else { Say "$ref is installed" }

if (-not (Test-Path $Uv)) {
    Say 'installing uv (astral.sh), which brings Python'
    $env:UV_NO_MODIFY_PATH = '1'; Invoke-RestMethod https://astral.sh/uv/install.ps1 | Invoke-Expression
}
Say 'getting Python and the app''s modules ready (the first time downloads about 150 MB)'
& $Uv sync --script $App; if ($LASTEXITCODE) { throw "uv could not prepare the app's Python (exit $LASTEXITCODE)" }
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
