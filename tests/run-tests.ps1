$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'build\http-server.exe'
if (-not (Test-Path $exe)) {
    throw "binario nao encontrado: $exe (compile antes com: mingw32-make)"
}

$port = 8090
$base = "http://127.0.0.1:$port"
$logOut = Join-Path $env:TEMP "hs-c-out-$PID.log"
$logErr = Join-Path $env:TEMP "hs-c-err-$PID.log"

$proc = Start-Process -FilePath $exe -ArgumentList "$port" `
    -WorkingDirectory $root -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput $logOut -RedirectStandardError $logErr

function Wait-ServerUp {
    $deadline = (Get-Date).AddSeconds(10)
    while ((Get-Date) -lt $deadline) {
        try {
            $c = New-Object Net.Sockets.TcpClient
            $c.Connect('127.0.0.1', $port)
            $c.Close()
            return $true
        } catch {
            Start-Sleep -Milliseconds 150
        }
    }
    return $false
}

function Req([string[]]$CurlArgs) {
    $bodyFile = New-TemporaryFile
    $w = '%{http_code}|%{content_type}|%{size_download}'
    $out = & curl.exe -s -o $bodyFile.FullName -w $w @CurlArgs
    $meta = [string]($out | Select-Object -Last 1)
    $parts = $meta.Split('|')
    if ($parts.Count -lt 3) { $parts = @('0', '', '0') }
    $result = [pscustomobject]@{
        Status      = 0
        ContentType = ''
        Size        = [long]0
        Body        = ''
    }
    [void][int]::TryParse($parts[0], [ref]$result.Status)
    $result.ContentType = $parts[1]
    [void][long]::TryParse($parts[2], [ref]$result.Size)
    $result.Body = (Get-Content $bodyFile.FullName -Raw -ErrorAction SilentlyContinue)
    Remove-Item $bodyFile.FullName -Force -ErrorAction SilentlyContinue
    return $result
}

$script:pass = 0
$script:fail = 0

function Assert([bool]$Cond, [string]$Name, [string]$Detail) {
    if ($Cond) {
        $script:pass++
        Write-Host "  PASS  $Name"
    } else {
        $script:fail++
        Write-Host "  FAIL  $Name  [$Detail]"
    }
}

try {
    if (-not (Wait-ServerUp)) {
        Write-Host "--- stdout do servidor ---"
        Get-Content $logOut -ErrorAction SilentlyContinue
        throw "servidor nao subiu na porta $port"
    }

    Write-Host "Executando testes contra $base ...`n"

    $r = Req @("$base/")
    Assert ($r.Status -eq 200) 'GET / retorna 200' "status=$($r.Status)"
    Assert ($r.ContentType -like 'text/html*') 'GET / content-type text/html' $r.ContentType
    Assert ($r.Body -match '<!DOCTYPE html>') 'GET / corpo contem DOCTYPE' ''

    $r = Req @("$base/style.css")
    Assert ($r.Status -eq 200 -and $r.ContentType -like 'text/css*') 'GET /style.css' "status=$($r.Status) ct=$($r.ContentType)"

    $r = Req @("$base/app.js")
    Assert ($r.Status -eq 200 -and $r.ContentType -like '*javascript*') 'GET /app.js' "status=$($r.Status) ct=$($r.ContentType)"

    $r = Req @("$base/arquivo-inexistente.bin")
    Assert ($r.Status -eq 404) 'GET arquivo inexistente retorna 404' "status=$($r.Status)"

    $r = Req @('-X', 'POST', "$base/")
    Assert ($r.Status -eq 405) 'POST / retorna 405' "status=$($r.Status)"

    $r = Req @('-X', 'PUT', "$base/health")
    Assert ($r.Status -eq 405) 'PUT /health retorna 405' "status=$($r.Status)"

    $r = Req @('-I', "$base/")
    Assert ($r.Status -eq 200 -and $r.Size -eq 0) 'HEAD / retorna 200 sem corpo' "status=$($r.Status) size=$($r.Size)"

    $r = Req @("$base/health")
    Assert ($r.Status -eq 200 -and $r.Body.Trim() -eq 'ok') 'GET /health corpo ok' "status=$($r.Status) body=$($r.Body)"

    $r = Req @('--path-as-is', "$base/../Makefile")
    Assert ($r.Status -eq 404) 'path traversal ../ bloqueado' "status=$($r.Status)"

    $r = Req @('--path-as-is', "$base/%2e%2e/Makefile")
    Assert ($r.Status -eq 404) 'path traversal encodado bloqueado' "status=$($r.Status)"

    $codes = (& curl.exe -s -o NUL -w '%{http_code} ' "$base/" "$base/style.css") -join ''
    Assert ($codes -match '200\s+200') 'keep-alive reusa a conexao' "codes=$codes"

    $r = Req @('--http1.0', "$base/")
    Assert ($r.Status -eq 200) 'HTTP/1.0 aceito' "status=$($r.Status)"

    $r = Req @('--http1.1', '-H', 'Host: x', "$base/") # smoke extra de pipeline
    Assert ($r.Status -eq 200) 'segunda request na mesma conexao' "status=$($r.Status)"
}
finally {
    if ($proc -and -not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
}

Write-Host ""
Write-Host "Resultado: $($script:pass) passaram, $($script:fail) falharam"

$errContent = Get-Content $logErr -ErrorAction SilentlyContinue
if ($errContent) {
    Write-Host "--- stderr do servidor ---"
    $errContent | ForEach-Object { Write-Host $_ }
}

exit ([int]($script:fail -gt 0))
