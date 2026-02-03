@echo off
REM Tiny64 – Windows launch script (SLIRP networking with port forwarding)
REM No admin required for SLIRP mode

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

REM Launch QEMU with SLIRP networking (user mode) - forwards UDP to host
"qemu-system-x86_64.exe" -m 256M ^
  -drive if=pflash,format=raw,unit=0,file="%OVMF_CODE%",readonly=on ^
  -drive if=pflash,format=raw,unit=1,file="%OVMF_VARS%" ^
  -drive file="%DISK_IMG%",format=raw,if=ide ^
  -serial stdio ^
  -netdev user,id=n0,hostfwd=udp::60001-:60001 ^
  -device e1000,netdev=n0

endlocal
