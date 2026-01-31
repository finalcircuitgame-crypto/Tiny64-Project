
Add-Type -AssemblyName System.Drawing

$width = 32
$height = 32
$bmp = New-Object System.Drawing.Bitmap $width, $height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::Transparent)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

$white = [System.Drawing.Brushes]::White
$pen = New-Object System.Drawing.Pen([System.Drawing.Color]::White, 2)

# Draw Ethernet Icon (Simple)
# Three blocks
$g.FillRectangle($white, 4, 4, 6, 6)   # Top Left
$g.FillRectangle($white, 22, 4, 6, 6)  # Top Right
$g.FillRectangle($white, 13, 20, 6, 6) # Bottom Center

# Lines
$g.DrawLine($pen, 7, 10, 7, 14)        # Down from Top Left
$g.DrawLine($pen, 25, 10, 25, 14)      # Down from Top Right
$g.DrawLine($pen, 7, 14, 25, 14)       # Connect Horizontally
$g.DrawLine($pen, 16, 14, 16, 20)      # Down to Bottom Center

$path = "assets/images/Icons/EthernetWhite.png"
$bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)

$g.Dispose()
$bmp.Dispose()
$pen.Dispose()

Write-Host "Generated $path"
