$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
	Write-Host "[Reload] Not running elevated. Relaunching as admin..."
	$argList = "-ExecutionPolicy Bypass -File `"$PSCommandPath`""
	Start-Process -FilePath "powershell.exe" -ArgumentList $argList -Verb RunAs
	exit
}

$pluginSource = Join-Path $root "build\win\x64\Release\EmoryDuctwork.aip"
$pluginDestDir = "C:\Program Files\Adobe\Adobe Illustrator 2024\Plug-ins\DuctworkMenu"
$testFile = "E:\Work\Work\Floorplans\Test Emory Ductwork.ai"
$illustratorExe = "C:\Program Files\Adobe\Adobe Illustrator 2024\Support Files\Contents\Windows\Illustrator.exe"

$projectFile = Join-Path $root "src\ProcessDuctwork\EmoryDuctwork.vcxproj"
$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if (!(Test-Path $msbuild)) {
	$msbuild = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
}
if (!(Test-Path $msbuild)) {
	$msbuild = "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
}
if (!(Test-Path $msbuild)) {
	throw "MSBuild not found - install VS 2022 BuildTools or Community"
}
Write-Host "[Reload] Building plugin..."
& $msbuild $projectFile /p:Configuration=Release /p:Platform=x64 | Out-Host
if ($LASTEXITCODE -ne 0) {
	throw "MSBuild failed with exit code $LASTEXITCODE"
}

Write-Host "[Reload] Closing Illustrator if running..."
$aiProcess = Get-Process -Name "Illustrator" -ErrorAction SilentlyContinue
if ($aiProcess) {
	$aiProcess | Stop-Process -Force
	$deadline = (Get-Date).AddSeconds(60)
	while ((Get-Process -Name "Illustrator" -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) {
		Start-Sleep -Milliseconds 500
	}
}

New-Item -ItemType Directory -Force -Path $pluginDestDir | Out-Null

$maxWaitSeconds = 30
$start = Get-Date
$lastCopyError = $null
while ($true) {
	try {
		Write-Host "[Reload] Copying plugin to $pluginDestDir..."
		Copy-Item $pluginSource $pluginDestDir -Force
		break
	} catch {
		$lastCopyError = $_
		if ((Get-Date) -gt $start.AddSeconds($maxWaitSeconds)) {
			if ($lastCopyError) {
				Write-Error "[Reload] Copy failed: $($lastCopyError.Exception.Message)"
			}
			throw
		}
		Start-Sleep -Milliseconds 500
	}
}

$cepSource = "E:\Work\Work\Custom Sketchup, Illustrator and Photoshop Scripts and Extensions\Illustrator\Extensions\Emory-Ductwork-Panel"
$cepDest = Join-Path $env:APPDATA "Adobe\CEP\extensions\Emory-Ductwork-Panel"
New-Item -ItemType Directory -Force -Path $cepDest | Out-Null

$robocopyArgs = @(
	$cepSource,
	$cepDest,
	"/MIR",
	"/XD", ".git", ".claude", ".vscode", "cpp-plugin", "logs", "node_modules",
	"/XF", "*.log", "README.md", "AI-INFO.md", "ARCHITECTURE.md", "DEPLOYMENT_INSTRUCTIONS.md", "js\debug-location.jsx"
)
Write-Host "[Reload] Syncing CEP extension..."
& robocopy @robocopyArgs | Out-Host
$robocopyExit = $LASTEXITCODE
if ($robocopyExit -ge 16) {
	throw "Robocopy failed with exit code $robocopyExit"
}
if ($robocopyExit -ge 8) {
	Write-Warning "Robocopy reported issues (exit code $robocopyExit). Continuing to launch Illustrator."
}

Write-Host "[Reload] Launching Illustrator..."
Start-Process -FilePath $illustratorExe -ArgumentList "`"$testFile`""

