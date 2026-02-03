@echo off
REM Tiny64 – Windows launch script (TAP networking)
REM Run in an elevated PowerShell / Command Prompt because opening TAP requires admin.

setlocal enabledelayedexpansion

REM Resolve script directory
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

REM Build first (WSL make) if desired – comment out if you build separately
wsl bash -c "cd '$(wslpath '%SCRIPT_DIR%')' && make image"

REM Paths
set OVMF_CODE=%SCRIPT_DIR%OVMF_CODE.fd
set OVMF_VARS=%SCRIPT_DIR%build\OVMF_VARS.fd
set DISK_IMG=%SCRIPT_DIR%build\disk.img

REM Launch QEMU with TAP adapter tap0 (created via TAP-Windows driver)
"qemu-system-x86_64.exe" -m 256M ^
  -drive if=pflash,format=raw,unit=0,file="%OVMF_CODE%",readonly=on ^
  -drive if=pflash,format=raw,unit=1,file="%OVMF_VARS%" ^
  -drive file="%DISK_IMG%",format=raw,if=ide ^
  -serial stdio ^
  -netdev tap,id=n0,ifname=tap0,script=no,downscript=no ^
  -device e1000,netdev=n0

endlocal
