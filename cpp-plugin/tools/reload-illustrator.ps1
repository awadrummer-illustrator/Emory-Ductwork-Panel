$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$taskName = "Reload Illustrator Ductwork"
$requestFile = Join-Path $env:TEMP "ductwork-reload-request.txt"

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
	$scheduledTask = Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
	if ($scheduledTask) {
		Write-Host "[Reload] Not running elevated. Invoking scheduled task '$taskName'..."
		Set-Content -Path $requestFile -Value $PSCommandPath -Encoding UTF8
		Start-ScheduledTask -TaskName $taskName
		exit
	}

	Write-Host "[Reload] Not running elevated and scheduled task '$taskName' was not found. Relaunching as admin..."
	$argList = "-ExecutionPolicy Bypass -File `"$PSCommandPath`""
	Start-Process -FilePath "powershell.exe" -ArgumentList $argList -Verb RunAs
	exit
}

$pluginSource = Join-Path $root "build\win\x64\Release\EmoryDuctwork.aip"
$pluginDestDir = "C:\Program Files\Adobe\Adobe Illustrator 2024\Plug-ins\DuctworkMenu"
$testFile = "E:\Work\Work\Floorplans\Test Emory Ductwork.ai"
$illustratorExe = "C:\Program Files\Adobe\Adobe Illustrator 2024\Support Files\Contents\Windows\Illustrator.exe"

function Get-LatestRecentIllustratorFile {
	$recentDir = Join-Path $env:APPDATA "Microsoft\Windows\Recent"
	if (!(Test-Path $recentDir)) {
		return $null
	}

	$shell = New-Object -ComObject WScript.Shell
	try {
		$resolved = Get-ChildItem -Path $recentDir -Filter *.lnk -File -ErrorAction SilentlyContinue |
			Sort-Object LastWriteTime -Descending |
			ForEach-Object {
				try {
					$shortcut = $shell.CreateShortcut($_.FullName)
					$targetPath = $shortcut.TargetPath
					if ([string]::IsNullOrWhiteSpace($targetPath)) {
						return
					}
					if (!(Test-Path $targetPath)) {
						return
					}
					if ([System.IO.Path]::GetExtension($targetPath).ToLowerInvariant() -ne ".ai") {
						return
					}

					[PSCustomObject]@{
						ShortcutPath = $_.FullName
						TargetPath   = $targetPath
						LastWriteTime = $_.LastWriteTime
					}
				} catch {
				}
			} |
			Select-Object -First 1

		if ($resolved) {
			return $resolved.TargetPath
		}
		return $null
	} finally {
		[System.Runtime.Interopservices.Marshal]::ReleaseComObject($shell) | Out-Null
	}
}

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
$launchFile = Get-LatestRecentIllustratorFile
if ([string]::IsNullOrWhiteSpace($launchFile)) {
	$launchFile = $testFile
}

if (-not [string]::IsNullOrWhiteSpace($launchFile) -and (Test-Path $launchFile)) {
	Write-Host "[Reload] Opening file: $launchFile"
	Start-Process -FilePath $illustratorExe -ArgumentList "`"$launchFile`""
} else {
	Write-Warning "[Reload] No recent Illustrator file found and fallback file is missing. Launching without a document."
	Start-Process -FilePath $illustratorExe
}

