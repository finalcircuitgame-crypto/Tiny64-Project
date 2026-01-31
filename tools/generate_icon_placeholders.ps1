Add-Type -AssemblyName System.Drawing

function Generate-Icon($path, $bgColor, $text, $textColor) {
    $bmp = New-Object System.Drawing.Bitmap 32,32
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear($bgColor)
    
    # Simpler font creation
    $font = New-Object System.Drawing.Font('Arial', 18)
    $brush = New-Object System.Drawing.SolidBrush($textColor)
    
    # Simple centering logic
    $size = $g.MeasureString($text, $font)
    $x = (32 - $size.Width) / 2
    $y = (32 - $size.Height) / 2
    
    $g.DrawString($text, $font, $brush, $x, $y)
    
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    
    $brush.Dispose()
    $font.Dispose()
    $g.Dispose()
    $bmp.Dispose()
    Write-Host "Generated $path"
}

$iconsDir = "assets/images/Icons"
if (-not (Test-Path $iconsDir)) { New-Item -ItemType Directory $iconsDir }

Generate-Icon "$iconsDir/Terminal.png" ([System.Drawing.Color]::FromArgb(255, 39, 60, 117)) ">" ([System.Drawing.Color]::White)
Generate-Icon "$iconsDir/Notepad.png" ([System.Drawing.Color]::FromArgb(255, 241, 196, 15)) "N" ([System.Drawing.Color]::Black)
Generate-Icon "$iconsDir/TaskManager.png" ([System.Drawing.Color]::FromArgb(255, 192, 57, 43)) "M" ([System.Drawing.Color]::White)
Generate-Icon "$iconsDir/DeviceManager.png" ([System.Drawing.Color]::FromArgb(255, 22, 160, 133)) "D" ([System.Drawing.Color]::White)
Generate-Icon "$iconsDir/Settings.png" ([System.Drawing.Color]::FromArgb(255, 127, 140, 141)) "S" ([System.Drawing.Color]::White)