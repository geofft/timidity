# Microsoft Developer Studio Project File - Name="cfgforsf" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=cfgforsf - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "cfgforsf.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "cfgforsf.mak" CFG="cfgforsf - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "cfgforsf - Win32 Release" ("Win32 (x86) Console Application" 用)
!MESSAGE "cfgforsf - Win32 Debug" ("Win32 (x86) Console Application" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "cfgforsf - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I ".." /I ".." /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I "..\..\include" /D "NDEBUG" /D "_MT" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /D "HAVE_CONFIG_H" /D "CFG_FOR_SF" /YX /FD /c
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "cfgforsf - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I ".." /I ".." /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I "..\..\include" /D "_DEBUG" /D "_MT" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /D "HAVE_CONFIG_H" /D "CFG_FOR_SF" /YX /FD /GZ /c
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"LIBCD LIBCMT" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "cfgforsf - Win32 Release"
# Name "cfgforsf - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\timidity\common.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\controls.c"
# End Source File
# Begin Source File

SOURCE="..\interface\dumb_c.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\instrum.c"
# End Source File
# Begin Source File

SOURCE=..\timidity\quantity.c
# End Source File
# Begin Source File

SOURCE=..\timidity\reverb.c
# End Source File
# Begin Source File

SOURCE="..\timidity\sbkconv.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\sffile.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\sfitem.c"
# End Source File
# Begin Source File

SOURCE=..\timidity\smplfile.c
# End Source File
# Begin Source File

SOURCE="..\timidity\sndfont.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\tables.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\version.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\libarc\arc.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\common.h"
# End Source File
# Begin Source File

SOURCE="..\config.h"
# End Source File
# Begin Source File

SOURCE="..\config_w32vc.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\controls.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\instrum.h"
# End Source File
# Begin Source File

SOURCE="..\interface.h"
# End Source File
# Begin Source File

SOURCE="..\interface_w32vc.h"
# End Source File
# Begin Source File

SOURCE="..\utils\mblock.h"
# End Source File
# Begin Source File

SOURCE="..\utils\nkflib.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\output.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\playmidi.h"
# End Source File
# Begin Source File

SOURCE=..\timidity\quantity.h
# End Source File
# Begin Source File

SOURCE="..\timidity\readmidi.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\resample.h"
# End Source File
# Begin Source File

SOURCE=..\timidity\reverb.h
# End Source File
# Begin Source File

SOURCE="..\timidity\sffile.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\sfitem.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\sflayer.h"
# End Source File
# Begin Source File

SOURCE="..\utils\strtab.h"
# End Source File
# Begin Source File

SOURCE="..\utils\support.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\tables.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\timidity.h"
# End Source File
# Begin Source File

SOURCE="..\libarc\url.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\wrd.h"
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=..\Release\utils.lib
# End Source File
# Begin Source File

SOURCE=..\Release\libarc.lib
# End Source File
# End Target
# End Project
