
Add-Type -AssemblyName System.Drawing

$sourcePath = "assets/images/Logos/Tiny64White.png"
$destPath = "kernel/c/graphics/logo_data.h"
$targetWidth = 256
$targetHeight = 256

if (-not (Test-Path $sourcePath)) {
    Write-Error "Source file not found: $sourcePath"
    exit 1
}

$img = [System.Drawing.Bitmap]::FromFile($sourcePath)
$resized = new-object System.Drawing.Bitmap $targetWidth, $targetHeight
$graph = [System.Drawing.Graphics]::FromImage($resized)
$graph.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$graph.DrawImage($img, 0, 0, $targetWidth, $targetHeight)

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("#ifndef LOGO_DATA_H")
[void]$sb.AppendLine("#define LOGO_DATA_H")
[void]$sb.AppendLine("#include <stdint.h>")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("static const uint32_t logo_width = $targetWidth;")
[void]$sb.AppendLine("static const uint32_t logo_height = $targetHeight;")
[void]$sb.AppendLine("static const uint32_t logo_data[] = {")

for ($y = 0; $y -lt $targetHeight; $y++) {
    for ($x = 0; $x -lt $targetWidth; $x++) {
        $color = $resized.GetPixel($x, $y)
        # Format: 0x00RRGGBB
        $val = ($color.R -shl 16) -bor ($color.G -shl 8) -bor $color.B
        [void]$sb.Append("0x{0:X6}, " -f $val)
    }
    [void]$sb.AppendLine("")
}

[void]$sb.AppendLine("};")
[void]$sb.AppendLine("#endif")

Set-Content -Path $destPath -Value $sb.ToString()
Write-Host "Converted $sourcePath to $destPath"

$img.Dispose()
$resized.Dispose()
$graph.Dispose()
