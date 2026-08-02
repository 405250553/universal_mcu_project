<#
.SYNOPSIS
    make 的 PowerShell 包裝腳本。

原生 PowerShell 的系統 PATH 沒有 Git 的 usr\bin（rm、mkdir 等 coreutils 所在位置），
導致 make.exe 直接用 CreateProcess 呼叫 rm/mkdir 時找不到執行檔。
這裡在呼叫 make 前，臨時（僅限本次程序）把該路徑補進 PATH。

.EXAMPLE
    .\build.ps1 list
    .\build.ps1 TARGET=stm32f746_avi_player
    .\build.ps1 TARGET=stm32f746_avi_player DEBUG=1
    .\build.ps1 TARGET=stm32f746_avi_player clean
    .\build.ps1 flash
#>

param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$MakeArgs
)

$gitUsrBin = "C:\Program Files\Git\usr\bin"
if ((Test-Path $gitUsrBin) -and ($env:Path -notlike "*$gitUsrBin*")) {
    $env:Path = "$gitUsrBin;$env:Path"
}

& make @MakeArgs
exit $LASTEXITCODE
