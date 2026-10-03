param()
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$brand = Join-Path $PSScriptRoot '..\resources\branding'
$source = [Drawing.Bitmap]::FromFile((Join-Path $brand 'logo-module.png'))
try {
    # Preserve the selected artwork, trimming only transparent margins.
    $left=$source.Width; $top=$source.Height; $right=0; $bottom=0
    for ($y=0; $y -lt $source.Height; $y++) {
        for ($x=0; $x -lt $source.Width; $x++) {
            if ($source.GetPixel($x,$y).A -gt 32) {
                $left=[Math]::Min($left,$x); $top=[Math]::Min($top,$y)
                $right=[Math]::Max($right,$x); $bottom=[Math]::Max($bottom,$y)
            }
        }
    }
    if ($right -le $left -or $bottom -le $top) { throw 'Empty logo.' }
    $crop=[Drawing.Rectangle]::new($left,$top,$right-$left+1,$bottom-$top+1)
    $frames=@()
    foreach ($size in @(16,20,24,32,40,48,64,128,256)) {
        $bitmap=[Drawing.Bitmap]::new($size,$size)
        $graphics=[Drawing.Graphics]::FromImage($bitmap)
        $stream=[IO.MemoryStream]::new()
        try {
            $graphics.Clear([Drawing.Color]::FromArgb(246,248,251))
            $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $scale=($size*0.80)/[Math]::Max($crop.Width,$crop.Height)
            $width=[int]($crop.Width*$scale); $height=[int]($crop.Height*$scale)
            $dest=[Drawing.Rectangle]::new([int](($size-$width)/2),[int](($size-$height)/2),$width,$height)
            $graphics.DrawImage($source,$dest,$crop,[Drawing.GraphicsUnit]::Pixel)
            $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
            $frames+=,@{Size=$size;Data=$stream.ToArray()}
            if ($size -eq 256) { $bitmap.Save((Join-Path $brand 'app-icon.png'),[Drawing.Imaging.ImageFormat]::Png) }
        } finally { $stream.Dispose(); $graphics.Dispose(); $bitmap.Dispose() }
    }
    $output=[IO.File]::Create((Join-Path $brand 'app.ico'))
    $writer=[IO.BinaryWriter]::new($output)
    try {
        $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$frames.Count)
        $offset=6+16*$frames.Count
        foreach ($frame in $frames) {
            $dimension=if ($frame.Size -eq 256) { 0 } else { $frame.Size }
            $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
            $writer.Write([byte]0); $writer.Write([byte]0)
            $writer.Write([uint16]1); $writer.Write([uint16]32)
            $writer.Write([uint32]$frame.Data.Length); $writer.Write([uint32]$offset)
            $offset+=$frame.Data.Length
        }
        foreach ($frame in $frames) { $writer.Write([byte[]]$frame.Data) }
    } finally { $writer.Dispose(); $output.Dispose() }
} finally { $source.Dispose() }
Write-Output 'Brand icons generated (16-256 px).'
