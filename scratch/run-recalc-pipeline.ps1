# Stello Opening Book Recalculation & Match Pipeline
$ErrorActionPreference = "Stop"

$tool = ".\Stello.Net\tools\Stello.BookTool\bin\Release\net10.0\Stello.BookTool.exe"
$bookTxt = "Stello.Net\Book\opening-book.txt"
$bookBin = "Stello.Net\Book\opening-book.bin"
$bookV1Bin = "Stello.Net\Book\opening-book-v1.bin"

Write-Host "=== Phase 1: Recalculating Book Leaves (60s limit, 7 workers) ==="
& $tool recalc $bookTxt --time-s 60 --workers 7 --save-every 10

if ($LASTEXITCODE -ne 0) {
    Write-Host "Recalc stopped or interrupted (code $LASTEXITCODE). Checkpoint saved to $bookTxt."
    exit $LASTEXITCODE
}

Write-Host "=== Phase 2: Building New Binary Book ==="
& $tool build $bookTxt $bookBin

Write-Host "=== Phase 3: Book Match (Old v1 Book vs New v2 Book at Depth 10) ==="
& $tool match $bookV1Bin $bookBin --starts-ply 8 --depth 10 --workers 7

Write-Host "=== Pipeline Completed Successfully ==="

