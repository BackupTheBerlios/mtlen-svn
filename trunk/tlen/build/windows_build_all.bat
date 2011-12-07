@echo off

set NLM=^


set NL=^^^%NLM%%NLM%^%NLM%%NLM%

cls



REM ====================================
REM ===== C H A N G E    I T  ! ! ======
REM ====================================
REM ========== ENV PATCHES =============
REM Microsoft Visual Studio 
set vs10msbuild=C:\Windows\Microsoft.NET\Framework\v4.0.30319\msbuild.exe
REM HaoZip command line archiver
set haoZipExecutable=C:\Program Files\HaoZip\HaoZipC.exe
REM zip files postfix
set zipFilesPostfix=2050
REM ========== ENV PATCHES END==========










set tlenProjectFolder=..
set czatyProjectFolder=..\tlen_czaty
set tmpFolder=.\windows_build_tmp
set targetFolderR=.\windows_build_release
set targetFolderD=.\windows_build_debug



REM goto package_only


REM compiling tlen - Debug Ansi
echo %NL%%NL%%NL% * tlen - Debug: cleaning VS 10 project   (Debug Ansi , Win32) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Debug Ansi" /p:Platform=Win32 %tlenProjectFolder%\tlen_10.vcxproj
echo %NL%%NL%%NL% * tlen - Debug: building VS 10 project  (Debug Ansi , Win32) %NL%%NL%
call %vs10msbuild% /p:Configuration="Debug Ansi" /p:Platform=Win32 %tlenProjectFolder%\tlen_10.vcxproj

REM compiling tlen - Debug Unicode
echo %NL%%NL%%NL% * tlen - Debug: cleaning VS 10 project   (Debug Unicode , Win32) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Debug Unicode" /p:Platform=Win32 %tlenProjectFolder%\tlen_10.vcxproj
echo %NL%%NL%%NL% * tlen - Debug: building VS 10 project  (Debug Unicode , Win32) %NL%%NL%
call %vs10msbuild% /p:Configuration="Debug Unicode" /p:Platform=Win32 %tlenProjectFolder%\tlen_10.vcxproj

REM compiling tlen - Debug x64
echo %NL%%NL%%NL% * tlen - Debug: cleaning VS 10 project   (Debug x64 , x64) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Debug x64" /p:Platform=x64 %tlenProjectFolder%\tlen_10.vcxproj
echo %NL%%NL%%NL% * tlen - Debug: building VS 10 project  (Debug x64 , x64) %NL%%NL%
call %vs10msbuild% /p:Configuration="Debug x64" /p:Platform=x64 %tlenProjectFolder%\tlen_10.vcxproj

REM compiling tlen_czaty - Debug Ansi
echo %NL%%NL%%NL% * tlen_czaty - Debug: cleaning VS 10 project   (Debug Ansi , Win32) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Debug Ansi" /p:Platform=Win32 %czatyProjectFolder%\mucc.vcxproj
echo %NL%%NL%%NL% * tlen_czaty - Debug: building VS 10 project  (Debug Ansi , Win32) %NL%%NL%
call %vs10msbuild% /p:Configuration="Debug Ansi" /p:Platform=Win32 %czatyProjectFolder%\mucc.vcxproj

REM compiling tlen_czaty - Debug Unicode
echo %NL%%NL%%NL% * tlen_czaty - Debug: cleaning VS 10 project   (Debug Unicode , Win32) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Debug Unicode" /p:Platform=Win32 %czatyProjectFolder%\mucc.vcxproj
echo %NL%%NL%%NL% * tlen_czaty - Debug: building VS 10 project  (Debug Unicode , Win32) %NL%%NL%
call %vs10msbuild% /p:Configuration="Debug Unicode" /p:Platform=Win32 %czatyProjectFolder%\mucc.vcxproj

REM compiling tlen_czaty - Debug x64
echo %NL%%NL%%NL% * tlen_czaty - Debug: cleaning VS 10 project   (Debug x64 , x64) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Debug x64" /p:Platform=x64 %czatyProjectFolder%\mucc.vcxproj
echo %NL%%NL%%NL% * tlen_czaty - Debug: building VS 10 project  (Debug x64 , x64) %NL%%NL%
call %vs10msbuild% /p:Configuration="Debug x64" /p:Platform=x64 %czatyProjectFolder%\mucc.vcxproj






REM compiling tlen - Release Ansi
echo %NL%%NL%%NL% * tlen - Release: cleaning VS 10 project   (Release Ansi , Win32) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Release Ansi" /p:Platform=Win32 %tlenProjectFolder%\tlen_10.vcxproj
echo %NL%%NL%%NL% * tlen - Release: building VS 10 project  (Release Ansi , Win32) %NL%%NL%
call %vs10msbuild% /p:Configuration="Release Ansi" /p:Platform=Win32 %tlenProjectFolder%\tlen_10.vcxproj

REM compiling tlen - Release Unicode
echo %NL%%NL%%NL% * tlen - Release: cleaning VS 10 project   (Release Unicode , Win32) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Release Unicode" /p:Platform=Win32 %tlenProjectFolder%\tlen_10.vcxproj
echo %NL%%NL%%NL% * tlen - Release: building VS 10 project  (Release Unicode , Win32) %NL%%NL%
call %vs10msbuild% /p:Configuration="Release Unicode" /p:Platform=Win32 %tlenProjectFolder%\tlen_10.vcxproj

REM compiling tlen - Release x64
echo %NL%%NL%%NL% * tlen - Release: cleaning VS 10 project   (Release x64 , x64) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Release x64" /p:Platform=x64 %tlenProjectFolder%\tlen_10.vcxproj
echo %NL%%NL%%NL% * tlen - Release: building VS 10 project  (Release x64 , x64) %NL%%NL%
call %vs10msbuild% /p:Configuration="Release x64" /p:Platform=x64 %tlenProjectFolder%\tlen_10.vcxproj

REM compiling tlen_czaty - Release Ansi
echo %NL%%NL%%NL% * tlen_czaty - Release: cleaning VS 10 project   (Release Ansi , Win32) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Release Ansi" /p:Platform=Win32 %czatyProjectFolder%\mucc.vcxproj
echo %NL%%NL%%NL% * tlen_czaty - Release: building VS 10 project  (Release Ansi , Win32) %NL%%NL%
call %vs10msbuild% /p:Configuration="Release Ansi" /p:Platform=Win32 %czatyProjectFolder%\mucc.vcxproj

REM compiling tlen_czaty - Release Unicode
echo %NL%%NL%%NL% * tlen_czaty - Release: cleaning VS 10 project   (Release Unicode , Win32) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Release Unicode" /p:Platform=Win32 %czatyProjectFolder%\mucc.vcxproj
echo %NL%%NL%%NL% * tlen_czaty - Release: building VS 10 project  (Release Unicode , Win32) %NL%%NL%
call %vs10msbuild% /p:Configuration="Release Unicode" /p:Platform=Win32 %czatyProjectFolder%\mucc.vcxproj

REM compiling tlen_czaty - Release x64
echo %NL%%NL%%NL% * tlen_czaty - Release: cleaning VS 10 project   (Release x64 , x64) %NL%%NL%
call %vs10msbuild% /target:clean /p:Configuration="Release x64" /p:Platform=x64 %czatyProjectFolder%\mucc.vcxproj
echo %NL%%NL%%NL% * tlen_czaty - Release: building VS 10 project  (Release x64 , x64) %NL%%NL%
call %vs10msbuild% /p:Configuration="Release x64" /p:Platform=x64 %czatyProjectFolder%\mucc.vcxproj





:package_only
echo %NL%%NL%%NL% * COPYING FILES %NL%%NL%

RMDIR /s /q %tmpFolder%\debug_ansi\
MKDIR %tmpFolder%\debug_ansi\
RMDIR /s /q %tmpFolder%\debug_unicode\
MKDIR %tmpFolder%\debug_unicode\
RMDIR /s /q %tmpFolder%\debug_x64\
MKDIR %tmpFolder%\debug_x64\
RMDIR /s /q %tmpFolder%\release_ansi\
MKDIR %tmpFolder%\release_ansi\
RMDIR /s /q %tmpFolder%\release_unicode\
MKDIR %tmpFolder%\release_unicode\
RMDIR /s /q %tmpFolder%\release_x64\
MKDIR %tmpFolder%\release_x64\



XCOPY %tlenProjectFolder%\docs %tmpFolder%\debug_ansi\docs\ /C
COPY "%tlenProjectFolder%\Debug Ansi\Plugins\tlen.pdb" %tmpFolder%\debug_ansi\ /y
COPY "%tlenProjectFolder%\Debug Ansi\Plugins\tlen.dll" %tmpFolder%\debug_ansi\ /y
COPY "%czatyProjectFolder%\Debug Ansi\tlen_czaty.pdb" %tmpFolder%\debug_ansi\ /y
COPY "%czatyProjectFolder%\Debug Ansi\tlen_czaty.dll" %tmpFolder%\debug_ansi\ /y


XCOPY %tlenProjectFolder%\docs %tmpFolder%\debug_unicode\docs\ /C
COPY "%tlenProjectFolder%\Debug Unicode\Plugins\tlen.pdb" %tmpFolder%\debug_unicode\ /y
COPY "%tlenProjectFolder%\Debug Unicode\Plugins\tlen.dll" %tmpFolder%\debug_unicode\ /y
COPY "%czatyProjectFolder%\Debug Unicode\tlen_czaty.pdb" %tmpFolder%\debug_unicode\ /y
COPY "%czatyProjectFolder%\Debug Unicode\tlen_czaty.dll" %tmpFolder%\debug_unicode\ /y


XCOPY %tlenProjectFolder%\docs %tmpFolder%\debug_x64\docs\ /C
COPY "%tlenProjectFolder%\Debug x64\Plugins\tlen.pdb" %tmpFolder%\debug_x64\ /y
COPY "%tlenProjectFolder%\Debug x64\Plugins\tlen.dll" %tmpFolder%\debug_x64\ /y
COPY "%czatyProjectFolder%\Debug x64\tlen_czaty.pdb" %tmpFolder%\debug_x64\ /y
COPY "%czatyProjectFolder%\Debug x64\tlen_czaty.dll" %tmpFolder%\debug_x64\ /y


XCOPY %tlenProjectFolder%\docs %tmpFolder%\release_ansi\docs\ /C
COPY "%tlenProjectFolder%\Release Ansi\Plugins\tlen.dll" %tmpFolder%\release_ansi\ /y
COPY "%czatyProjectFolder%\Release Ansi\tlen_czaty.dll" %tmpFolder%\release_ansi\ /y


XCOPY %tlenProjectFolder%\docs %tmpFolder%\release_unicode\docs\ /C
COPY "%tlenProjectFolder%\Release Unicode\Plugins\tlen.dll" %tmpFolder%\release_unicode\ /y
COPY "%czatyProjectFolder%\Release Unicode\tlen_czaty.dll" %tmpFolder%\release_unicode\ /y


XCOPY %tlenProjectFolder%\docs %tmpFolder%\release_x64\docs\ /C
COPY "%tlenProjectFolder%\Release x64\Plugins\tlen.dll" %tmpFolder%\release_x64\ /y
COPY "%czatyProjectFolder%\Release x64\tlen_czaty.dll" %tmpFolder%\release_x64\ /y



echo %NL%%NL%%NL% * MAKE ARCHIVES %NL%%NL%
"%haoZipExecutable%" a %targetFolderD%\TlenAD%zipFilesPostfix%.zip %tmpFolder%\debug_ansi\*
"%haoZipExecutable%" a %targetFolderD%\TlenUD%zipFilesPostfix%.zip %tmpFolder%\debug_unicode\*
"%haoZipExecutable%" a %targetFolderD%\TlenXD%zipFilesPostfix%.zip %tmpFolder%\debug_x64\*
"%haoZipExecutable%" a %targetFolderR%\TlenA%zipFilesPostfix%.zip %tmpFolder%\release_ansi\*
"%haoZipExecutable%" a %targetFolderR%\TlenU%zipFilesPostfix%.zip %tmpFolder%\release_unicode\*
"%haoZipExecutable%" a %targetFolderR%\TlenX%zipFilesPostfix%.zip %tmpFolder%\release_x64\*


echo %NL%%NL%%NL% * CLEANING %NL%%NL%
RMDIR /s /q %tmpFolder%\debug_ansi\
RMDIR /s /q %tmpFolder%\debug_unicode\
RMDIR /s /q %tmpFolder%\debug_x64\
RMDIR /s /q %tmpFolder%\release_ansi\
RMDIR /s /q %tmpFolder%\release_unicode\
RMDIR /s /q %tmpFolder%\release_x64\



echo %NL%%NL%%NL%
echo %NL%%NL%%NL% * TLEN BUILD ALL COMPLETED %NL%%NL%
PAUSE

REM eof