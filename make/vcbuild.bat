@ECHO OFF

REM Allow to explicitly specify the desired Visual Studio version

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Enterprise\Common7\Tools\VsDevCmd.bat" (
	CALL "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat"
	IF NOT ERRORLEVEL 1 GOTO BUILD
)

IF /I "%1" == "vc17" GOTO TRY_VS17
IF /I "%1" == "vc15" GOTO TRY_VS15
IF /I "%1" == "vc12" GOTO TRY_VS12
IF /I "%1" == "vc10" GOTO TRY_VS10
IF /I "%1" == "vc9" GOTO TRY_VS9

REM vs15 is VS 2017
:TRY_VS17
echo Trying VS17...
CALL "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat" 2>NUL
IF NOT ERRORLEVEL 1 GOTO BUILD

REM vs14 is VS 2015
:TRY_VS15
echo Trying VS15...
CALL "%VS140COMNTOOLS%\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 GOTO BUILD

REM vs12 is VS 2012
:TRY_VS12
echo Trying VS12...
CALL "%VS120COMNTOOLS%\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 GOTO BUILD

REM vs10 is VS 2010
:TRY_VS10
echo Trying VS10...
CALL "%VS100COMNTOOLS%\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 GOTO BUILD

REM vs9 is VS 2008
:TRY_VS9
echo Trying VS9...
CALL "%VS90COMNTOOLS%\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 GOTO BUILD


ECHO Visual Studio 2012, 2010, or 2008 doesn't seem to be installed
EXIT /B 1

:BUILD
nmake -f makefile.vc all

