$ErrorActionPreference = 'Stop'
$target = 'build/x64/WindowTidy.exe'
if (-not (Test-Path -LiteralPath $target -PathType Leaf)) {
    throw "Executable missing: $target"
}
$imports = @(& dumpbin.exe /dependents $target)
if ($LASTEXITCODE -ne 0) {
    throw 'dumpbin.exe could not read PE imports.'
}
$bad = @($imports | Select-String -Pattern '(?i)(vcruntime[0-9_]*|msvcp[0-9_]*|ucrtbase|api-ms-win-crt)[\w.-]*\.dll')
if ($bad.Count -gt 0) {
    throw "Unexpected dynamic MSVC runtime: $($bad -join '; ')"
}
$dependencies = @($imports | Select-String -Pattern '(?i)\.dll')
Write-Host 'Portable executable dependencies:'
$dependencies | ForEach-Object { Write-Host $_.Line.Trim() }
if ($dependencies.Count -eq 0) {
    throw 'Could not enumerate PE dependencies.'
}
