# Microsoft Developer Studio Project File - Name="config" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Generic Project" 0x010a

CFG=config - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "config.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "config.mak" CFG="config - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "config - Win32 Release" (based on "Win32 (x86) Generic Project")
!MESSAGE "config - Win32 Debug" (based on "Win32 (x86) Generic Project")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
MTL=midl.exe

!IF  "$(CFG)" == "config - Win32 Release"

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

!ELSEIF  "$(CFG)" == "config - Win32 Debug"

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

!ENDIF 

# Begin Target

# Name "config - Win32 Release"
# Name "config - Win32 Debug"
# Begin Source File

SOURCE=.\config.txt

!IF  "$(CFG)" == "config - Win32 Release"

USERDEP__CONFI="gsf-config.h.in.stamp"	
# Begin Custom Build
InputPath=.\config.txt

"gsf-config.h.stamp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl config.pl --template ../gsf-config.h.in > gsf-config.h.stamp 
	if errorlevel 1 goto error 
	fc gsf-config.h gsf-config.h.stamp > nul 2> nul 
	if not errorlevel 1 goto unchanged 
	copy /y gsf-config.h.stamp gsf-config.h 
	goto end 
	:unchanged 
	echo gsf-config.h is unchanged 
	goto end 
	:error 
	dir nul > nul 2> nul 
	:end 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "config - Win32 Debug"

USERDEP__CONFI="gsf-config.h.in.stamp"	
# Begin Custom Build
InputPath=.\config.txt

"gsf-config.h.stamp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	perl config.pl --template ../gsf-config.h.in > gsf-config.h.stamp 
	if errorlevel 1 goto error 
	fc gsf-config.h gsf-config.h.stamp > nul 2> nul 
	if not errorlevel 1 goto unchanged 
	copy /y gsf-config.h.stamp gsf-config.h 
	goto end 
	:unchanged 
	echo gsf-config.h is unchanged 
	goto end 
	:error 
	dir nul > nul 2> nul 
	:end 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\configure.in"

!IF  "$(CFG)" == "config - Win32 Release"

# Begin Custom Build
InputPath="..\configure.in"

"gsf-config.h.in.stamp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cd .. 
	perl msvc\autom4te.hack\autoheader 
	if errorlevel 1 goto end 
	echo>msvc\gsf-config.h.in.stamp 
	:end 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "config - Win32 Debug"

# Begin Custom Build
InputPath="..\configure.in"

"gsf-config.h.in.stamp" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cd .. 
	perl msvc\autom4te.hack\autoheader 
	if errorlevel 1 goto end 
	echo>msvc\gsf-config.h.in.stamp 
	:end 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=".\gsf-config.h.stamp"

!IF  "$(CFG)" == "config - Win32 Release"

# Begin Custom Build
InputPath=.\gsf-config.h.stamp

"gsf-config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	goto end 
	:end 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "config - Win32 Debug"

# Begin Custom Build
InputPath=.\gsf-config.h.stamp

"gsf-config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	goto end 
	:end 
	
# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
