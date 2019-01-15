:: set environment
set GYP_MSVS_VERSION=2017
set DEPOT_TOOLS_WIN_TOOLCHAIN=0

:: gn gen out
call D:\code\depot_tools D:\code\depot_tools\gn.py gen .\out --ide=vs

:: gn args out
call D:\code\depot_tools\python D:\code\depot_tools\gn.py args .\out

:: compiling
call D:\code\depot_tools\ninja -C .\out example

pause