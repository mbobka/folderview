[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Release',

    [string] $Version = '',

    [string] $ZipName = '',

    [string] $OutputDir = 'out-win-amd64',

    [switch] $NoCache
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = [System.IO.Path]::GetFullPath((Resolve-Path (Join-Path $PSScriptRoot '..')).Path)
$outputPath = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
    [System.IO.Path]::GetFullPath($OutputDir)
}
else {
    [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDir))
}

$repoRootWithSeparator = $repoRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if (-not $outputPath.StartsWith($repoRootWithSeparator, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDir must be inside repository: $outputPath"
}

if (Test-Path -LiteralPath $outputPath) {
    Get-ChildItem -LiteralPath $outputPath -Force | ForEach-Object {
        Remove-Item -LiteralPath $_.FullName -Recurse -Force
    }
}
else {
    New-Item -ItemType Directory -Path $outputPath | Out-Null
}

$env:DOCKER_BUILDKIT = '1'

$dockerfilePath = Join-Path $PSScriptRoot 'Dockerfile.win-amd64'

$dockerArgs = @(
    'build',
    '--platform', 'linux/amd64',
    '-f', $dockerfilePath,
    '--target', 'artifacts',
    '--output', "type=local,dest=$outputPath",
    '--build-arg', "CONFIGURATION=$Configuration"
)

if ($Version.Trim().Length -gt 0) {
    $dockerArgs += @('--build-arg', "VERSION=$($Version.Trim())")
}

if ($ZipName.Trim().Length -gt 0) {
    $dockerArgs += @('--build-arg', "ZIP_NAME=$($ZipName.Trim())")
}

if ($NoCache) {
    $dockerArgs += '--no-cache'
}

$dockerArgs += $repoRoot

& docker @dockerArgs
if ($LASTEXITCODE -ne 0) {
    throw "docker build failed with exit code $LASTEXITCODE"
}
