param([string]$dllBase)
$dll = "D:\Downloads\aurora_restructured\$dllBase\$dllBase.dll"
# Export names use underscores instead of hyphens (C identifier convention)
# Strip _pypi or _npm suffix to get the C identifier prefix
$exportPrefix = $dllBase -replace '_(pypi|npm)$','' -replace '-','_'
Write-Host "Loading $dll ..."
try {
    $lib = [System.Runtime.InteropServices.NativeLibrary]::Load($dll)
    Write-Host "  LoadLibrary OK"
    $pReq = [System.Runtime.InteropServices.NativeLibrary]::GetExport($lib, "${exportPrefix}_require")
    Write-Host "  GetExport ${exportPrefix}_require OK"
    $pFree = [System.Runtime.InteropServices.NativeLibrary]::GetExport($lib, "${exportPrefix}_free")
    Write-Host "  GetExport free OK"
    
    $reqAddr = [System.Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer($pReq, [Type]([IntPtr]))
    
    Write-Host "  Calling ${exportPrefix}_require ..."
    $mod = [System.Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer($pReq, [Func[IntPtr]]).Invoke()
    Write-Host "  Result: $mod"
    
    if ($mod -ne [IntPtr]::Zero) {
        Write-Host "  SUCCESS: bridge loaded OK" -ForegroundColor Green
    } else {
        Write-Host "  FAIL: bridge returned NULL" -ForegroundColor Red
    }
    
    [System.Runtime.InteropServices.NativeLibrary]::Free($lib)
} catch {
    Write-Host "  ERROR: $_" -ForegroundColor Red
}
