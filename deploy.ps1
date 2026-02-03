Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# --- DARK THEME COLORS ---
$BgColor = [System.Drawing.Color]::FromArgb(30, 30, 35)
$HeaderColor = [System.Drawing.Color]::FromArgb(45, 45, 52)
$AccentColor = [System.Drawing.Color]::FromArgb(0, 122, 204) # Visual Studio Blue
$TextColor = [System.Drawing.Color]::White
$MutedText = [System.Drawing.Color]::FromArgb(180, 180, 180)
$ButtonColor = [System.Drawing.Color]::FromArgb(0, 120, 212)
$RedColor = [System.Drawing.Color]::FromArgb(200, 50, 50)

# --- LOCK DETECTION FUNCTION ---
function Get-LockingProcess {
    param([string]$path)
    $res = New-Object System.Collections.Generic.List[string]
    $type = $null
    try { $type = [Win32.RestartManager] } catch { }
    if ($null -eq $type) {
        $signature = @'
        [DllImport("rstrtmgr.dll", CharSet=CharSet.Unicode)]
        public static extern int RmStartSession(out uint pSessionHandle, uint dwSessionFlags, string strSessionKey);
        [DllImport("rstrtmgr.dll")]
        public static extern int RmEndSession(uint dwSessionHandle);
        [DllImport("rstrtmgr.dll", CharSet=CharSet.Unicode)]
        public static extern int RmRegisterResources(uint dwSessionHandle, uint nFiles, string[] rgsFilenames, uint nApplications, IntPtr rgApplications, uint nServices, IntPtr rgsServiceNames);
        [DllImport("rstrtmgr.dll")]
        public static extern int RmGetList(uint dwSessionHandle, out uint pnProcInfoNeeded, ref uint pnProcInfo, [In, Out] RM_PROCESS_INFO[] rgProcInfo, ref uint lpdwRebootReasons);
        [StructLayout(LayoutKind.Sequential)]
        public struct RM_UNIQUE_PROCESS { public int dwProcessId; public System.Runtime.InteropServices.ComTypes.FILETIME ProcessStartTime; }
        [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
        public struct RM_PROCESS_INFO { public RM_UNIQUE_PROCESS Process; [MarshalAs(UnmanagedType.ByValTStr, SizeConst=256)] public string strAppName; [MarshalAs(UnmanagedType.ByValTStr, SizeConst=64)] public string strServiceShortName; public int ApplicationType; public uint AppStatus; public uint TSSessionId; [MarshalAs(UnmanagedType.Bool)] public bool bRestartable; }
'@
        Add-Type -MemberDefinition $signature -Name "RestartManager" -Namespace "Win32" -ErrorAction SilentlyContinue
        try { $type = [Win32.RestartManager] } catch { return $res }
    }
    $sessionHandle = 0
    $sessionKey = [Guid]::NewGuid().ToString()
    if ($type::RmStartSession([ref]$sessionHandle, 0, $sessionKey) -eq 0) {
        try {
            $files = @($path)
            if ($type::RmRegisterResources($sessionHandle, [uint32]$files.Length, $files, 0, [IntPtr]::Zero, 0, [IntPtr]::Zero) -eq 0) {
                $pnProcInfoNeeded = [uint32]0 ; $pnProcInfo = [uint32]0 ; $rebootReasons = [uint32]0
                $type::RmGetList($sessionHandle, [ref]$pnProcInfoNeeded, [ref]$pnProcInfo, $null, [ref]$rebootReasons) | Out-Null
                if ($pnProcInfoNeeded -gt 0) {
                    $processInfo = New-Object Win32.RestartManager+RM_PROCESS_INFO[] $pnProcInfoNeeded
                    $pnProcInfo = $pnProcInfoNeeded
                    if ($type::RmGetList($sessionHandle, [ref]$pnProcInfoNeeded, [ref]$pnProcInfo, $processInfo, [ref]$rebootReasons) -eq 0) {
                        foreach ($info in $processInfo) { $res.Add("$($info.strAppName) (PID: $($info.Process.dwProcessId))") }
                    }
                }
            }
        } finally { $type::RmEndSession($sessionHandle) | Out-Null }
    }
    return $res
}

# --- FORM SETUP ---
$Form = New-Object System.Windows.Forms.Form
$Form.Text = "Tiny64 Flasher"
$Form.Size = New-Object System.Drawing.Size(420, 320)
$Form.BackColor = $BgColor
$Form.ForeColor = $TextColor
$Form.StartPosition = "CenterScreen"
$Form.FormBorderStyle = "FixedDialog"
$Form.MaximizeBox = $false

$Header = New-Object System.Windows.Forms.Panel
$Header.Size = New-Object System.Drawing.Size(420, 50)
$Header.BackColor = $HeaderColor
$Form.Controls.Add($Header)

$Title = New-Object System.Windows.Forms.Label
$Title.Text = "TINY64 USB DEPLOYER"
$Title.Font = New-Object System.Drawing.Font("Segoe UI Semibold", 12)
$Title.Location = New-Object System.Drawing.Point(20, 12)
$Title.AutoSize = $true
$Header.Controls.Add($Title)

$LabelDrive = New-Object System.Windows.Forms.Label
$LabelDrive.Text = "TARGET DRIVE"
$LabelDrive.Font = New-Object System.Drawing.Font("Segoe UI Bold", 8)
$LabelDrive.ForeColor = $MutedText
$LabelDrive.Location = New-Object System.Drawing.Point(30, 75)
$LabelDrive.AutoSize = $true
$Form.Controls.Add($LabelDrive)

$DriveCombo = New-Object System.Windows.Forms.ComboBox
$DriveCombo.Location = New-Object System.Drawing.Point(30, 95)
$DriveCombo.Size = New-Object System.Drawing.Size(350, 25)
$DriveCombo.BackColor = $HeaderColor
$DriveCombo.ForeColor = $TextColor
$DriveCombo.FlatStyle = "Flat"
$DriveCombo.DropDownStyle = "DropDownList"

$Drives = Get-WmiObject Win32_LogicalDisk | Where-Object { $_.DriveType -eq 2 }
foreach ($Drive in $Drives) {
    $DriveCombo.Items.Add("$($Drive.DeviceID) ($($Drive.VolumeName))") | Out-Null
}
$Form.Controls.Add($DriveCombo)

$StatusLabel = New-Object System.Windows.Forms.Label
$StatusLabel.Text = "Select a drive to begin"
$StatusLabel.Font = New-Object System.Drawing.Font("Segoe UI", 9)
$StatusLabel.ForeColor = $MutedText
$StatusLabel.Location = New-Object System.Drawing.Point(30, 140)
$StatusLabel.Size = New-Object System.Drawing.Size(350, 40)
$StatusLabel.TextAlign = "MiddleCenter"
$StatusLabel.BackColor = [System.Drawing.Color]::FromArgb(25, 25, 30)
$Form.Controls.Add($StatusLabel)

$DeployButton = New-Object System.Windows.Forms.Button
$DeployButton.Text = "ERASE & FLASH"
$DeployButton.Location = New-Object System.Drawing.Point(30, 200)
$DeployButton.Size = New-Object System.Drawing.Size(350, 50)
$DeployButton.BackColor = $ButtonColor
$DeployButton.FlatStyle = "Flat"
$DeployButton.FlatAppearance.BorderSize = 0
$DeployButton.Cursor = "Hand"
$DeployButton.Font = New-Object System.Drawing.Font("Segoe UI Bold", 10)
$DeployButton.Add_Click({
    if ($DriveCombo.SelectedItem -eq $null) {
        [System.Windows.Forms.MessageBox]::Show("Please select a target USB drive.", "Warning")
        return
    }

    $DriveID = $DriveCombo.SelectedItem.Substring(0, 2)
    $Confirm = [System.Windows.Forms.MessageBox]::Show("🚨 WARNING 🚨`n`nAll data on $DriveID will be PERMANENTLY DELETED.`n`nDo you want to continue?", "Confirm Flash", [System.Windows.Forms.MessageBoxButtons]::YesNo, [System.Windows.Forms.MessageBoxIcon]::Warning)
    if ($Confirm -eq "No") { return }

    $Dest = "$DriveID\"
    $ROOT = $PSScriptRoot
    $BOOT_EFI = Join-Path $ROOT "build\boot\BOOTX64.EFI"
    $KERNEL_ELF = Join-Path $ROOT "build\kernel\kernel.elf"

    $DeployButton.Enabled = $false
    $StatusLabel.ForeColor = $TextColor
    try {
        if (-Not (Test-Path $BOOT_EFI)) { throw "BOOTX64.EFI not found! Run make first." }

        # 1. Clear Drive
        $StatusLabel.Text = "Erasing existing data..."
        [System.Windows.Forms.Application]::DoEvents()
        foreach ($Item in Get-ChildItem -Path $Dest -Force) {
            if ($Item.Name -eq "System Volume Information" -or $Item.Name -eq '$RECYCLE.BIN') { continue }
            try {
                Remove-Item $Item.FullName -Recurse -Force -ErrorAction Stop
            } catch {
                throw "Access Denied: $($Item.Name). It might be in use."
            }
        }

        # 2. Create Structure
        $StatusLabel.Text = "Building directory structure..."
        [System.Windows.Forms.Application]::DoEvents()
        $EfiPath = Join-Path $Dest "EFI\BOOT"
        New-Item -Path $EfiPath -ItemType Directory -Force | Out-Null

        # 3. Copy BOOTX64.EFI (Robocopy for robustness)
        $StatusLabel.Text = "Flashing Bootloader..."
        [System.Windows.Forms.Application]::DoEvents()
        robocopy (Split-Path $BOOT_EFI) $EfiPath (Split-Path $BOOT_EFI -Leaf) /R:0 /W:0 /NJH /NJS /NDL /NC /NS /NP | Out-Null

        # 4. Copy kernel.elf
        if (Test-Path $KERNEL_ELF) {
            $StatusLabel.Text = "Flashing Kernel..."
            [System.Windows.Forms.Application]::DoEvents()
            robocopy (Split-Path $KERNEL_ELF) $Dest (Split-Path $KERNEL_ELF -Leaf) /R:0 /W:0 /J /IS /NJH /NJS /NDL /NC /NS /NP | Out-Null
        }

        # 5. Finalize
        $StatusLabel.Text = "Syncing file system..."
        $shell = New-Object -ComObject Shell.Application
        $shell.Namespace($Dest).Self.InvokeVerb("Standard.Refresh")
        [System.Windows.Forms.Application]::DoEvents()

        $StatusLabel.Text = "DEPLOYMENT SUCCESSFUL!"
        $StatusLabel.ForeColor = [System.Drawing.Color]::PaleGreen
        [System.Windows.Forms.MessageBox]::Show("Tiny64 has been successfully flashed to $DriveID !", "Success")
        $Form.Close()
    }
    catch {
        $StatusLabel.Text = "DEPLOYMENT FAILED"
        $StatusLabel.ForeColor = $RedColor
        
        $Lockers = @()
        if (Test-Path (Join-Path $Dest "kernel.elf")) {
            $Lockers = Get-LockingProcess -path (Join-Path $Dest "kernel.elf")
        }

        $ErrMsg = $_.Exception.Message
        if ($Lockers.Count -gt 0) {
            $ProcessList = $Lockers -join ", "
            $ErrMsg = "FILE LOCKED BY: $ProcessList`n`nPlease close these and try again."
        }

        [System.Windows.Forms.MessageBox]::Show("Error: $ErrMsg", "Flash Failed", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
    }
    $DeployButton.Enabled = $true
})
$Form.Controls.Add($DeployButton)

$Form.ShowDialog() | Out-Null
