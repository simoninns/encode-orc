# Test runner script for encode-orc YAML projects
# Runs all test YAML files and outputs results to test-output directory

$ErrorActionPreference = "Stop"

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = $ScriptDir

# Directories
$TestProjectsDir = Join-Path $ProjectRoot "example-projects"
$TestOutputDir = Join-Path $ProjectRoot "example-output"
$BuildDir = Join-Path $ProjectRoot "build"

# Determine executable location
$EncodeOrc = $null
$PossiblePaths = @(
    (Join-Path $BuildDir "Release\encode-orc.exe"),
    (Join-Path $BuildDir "encode-orc.exe"),
    (Join-Path $BuildDir "encode-orc")
)

foreach ($path in $PossiblePaths) {
    if (Test-Path $path) {
        $EncodeOrc = $path
        break
    }
}

if (-not $EncodeOrc) {
    Write-Host "Error: encode-orc executable not found. Build may have failed." -ForegroundColor Red
    Write-Host "Checked:" -ForegroundColor Red
    foreach ($path in $PossiblePaths) {
        Write-Host "  - $path" -ForegroundColor Red
    }
    exit 1
}

# Create output directory if it doesn't exist
New-Item -ItemType Directory -Force -Path $TestOutputDir | Out-Null

# Count tests
$totalTests = 0
$passedTests = 0
$failedTests = 0

Write-Host "========================================" -ForegroundColor Blue
Write-Host "  encode-orc YAML Project Test Suite" -ForegroundColor Blue
Write-Host "========================================" -ForegroundColor Blue
Write-Host ""

# Find all YAML files in test-projects directory
if (-not (Test-Path $TestProjectsDir)) {
    Write-Host "Error: test-projects directory not found at $TestProjectsDir" -ForegroundColor Red
    exit 1
}

$yamlFiles = Get-ChildItem -Path $TestProjectsDir -Filter *.yaml

if ($yamlFiles.Count -eq 0) {
    Write-Host "Error: No YAML test files found in $TestProjectsDir" -ForegroundColor Red
    exit 1
}

# Run each test
foreach ($yamlFile in $yamlFiles) {
    $totalTests++
    $filename = $yamlFile.Name
    
    Write-Host "Test ${totalTests}: $filename ... " -NoNewline
    
    # Run encode-orc with YAML file
    $output = & $EncodeOrc $yamlFile.FullName --quiet 2>&1
    $exitCode = $LASTEXITCODE
    
    if ($exitCode -eq 0) {
        Write-Host "PASSED" -ForegroundColor Green
        $passedTests++
        
        # Extract output filename from YAML to verify it was created
        $content = Get-Content $yamlFile.FullName -Raw
        if ($content -match 'filename:\s*"([^"]*)"') {
            $outputFile = $matches[1]
            if (Test-Path $outputFile) {
                $fileSize = (Get-Item $outputFile).Length
                $fileSizeFormatted = if ($fileSize -gt 1MB) {
                    "{0:N2} MB" -f ($fileSize / 1MB)
                } elseif ($fileSize -gt 1KB) {
                    "{0:N2} KB" -f ($fileSize / 1KB)
                } else {
                    "$fileSize bytes"
                }
                Write-Host "  Output: $outputFile ($fileSizeFormatted)"
            }
        }
    } else {
        Write-Host "FAILED" -ForegroundColor Red
        $failedTests++
        # Show error output if any
        if ($output) {
            $output | ForEach-Object { Write-Host "  $_" }
        }
    }
}

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Blue
Write-Host "Test Summary" -ForegroundColor Blue
Write-Host "========================================" -ForegroundColor Blue
Write-Host "Total tests:  $totalTests"
if ($passedTests -gt 0) {
    Write-Host "Passed:       $passedTests" -ForegroundColor Green
} else {
    Write-Host "Passed:       $passedTests"
}
if ($failedTests -gt 0) {
    Write-Host "Failed:       $failedTests" -ForegroundColor Red
} else {
    Write-Host "Failed:       $failedTests" -ForegroundColor Green
}
Write-Host ""

if ($failedTests -eq 0) {
    Write-Host "All tests passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "Some tests failed." -ForegroundColor Red
    exit 1
}
