# Microsoft Developer Studio Project File - Name="libgsf" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

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
!MESSAGE "libgsf - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libgsf - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
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
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GR /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libgsf - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GR /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "libgsf - Win32 Release"
# Name "libgsf - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\gsf\gsf-infile-msole.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-infile-msvba.c"
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

SOURCE="..\gsf\gsf-input-stdio.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input-textline.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-input.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-msole-utils.c"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-outfile-msole.c"
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

SOURCE="..\gsf\gsf-output-gzip.c"
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
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\gsf-config.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-doc-meta-data.h"
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

SOURCE="..\gsf\gsf-outfile-zip.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-outfile.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-bzip.h"
# End Source File
# Begin Source File

SOURCE="..\gsf\gsf-output-gzip.h"
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
# End Target
# End Project
