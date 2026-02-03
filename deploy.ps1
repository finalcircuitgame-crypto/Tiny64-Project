# Tiny64 USB Deployment Script
# This script copies the compiled UEFI bootloader and Kernel to a physical USB drive.

# --- CONFIGURATION ---
# Change this to your USB drive letter (e.g., "E:", "F:", etc.)
$USB_DRIVE = "G:" 

if (-not (Test-Path $USB_DRIVE)) {
    Write-Host "USB Drive $USB_DRIVE not found!" -ForegroundColor Red
    Write-Host "Please edit this script ($PSCommandPath) and set `$USB_DRIVE to your correct drive letter." -ForegroundColor Yellow
    exit
}

Write-Host "🚀 Syncing Tiny64 to USB Drive $USB_DRIVE..." -ForegroundColor Cyan

# 0. Clear the drive
Write-Host "🧹 Clearing $USB_DRIVE..." -ForegroundColor Gray
Get-ChildItem -Path $USB_DRIVE -Force | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue

# Define paths relative to this script's location (tools/ folder)
$ROOT = Split-Path $PSScriptRoot -Parent
$BOOT_EFI = Join-Path $ROOT "build\boot\BOOTX64.EFI"
$KERNEL_ELF = Join-Path $ROOT "build\kernel\kernel.elf"

# Check if build artifacts exist
if (-not (Test-Path $BOOT_EFI)) {
    Write-Host "Error: $BOOT_EFI not found." -ForegroundColor Red
    Write-Host "Make sure you have built the project using 'wsl make' first." -ForegroundColor Yellow
    exit
}

# 1. Ensure EFI directory structure exists on the USB drive
$EFI_PATH = "$USB_DRIVE\EFI\BOOT"
if (-not (Test-Path $EFI_PATH)) { 
    Write-Host "Creating $EFI_PATH..." -ForegroundColor Gray
    New-Item -Path $EFI_PATH -ItemType Directory -Force | Out-Null
}

# 2. Copy artifacts
try {
    Write-Host "Copying BOOTX64.EFI..." -NoNewline -ForegroundColor Gray
    # Robocopy is much more robust for USB transfers
    robocopy (Split-Path $BOOT_EFI) $EFI_PATH (Split-Path $BOOT_EFI -Leaf) /NJH /NJS /NDL /NC /NS /NP /R:0 /W:0 | Out-Null
    Write-Host " [OK]" -ForegroundColor Green

    Write-Host "Copying kernel.elf..." -ForegroundColor Gray
    if (Test-Path $KERNEL_ELF) {
        # /J = Unbuffered I/O (Recommended for USB/Large files to prevent OS buffering hangs)
        # /IS = Include Same (Forces overwrite even if the file looks identical)
        robocopy (Split-Path $KERNEL_ELF) $USB_DRIVE (Split-Path $KERNEL_ELF -Leaf) /R:0 /W:0 /V /J /IS
        if ($LASTEXITCODE -le 3) {
            Write-Host "kernel.elf Copy [OK]" -ForegroundColor Green
        } else {
            Write-Host "kernel.elf Copy [FAILED] - Exit Code: $LASTEXITCODE" -ForegroundColor Red
        }
    } else {
        Write-Host "kernel.elf [MISSING]" -ForegroundColor Red
    }

    Write-Host "Finalizing USB Write..." -NoNewline -ForegroundColor Gray
    # Refreshing the shell namespace is the safest non-admin way to signal a write completion
    $shell = New-Object -ComObject Shell.Application
    $shell.Namespace($USB_DRIVE).Self.InvokeVerb("Standard.Refresh")
    Write-Host " [OK]" -ForegroundColor Green

    Write-Host "`n✅ Deployment successful!" -ForegroundColor Green
}
catch {
    Write-Host "`n❌ Failed to copy files: $($_.Exception.Message)" -ForegroundColor Red
}
