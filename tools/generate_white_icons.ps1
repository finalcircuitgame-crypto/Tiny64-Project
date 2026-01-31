
Add-Type -AssemblyName System.Drawing

function Draw-Icon($name, $drawAction) {
    $width = 32
    $height = 32
    $bmp = New-Object System.Drawing.Bitmap $width, $height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $white = [System.Drawing.Brushes]::White
    $whitePen = New-Object System.Drawing.Pen([System.Drawing.Color]::White, 2)
    $thinPen = New-Object System.Drawing.Pen([System.Drawing.Color]::White, 1)

    & $drawAction $g $white $whitePen $thinPen

    $path = "assets/images/Icons/$name.png"
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
    $whitePen.Dispose()
    $thinPen.Dispose()
    Write-Host "Generated $path"
}

# --- Task Manager ---
Draw-Icon "TaskManager" { param($g, $b, $p, $tp)
    $g.DrawRectangle($p, 4, 6, 24, 20)
    $g.FillRectangle($b, 7, 14, 4, 9)
    $g.FillRectangle($b, 14, 10, 4, 13)
    $g.FillRectangle($b, 21, 17, 4, 6)
}

# --- Settings ---
Draw-Icon "Settings" { param($g, $b, $p, $tp)
    $g.DrawEllipse($p, 10, 10, 12, 12)
    # Spokes
    $g.DrawLine($p, 16, 4, 16, 8)
    $g.DrawLine($p, 16, 24, 16, 28)
    $g.DrawLine($p, 4, 16, 8, 16)
    $g.DrawLine($p, 24, 16, 28, 16)
    # Diagonal Spokes
    $g.DrawLine($p, 7, 7, 10, 10)
    $g.DrawLine($p, 22, 22, 25, 25)
    $g.DrawLine($p, 7, 25, 10, 22)
    $g.DrawLine($p, 22, 10, 25, 7)
}

# --- Notepad ---
Draw-Icon "Notepad" { param($g, $b, $p, $tp)
    $points = @(
        (New-Object System.Drawing.Point 6, 4),
        (New-Object System.Drawing.Point 20, 4),
        (New-Object System.Drawing.Point 26, 10),
        (New-Object System.Drawing.Point 26, 28),
        (New-Object System.Drawing.Point 6, 28),
        (New-Object System.Drawing.Point 6, 4)
    )
    $g.DrawPolygon($p, $points)
    $g.DrawLine($p, 20, 4, 20, 10)
    $g.DrawLine($p, 20, 10, 26, 10)
    # Text lines
    $g.DrawLine($p, 10, 14, 22, 14)
    $g.DrawLine($p, 10, 19, 22, 19)
    $g.DrawLine($p, 10, 24, 18, 24)
}

# --- Terminal ---
Draw-Icon "Terminal" { param($g, $b, $p, $tp)
    $g.DrawRectangle($p, 2, 5, 28, 22)
    # Prompt >
    $g.DrawLine($p, 8, 12, 12, 16)
    $g.DrawLine($p, 12, 16, 8, 20)
    # Cursor _
    $g.DrawLine($p, 14, 20, 20, 20)
}

# --- File Explorer ---
Draw-Icon "FileExplorer" { param($g, $b, $p, $tp)
    $points = @(
        (New-Object System.Drawing.Point 4, 6),
        (New-Object System.Drawing.Point 14, 6),
        (New-Object System.Drawing.Point 16, 9),
        (New-Object System.Drawing.Point 28, 9),
        (New-Object System.Drawing.Point 28, 26),
        (New-Object System.Drawing.Point 4, 26),
        (New-Object System.Drawing.Point 4, 6)
    )
    $g.DrawPolygon($p, $points)
    $g.DrawLine($p, 4, 11, 28, 11)
}

# --- Device Manager ---
Draw-Icon "DeviceManager" { param($g, $b, $p, $tp)
    $g.DrawRectangle($p, 6, 6, 20, 14) # Monitor
    $g.FillRectangle($b, 14, 20, 4, 4) # Stand neck
    $g.FillRectangle($b, 10, 24, 12, 2) # Base
    
    # Chip inside
    $g.DrawRectangle($tp, 12, 10, 8, 6)
    $g.FillRectangle($b, 14, 12, 4, 2)
}
