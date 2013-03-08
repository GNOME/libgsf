# Microsoft Developer Studio Project File - Name="libgsf" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libgsf - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libgsf.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libgsf.mak" CFG="libgsf - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libgsf - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libgsf - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libgsf - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "libgsf___Win32_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBGSF_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "$(GTK_BASEPATH)/lib/glib-2.0/include" /I "./" /I "../" /I "../gsf" /I "$(GTK_BASEPATH)/include/glib-2.0" /I "$(GTK_BASEPATH)/include/libxml2" /I "$(GTK_BASEPATH)/include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBGSF_EXPORTS" /D "HAVE_CONFIG_H" /D "G_DISABLE_DEPRECATED" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 gobject-2.0.lib glib-2.0.lib intl.lib iconv.lib libbz2.lib xml2.lib z.lib ws2_32.lib /nologo /dll /machine:I386 /out:"Release/libgsf-1-1.dll" /implib:"Release/gsf-1.lib" /libpath:"$(GTK_BASEPATH)/lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "libgsf - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "libgsf___Win32_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBGSF_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MD /W3 /Gm /GX /ZI /Od /I "./" /I "../" /I "../gsf" /I "$(GTK_BASEPATH)/include/glib-2.0" /I "$(GTK_BASEPATH)/lib/glib-2.0/include" /I "$(GTK_BASEPATH)/include/libxml2" /I "$(GTK_BASEPATH)/include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBGSF_EXPORTS" /D "HAVE_CONFIG_H" /D "G_DISABLE_DEPRECATED" /YX /FD /GZ /c
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
# ADD LINK32 gobject-2.0.lib glib-2.0.lib intl.lib iconv.lib libbz2.lib xml2.lib z.lib ws2_32.lib /nologo /dll /debug /machine:I386 /out:"Debug/libgsf-1-1.dll" /implib:"Debug/gsf-1.lib" /pdbtype:sept /libpath:"$(GTK_BASEPATH)/lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "libgsf - Win32 Release"
# Name "libgsf - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\gsf\gsf-blob.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-clip-data.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-doc-meta-data.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-docprop-vector.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile-msole.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile-msvba.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile-stdio.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile-zip.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-bzip.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-gzip.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-iochannel.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-memory.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-proxy.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-stdio.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-textline.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-libxml.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-msole-utils.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-outfile-msole.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-outfile-stdio.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-outfile-zip.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-outfile.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-bzip.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-csv.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-gzip.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-iconv.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-iochannel.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-memory.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-stdio.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-shared-memory.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-structured-blob.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-timestamp.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-utils.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-zip-utils.c"
# End Source File
# Begin Source File

SOURCE=.\gsf.def
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\gsf\gsf-blob.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-clip-data.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-doc-meta-data.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-docprop-vector.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-impl-utils.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile-impl.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile-msole.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile-msvba.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile-stdio.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile-zip.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-bzip.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-gzip.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-impl.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-iochannel.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-memory.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-proxy.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-stdio.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-textline.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-libxml.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-meta-names.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-msole-impl.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-msole-utils.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-outfile-impl.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-outfile-msole.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-outfile-stdio.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-outfile-zip.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-outfile.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-bzip.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-csv.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-gzip.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-iconv.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-impl.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-iochannel.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-memory.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-stdio.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-shared-memory.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-structured-blob.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-timestamp.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-utils.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-zip-impl.h"
# End Source File
# Begin Source File

SOURCE=..\gsf\gsf.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
