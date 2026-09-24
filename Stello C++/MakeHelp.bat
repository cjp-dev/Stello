@echo off
REM -- First make map file from Microsoft Visual C++ generated resource.h
echo // MAKEHELP.BAT generated Help Map file.  Used by STELLO.HPJ. >"hlp\Stello.hm"
echo. >>"hlp\Stello.hm"
echo // Commands (ID_* and IDM_*) >>"hlp\Stello.hm"
makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\Stello.hm"
echo. >>"hlp\Stello.hm"
echo // Prompts (IDP_*) >>"hlp\Stello.hm"
makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\Stello.hm"
echo. >>"hlp\Stello.hm"
echo // Resources (IDR_*) >>"hlp\Stello.hm"
makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\Stello.hm"
echo. >>"hlp\Stello.hm"
echo // Dialogs (IDD_*) >>"hlp\Stello.hm"
makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\Stello.hm"
echo. >>"hlp\Stello.hm"
echo // Frame Controls (IDW_*) >>"hlp\Stello.hm"
makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\Stello.hm"
REM -- Make help for Project STELLO


echo Building Win32 Help files
start /wait hcw /C /E /M "hlp\Stello.hpj"
if errorlevel 1 goto :Error
if not exist "hlp\Stello.hlp" goto :Error
if not exist "hlp\Stello.cnt" goto :Error
echo.
if exist Debug\nul copy "hlp\Stello.hlp" Debug
if exist Debug\nul copy "hlp\Stello.cnt" Debug
if exist Release\nul copy "hlp\Stello.hlp" Release
if exist Release\nul copy "hlp\Stello.cnt" Release
echo.
goto :done

:Error
echo hlp\Stello.hpj(1) : error: Problem encountered creating help file

:done
echo.
