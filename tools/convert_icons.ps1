
Add-Type -AssemblyName System.Drawing

$icons = @{
    "terminal" = "assets/images/Icons/Terminal.png"
    "explorer" = "assets/images/Icons/FileExplorer.png"
    "notepad"  = "assets/images/Icons/Notepad.png"
    "taskmgr"  = "assets/images/Icons/TaskManager.png"
    "devmgr"   = "assets/images/Icons/DeviceManager.png"
    "settings" = "assets/images/Icons/Settings.png"
    "ethernet" = "assets/images/Icons/EthernetWhite.png"
}

$destPath = "kernel/cpp/ui/app_icons.h"
$targetWidth = 32
$targetHeight = 32

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("#ifndef APP_ICONS_H")
[void]$sb.AppendLine("#define APP_ICONS_H")
[void]$sb.AppendLine("#include <stdint.h>")
[void]$sb.AppendLine("")

foreach ($name in $icons.Keys) {
    $path = $icons[$name]
    if (Test-Path $path) {
        Write-Host "Converting $name ($path)..."
        $img = [System.Drawing.Bitmap]::FromFile($path)
        $resized = new-object System.Drawing.Bitmap $targetWidth, $targetHeight
        $graph = [System.Drawing.Graphics]::FromImage($resized)
        $graph.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graph.DrawImage($img, 0, 0, $targetWidth, $targetHeight)

        [void]$sb.AppendLine("static const uint32_t icon_$($name)_data[] = {")
        for ($y = 0; $y -lt $targetHeight; $y++) {
            for ($x = 0; $x -lt $targetWidth; $x++) {
                $color = $resized.GetPixel($x, $y)
                # Format: 0xAARRGGBB
                $val = ([uint32]$color.A -shl 24) -bor ([uint32]$color.R -shl 16) -bor ([uint32]$color.G -shl 8) -bor [uint32]$color.B
                [void]$sb.Append("0x{0:X8}, " -f $val)
            }
            [void]$sb.AppendLine("")
        }
        [void]$sb.AppendLine("};")
        [void]$sb.AppendLine("")

        $img.Dispose()
        $resized.Dispose()
        $graph.Dispose()
    } else {
        Write-Warning "Icon not found: $path"
        # Generate a placeholder if missing?
        [void]$sb.AppendLine("// Icon $name not found at $path")
    }
}

[void]$sb.AppendLine("#endif")
Set-Content -Path $destPath -Value $sb.ToString()
Write-Host "Icons converted to $destPath"
