# Run this script elevated (as admin) to set up the reload task
# Right-click -> Run as Administrator

$ErrorActionPreference = "Stop"

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "Not running elevated. Relaunching as admin..."
    $argList = "-ExecutionPolicy Bypass -File `"$PSCommandPath`""
    Start-Process -FilePath "powershell.exe" -ArgumentList $argList -Verb RunAs
    exit
}

$taskName = "Reload Illustrator Ductwork"
$scriptPath = Join-Path $PSScriptRoot "reload-illustrator.ps1"

Write-Host "Setting up scheduled task: $taskName"
Write-Host "Script path: $scriptPath"

# Remove existing task if present
Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue

# Create task components
$action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument "-ExecutionPolicy Bypass -File `"$scriptPath`""
$principal = New-ScheduledTaskPrincipal -UserId $env:USERNAME -RunLevel Highest -LogonType Interactive
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Minutes 10)

$task = New-ScheduledTask -Action $action -Principal $principal -Settings $settings -Description "Build and reload Process Ductwork plugin"

# Register the task
Register-ScheduledTask -TaskName $taskName -InputObject $task -Force | Out-Null

Write-Host ""
Write-Host "Task '$taskName' created successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "You can now use the shortcut or run:"
Write-Host "  schtasks /run /tn `"$taskName`""
Write-Host ""
Write-Host "Press any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
