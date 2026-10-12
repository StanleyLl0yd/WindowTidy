$ErrorActionPreference = 'Stop'
$source = 'res/WindowTidy.source.png'
$ico = 'res/WindowTidy.ico'
$exe = 'build/x64/WindowTidy.exe'
$expectedSourceHash = '74776d47df0491fc22fab6a57890d4e36dd284ff39ed07d3c614ad24081e9051'
$expectedIcoHash = '872946daaa4be471c8e540e121f4fbe8b08a097c6c42b3328e725c3fe938813f'
$expectedSizes = @(16, 20, 24, 32, 40, 48, 64, 96, 128, 256)

foreach ($path in @($source, $ico, $exe)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing required icon/build file: $path" }
}
$assets = @(
    [pscustomobject]@{ Path = $source; ExpectedHash = $expectedSourceHash }
    [pscustomobject]@{ Path = $ico; ExpectedHash = $expectedIcoHash }
)
foreach ($asset in $assets) {
    $actual = (Get-FileHash -LiteralPath $asset.Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $asset.ExpectedHash) { throw "Icon asset SHA-256 mismatch: $($asset.Path)" }
}

$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $ico).Path)
if ($bytes.Length -lt 6 -or [BitConverter]::ToUInt16($bytes, 0) -ne 0 -or
    [BitConverter]::ToUInt16($bytes, 2) -ne 1) { throw 'Invalid ICO header.' }
$count = [BitConverter]::ToUInt16($bytes, 4)
if ($count -ne $expectedSizes.Count -or $bytes.Length -lt 6 + 16 * $count) {
    throw "Wrong number of ICO images: $count"
}
$actualSizes = @()
$pngSignature = [byte[]](137,80,78,71,13,10,26,10)
for ($i = 0; $i -lt $count; $i++) {
    $offset = 6 + 16 * $i
    $size = [int]$bytes[$offset]
    if ($size -eq 0) { $size = 256 }
    $height = [int]$bytes[$offset + 1]
    if ($height -eq 0) { $height = 256 }
    if ($height -ne $size) { throw "ICO entry $i is not square." }
    $length = [BitConverter]::ToUInt32($bytes, $offset + 8)
    $imageOffset = [BitConverter]::ToUInt32($bytes, $offset + 12)
    if ($length -lt 8 -or ($imageOffset + $length) -gt $bytes.Length) {
        throw "ICO entry $i points outside the file."
    }
    for ($j = 0; $j -lt 8; $j++) {
        if ($bytes[$imageOffset + $j] -ne $pngSignature[$j]) {
            throw "ICO entry $i is not a PNG raster."
        }
    }
    $actualSizes += $size
}
if (($actualSizes -join ',') -ne ($expectedSizes -join ',')) {
    throw "Unexpected ICO image sizes: $($actualSizes -join ',')"
}

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class IconResources {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadLibraryExW(string filename, IntPtr file, uint flags);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr FindResourceW(IntPtr module, IntPtr name, IntPtr type);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr LockResource(IntPtr resource);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint SizeofResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool FreeLibrary(IntPtr module);
}
'@
$module = [IconResources]::LoadLibraryExW((Resolve-Path -LiteralPath $exe).Path,
    [IntPtr]::Zero, 2) # LOAD_LIBRARY_AS_DATAFILE
if ($module -eq [IntPtr]::Zero) { throw 'Could not load the executable resource table.' }
try {
    $resource = [IconResources]::FindResourceW($module, [IntPtr]100, [IntPtr]14) # RT_GROUP_ICON
    if ($resource -eq [IntPtr]::Zero) { throw 'Embedded application icon group #100 missing.' }
    $length = [IconResources]::SizeofResource($module, $resource)
    $handle = [IconResources]::LoadResource($module, $resource)
    if ($handle -eq [IntPtr]::Zero) { throw 'Cannot load embedded icon group.' }
    $pointer = [IconResources]::LockResource($handle)
    if ($pointer -eq [IntPtr]::Zero) { throw 'Cannot access embedded icon group.' }
    $embeddedCount = [Runtime.InteropServices.Marshal]::ReadInt16($pointer, 4)
    if ($embeddedCount -ne $expectedSizes.Count -or $length -lt 6 + 14 * $embeddedCount) {
        throw "Unexpected number of embedded application icon images: $embeddedCount"
    }
    Write-Host "Verified approved icon: $count PNG frames, sizes $($actualSizes -join ', '), embedded in EXE."
} finally {
    [void][IconResources]::FreeLibrary($module)
}
