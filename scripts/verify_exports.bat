@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
echo === requests_pypi.dll exports ===
dumpbin /exports "D:\Downloads\aurora_restructured\build\Debug\requests_pypi\requests_pypi.dll" | findstr requests_
echo === markdown_pypi.dll exports ===
dumpbin /exports "D:\Downloads\aurora_restructured\build\Debug\markdown_pypi\markdown_pypi.dll" | findstr markdown_
