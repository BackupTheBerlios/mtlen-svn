# Microsoft Developer Studio Project File - Name="mucc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=mucc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mucc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mucc.mak" CFG="mucc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mucc - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "mucc - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mucc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MUCC_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O1 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MUCC_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x415 /d "NDEBUG"
# ADD RSC /l 0x415 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib /nologo /base:"0x32600000" /dll /machine:I386 /out:"mucc.dll"

!ELSEIF  "$(CFG)" == "mucc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MUCC_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MUCC_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x415 /d "_DEBUG"
# ADD RSC /l 0x415 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib /nologo /dll /debug /machine:I386 /out:"mucc.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "mucc - Win32 Release"
# Name "mucc - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\AdminWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\ChatEvent.cpp
# End Source File
# Begin Source File

SOURCE=.\ChatGroup.cpp
# End Source File
# Begin Source File

SOURCE=.\ChatRoom.cpp
# End Source File
# Begin Source File

SOURCE=.\ChatUser.cpp
# End Source File
# Begin Source File

SOURCE=.\ChatWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\FontList.cpp
# End Source File
# Begin Source File

SOURCE=.\HelperDialog.cpp
# End Source File
# Begin Source File

SOURCE=.\ManagerWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\mucc.cpp
# End Source File
# Begin Source File

SOURCE=.\mucc_services.cpp
# End Source File
# Begin Source File

SOURCE=.\Options.cpp
# End Source File
# Begin Source File

SOURCE=.\Utils.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\AdminWindow.h
# End Source File
# Begin Source File

SOURCE=.\ChatEvent.h
# End Source File
# Begin Source File

SOURCE=.\ChatGroup.h
# End Source File
# Begin Source File

SOURCE=.\ChatRoom.h
# End Source File
# Begin Source File

SOURCE=.\ChatUser.h
# End Source File
# Begin Source File

SOURCE=.\ChatWindow.h
# End Source File
# Begin Source File

SOURCE=.\FontList.h
# End Source File
# Begin Source File

SOURCE=.\HelperDialog.h
# End Source File
# Begin Source File

SOURCE=.\m_mucc.h
# End Source File
# Begin Source File

SOURCE=.\ManagerWindow.h
# End Source File
# Begin Source File

SOURCE=.\mucc.h
# End Source File
# Begin Source File

SOURCE=.\mucc_services.h
# End Source File
# Begin Source File

SOURCE=.\Options.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\Utils.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\icos\admin.ico
# End Source File
# Begin Source File

SOURCE=.\icos\administration.ico
# End Source File
# Begin Source File

SOURCE=.\icos\blank.ico
# End Source File
# Begin Source File

SOURCE=.\icos\bold.ico
# End Source File
# Begin Source File

SOURCE=.\icos\chat.ico
# End Source File
# Begin Source File

SOURCE=.\icos\glowner.ico
# End Source File
# Begin Source File

SOURCE=.\icos\invite.ico
# End Source File
# Begin Source File

SOURCE=.\icos\italic.ico
# End Source File
# Begin Source File

SOURCE=.\icos\moderator.ico
# End Source File
# Begin Source File

SOURCE=.\mucc.rc
# End Source File
# Begin Source File

SOURCE=.\icos\next.ico
# End Source File
# Begin Source File

SOURCE=.\icos\options.ico
# End Source File
# Begin Source File

SOURCE=.\icos\owner.ico
# End Source File
# Begin Source File

SOURCE=.\icos\prev.ico
# End Source File
# Begin Source File

SOURCE=.\icos\r_anon.ico
# End Source File
# Begin Source File

SOURCE=.\icos\r_memb.ico
# End Source File
# Begin Source File

SOURCE=.\icos\r_mod.ico
# End Source File
# Begin Source File

SOURCE=.\icos\registered.ico
# End Source File
# Begin Source File

SOURCE=.\icos\search.ico
# End Source File
# Begin Source File

SOURCE=.\icos\tlensmall.ico
# End Source File
# Begin Source File

SOURCE=.\icos\underline.ico
# End Source File
# End Group
# End Target
# End Project
