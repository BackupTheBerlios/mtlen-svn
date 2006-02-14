# Microsoft Developer Studio Project File - Name="Tlen" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Tlen - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Tlen.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Tlen.mak" CFG="Tlen - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Tlen - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Tlen - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Tlen - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "TLEN_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O1 /I "../../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "TLEN_EXPORTS" /D "TLEN_PLUGIN" /FAcs /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib version.lib winmm.lib /nologo /base:"0x32600000" /dll /map /machine:I386 /out:"tlen.dll" /libpath:"codec" /ALIGN:4096 /ignore:4108
# SUBTRACT LINK32 /pdb:none /nodefaultlib

!ELSEIF  "$(CFG)" == "Tlen - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "TLEN_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "TLEN_EXPORTS" /D "TLEN_PLUGIN" /FAcs /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib version.lib winmm.lib /nologo /base:"0x32600000" /dll /map /debug /machine:I386 /out:"tlen.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "Tlen - Win32 Release"
# Name "Tlen - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "GSMCodec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\codec\gsm.h
# End Source File
# Begin Source File

SOURCE=.\codec\gsm_codec.c
# End Source File
# Begin Source File

SOURCE=.\codec\gsm_long.c
# End Source File
# Begin Source File

SOURCE=.\codec\gsm_lpc.c
# End Source File
# Begin Source File

SOURCE=.\codec\gsm_preprocess.c
# End Source File
# Begin Source File

SOURCE=.\codec\gsm_rpe.c
# End Source File
# Begin Source File

SOURCE=.\codec\gsm_short.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\jabber_iq.c
# End Source File
# Begin Source File

SOURCE=.\jabber_iqid.c
# End Source File
# Begin Source File

SOURCE=.\jabber_list.c
# End Source File
# Begin Source File

SOURCE=.\jabber_misc.c
# End Source File
# Begin Source File

SOURCE=.\jabber_opt.c
# End Source File
# Begin Source File

SOURCE=.\jabber_ssl.c
# End Source File
# Begin Source File

SOURCE=.\jabber_svc.c
# End Source File
# Begin Source File

SOURCE=.\jabber_thread.c
# End Source File
# Begin Source File

SOURCE=.\jabber_util.c
# End Source File
# Begin Source File

SOURCE=.\jabber_ws.c
# End Source File
# Begin Source File

SOURCE=.\jabber_xml.c
# End Source File
# Begin Source File

SOURCE=.\sha1.c
# End Source File
# Begin Source File

SOURCE=.\tlen.c
# End Source File
# Begin Source File

SOURCE=.\tlen.rc
# End Source File
# Begin Source File

SOURCE=.\tlen_advsearch.c
# End Source File
# Begin Source File

SOURCE=.\tlen_file.c
# End Source File
# Begin Source File

SOURCE=.\tlen_muc.c
# End Source File
# Begin Source File

SOURCE=.\tlen_userinfo.c
# End Source File
# Begin Source File

SOURCE=.\tlen_voice.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\jabber.h
# End Source File
# Begin Source File

SOURCE=.\jabber_iq.h
# End Source File
# Begin Source File

SOURCE=.\jabber_list.h
# End Source File
# Begin Source File

SOURCE=.\jabber_ssl.h
# End Source File
# Begin Source File

SOURCE=.\jabber_xml.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\sha1.h
# End Source File
# Begin Source File

SOURCE=.\tlen_file.h
# End Source File
# Begin Source File

SOURCE=.\tlen_muc.h
# End Source File
# Begin Source File

SOURCE=.\tlen_voice.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\icos\inbox.ico
# End Source File
# Begin Source File

SOURCE=.\icos\microphone.ico
# End Source File
# Begin Source File

SOURCE=.\icos\muc.ico
# End Source File
# Begin Source File

SOURCE=.\icos\sendmail.ico
# End Source File
# Begin Source File

SOURCE=.\icos\speaker.ico
# End Source File
# Begin Source File

SOURCE=.\icos\tlensmall.ico
# End Source File
# End Group
# Begin Source File

SOURCE=.\docs\changelog_tlen.txt
# End Source File
# End Target
# End Project
