# Microsoft Developer Studio Project File - Name="libgsf_win32" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libgsf_win32 - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libgsf_win32.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libgsf_win32.mak" CFG="libgsf_win32 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libgsf_win32 - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libgsf_win32 - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libgsf_win32 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "libgsf_win32___Win32_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBGSF_WIN32_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "./" /I "../" /I "../gsf-win32" /I "$(GTK_BASEPATH)/include/glib-2.0" /I "$(GTK_BASEPATH)/lib/glib-2.0/include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBGSF_WIN32_EXPORTS" /D "HAVE_CONFIG_H" /D "G_DISABLE_DEPRECATED" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 gobject-2.0.lib glib-2.0.lib intl.lib iconv.lib /nologo /dll /machine:I386 /out:"Release/libgsf-win32-1-1.dll" /implib:"Release/gsf-win32-1.lib" /libpath:"Release" /libpath:"$(GTK_BASEPATH)/lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "libgsf_win32 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libgsf_win32___Win32_Debug"
# PROP BASE Intermediate_Dir "libgsf_win32___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "libgsf_win32___Win32_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBGSF_WIN32_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "./" /I "../" /I "../gsf-win32" /I "$(GTK_BASEPATH)/include/glib-2.0" /I "$(GTK_BASEPATH)/lib/glib-2.0/include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBGSF_WIN32_EXPORTS" /D "HAVE_CONFIG_H" /D "G_DISABLE_DEPRECATED" /YX /FD /GZ /c
# SUBTRACT CPP /WX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 gobject-2.0.lib glib-2.0.lib intl.lib iconv.lib gsf-1.lib /nologo /dll /debug /machine:I386 /out:"Debug/libgsf-win32-1-1.dll" /implib:"Debug/gsf-win32-1.lib" /pdbtype:sept /libpath:"Debug" /libpath:"$(GTK_BASEPATH)/lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "libgsf_win32 - Win32 Release"
# Name "libgsf_win32 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\gsf-win32\gsf-input-win32.c"
# End Source File
# Begin Source File

SOURCE="..\gsf-win32\gsf-output-win32.c"
# End Source File
# Begin Source File

SOURCE=".\gsf-win32.def"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\gsf-win32\gsf-input-win32.h"
# End Source File
# Begin Source File

SOURCE="..\gsf-win32\gsf-output-win32.h"
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
