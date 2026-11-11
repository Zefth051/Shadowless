param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\src\Shadowless\Assets\Shadowless.ico'),
    [string]$Color = '#63E0BD',
    [string]$SourcePath = (Join-Path $PSScriptRoot '../src/Shadowless/Assets/ShadowlessMoon.png')
)

$ErrorActionPreference = 'Stop'
try {
    Add-Type -AssemblyName System.Drawing.Common
}
catch {
    Add-Type -AssemblyName System.Drawing
}

$sizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
$pngImages = [System.Collections.Generic.List[byte[]]]::new()
$iconColor = [System.Drawing.ColorTranslator]::FromHtml($Color)

# Map the source moon's blue shadow to the accent and retain its pale highlights.
# The generated source artwork and its alpha channel remain the single master.
$source = [System.Drawing.Image]::FromFile([IO.Path]::GetFullPath($SourcePath))
$attributes = [System.Drawing.Imaging.ImageAttributes]::new()
$matrix = [System.Drawing.Imaging.ColorMatrix]::new()
$matrix.Matrix00 = (255 - $iconColor.R) / 156.0
$matrix.Matrix01 = (255 - $iconColor.G) / 156.0
$matrix.Matrix02 = (255 - $iconColor.B) / 156.0
$matrix.Matrix11 = 0
$matrix.Matrix22 = 0
$matrix.Matrix40 = 1 - $matrix.Matrix00
$matrix.Matrix41 = 1 - $matrix.Matrix01
$matrix.Matrix42 = 1 - $matrix.Matrix02
$attributes.SetColorMatrix($matrix)
try {
    foreach ($size in $sizes) {
        $bitmap = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $stream = [System.IO.MemoryStream]::new()
        try {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.DrawImage($source, [System.Drawing.Rectangle]::new(0, 0, $size, $size),
                0, 0, $source.Width, $source.Height, [System.Drawing.GraphicsUnit]::Pixel, $attributes)
            $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
            $pngImages.Add($stream.ToArray())
        }
        finally {
            $stream.Dispose()
            $graphics.Dispose()
            $bitmap.Dispose()
        }
    }
}
finally {
    $attributes.Dispose()
    $source.Dispose()
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($resolvedOutput)) | Out-Null
$file = [System.IO.File]::Create($resolvedOutput)
$writer = [System.IO.BinaryWriter]::new($file)
try {
    $writer.Write([uint16]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]$sizes.Count)

    $offset = 6 + 16 * $sizes.Count
    for ($index = 0; $index -lt $sizes.Count; $index++) {
        $size = $sizes[$index]
        $image = $pngImages[$index]
        $iconDimension = if ($size -eq 256) { 0 } else { $size }
        $writer.Write([byte]$iconDimension)
        $writer.Write([byte]$iconDimension)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]32)
        $writer.Write([uint32]$image.Length)
        $writer.Write([uint32]$offset)
        $offset += $image.Length
    }

    foreach ($image in $pngImages) {
        $writer.Write($image)
    }
}
finally {
    $writer.Dispose()
    $file.Dispose()
}

Write-Output $resolvedOutput
