$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath 'build/x64/WindowTidy.exe').Path
$process = $null
try {
    $process = Start-Process -FilePath $exe -PassThru -ErrorAction Stop
    Start-Sleep -Seconds 3
    $process.Refresh()
    if ($process.HasExited) {
        throw "WindowTidy.exe exited unexpectedly with code $($process.ExitCode)"
    }
    Write-Host "Window Tidy startup smoke test passed (PID $($process.Id))."
} finally {
    if ($null -ne $process) {
        $process.Refresh()
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            $process.WaitForExit(3000) | Out-Null
        }
    }
}
