$fonts = @{
    "fonts/Inter/Static/Inter-Regular.ttf" = "font_ttf_data"
    "fonts/Inter/Static/Inter-Bold.ttf" = "font_bold_data"
    "fonts/Inter/Static/Inter-Italic.ttf" = "font_italic_data"
    "fonts/Inter/Static/Inter-BoldItalic.ttf" = "font_bold_italic_data"
    "fonts/Roboto/Static/Roboto-Regular.ttf" = "font_roboto_regular_data"
    "fonts/Roboto/Static/Roboto-Bold.ttf" = "font_roboto_bold_data"
    "fonts/Roboto/Static/Roboto-Italic.ttf" = "font_roboto_italic_data"
    "fonts/Roboto/Static/Roboto-BoldItalic.ttf" = "font_roboto_bold_italic_data"
}

$outputFile = "kernel/c/graphics/font_data.h"
$header = @"
#ifndef FONT_DATA_H
#define FONT_DATA_H

"@

Set-Content -Path $outputFile -Value $header

foreach ($fontPath in $fonts.Keys) {
    $arrayName = $fonts[$fontPath]
    $filePath = "assets/$fontPath"
    
    if (Test-Path $filePath) {
        Write-Host "Converting $fontPath..."
        $bytes = [System.IO.File]::ReadAllBytes($filePath)
        
        Add-Content -Path $outputFile -Value "static const unsigned char $($arrayName)[] = {"
        
        $hexLines = @()
        for ($i = 0; $i -lt $bytes.Length; $i += 16) {
            $chunk = $bytes[$i..($i + 15)] | Where-Object { $_ -ne $null }
            $hexStrings = $chunk | ForEach-Object { "0x{0:X2}" -f $_ }
            $hexLines += ($hexStrings -join ", ")
        }
        
        Add-Content -Path $outputFile -Value ($hexLines -join ",`n")
        Add-Content -Path $outputFile -Value "};`n"
    } else {
        Write-Warning "Could not find $filePath"
    }
}

Add-Content -Path $outputFile -Value "#endif"
