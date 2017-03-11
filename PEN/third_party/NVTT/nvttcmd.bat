@echo off
@set PATH=%~dp0bin;%PATH%

if "%1" == "" goto usage
if /i %1 == nvcompress    goto nvcompress
if /i %1 == nvdecompress  goto nvdecompress
if /i %1 == nvddsinfo goto nvddsinfo
if /i %1 == nvassemble goto nvassemble
if /i %1 == nvzoom goto nvzoom
if /i %1 == nvimgdiff goto nvimgdiff
if /i %1 == nvttcmd    goto nvttcmd
goto usage

:usage
echo Usage information:
echo    %0 [option]
echo where [option] is: nvcompress ^| nvdecompress ^| nvddsinfo ^| nvassemble ^| nvzoom ^| nvimgdiff
goto :eof

:nvttcmd
echo Usage information:
echo    [option]
echo where [option] is: nvcompress ^| nvdecompress ^| nvddsinfo ^| nvassemble ^| nvzoom ^| nvimgdiff 
cmd.exe
goto :eof

:nvcompress
call %*
goto :eof

:nvdecompress
call %*
goto :eof

:nvddsinfo
call %*
goto :eof

:nvassemble
call %*
goto :eof

:nvzoom
call %*
goto :eof

:nvimgdiff
call %*
goto :eof
