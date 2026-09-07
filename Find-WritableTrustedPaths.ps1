# Find-WritableTrustedPaths.ps1
# Recursively walks AppLocker-trusted directories and reports
# every subdirectory the current user can write to.
# Outputs results ranked by usefulness (exec-friendly paths first).
# creds: https://radiantsec.io/docs/applocker/bypass-trusted-folders/

param(
    [string[]]$TrustedRoots = @(
        $env:WINDIR,
        $env:PROGRAMFILES,
        ${env:PROGRAMFILES(X86)}
    ),
    [int]$MaxDepth  = 4,
    [switch]$Quiet
)

function Test-Writable {
    param([string]$Path)
    $probe = Join-Path $Path ([System.IO.Path]::GetRandomFileName())
    try {
        [System.IO.File]::WriteAllBytes($probe, [byte[]]@(0x4D, 0x5A))
        Remove-Item $probe -Force -ErrorAction SilentlyContinue
        return $true
    } catch {
        return $false
    }
}

function Get-Depth {
    param([string]$Path, [string]$Root)
    $rel = $Path.Substring($Root.Length).TrimStart('\')
    return ($rel -split '\\').Count
}

$results = [System.Collections.Generic.List[PSCustomObject]]::new()

foreach ($root in $TrustedRoots | Where-Object { $_ -and (Test-Path $_) }) {
    if (-not $Quiet) { Write-Host "[*] scanning $root ..." -ForegroundColor Cyan }

    Get-ChildItem -Path $root -Recurse -Directory -ErrorAction SilentlyContinue |
    Where-Object {
        (Get-Depth $_.FullName $root) -le $MaxDepth
    } |
    ForEach-Object {
        if (Test-Writable $_.FullName) {
            # check if we can also create executables (some paths allow write but block .exe)
            $exeProbe = Join-Path $_.FullName "test_$(Get-Random).exe"
            $exeOk = $false
            try {
                [System.IO.File]::WriteAllBytes($exeProbe, [byte[]]@(0x4D,0x5A,0x90,0x00))
                Remove-Item $exeProbe -Force -ErrorAction SilentlyContinue
                $exeOk = $true
            } catch {}

            $results.Add([PSCustomObject]@{
                Path       = $_.FullName
                ExeDrop    = $exeOk
                Root       = $root
                Depth      = (Get-Depth $_.FullName $root)
            })
        }
    }
}

# rank: exe-droppable first, then by depth (shallower = less conspicuous)
$ranked = $results | Sort-Object -Property @(
    @{ Expression = "ExeDrop"; Descending = $true },
    @{ Expression = "Depth";   Descending = $false }
)

Write-Host "`n[+] Writable trusted paths ($($ranked.Count) found):`n" -ForegroundColor Green

$ranked | ForEach-Object {
    $tag = if ($_.ExeDrop) { "[EXE]" } else { "[WRT]" }
    $col = if ($_.ExeDrop) { "Yellow" } else { "Gray" }
    Write-Host "  $tag $($_.Path)" -ForegroundColor $col
}

# export CSV for offline analysis
$ranked | Export-Csv -Path ".\writable_trusted_paths.csv" -NoTypeInformation
Write-Host "`n[*] saved to writable_trusted_paths.csv" -ForegroundColor Cyan
