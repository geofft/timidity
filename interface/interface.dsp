# Microsoft Developer Studio Project File - Name="interface" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=interface - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "interface.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "interface.mak" CFG="interface - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "interface - Win32 Release" ("Win32 (x86) Static Library" 用)
!MESSAGE "interface - Win32 Debug" ("Win32 (x86) Static Library" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "interface - Win32 Release"

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
# ADD CPP /nologo /Zp16 /MT /W3 /GX /O2 /Ob2 /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I ".." /I ".." /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /D "NDEBUG" /D "_MT" /D "_WINDOWS" /D "_LIB" /D "WIN32" /D "_MBCS" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /i "..\interface" /i "..\libarc" /i "..\libunimod" /i "..\timidity" /i "..\utils" /i ".." /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../Release\interface.lib"

!ELSEIF  "$(CFG)" == "interface - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I ".." /I "..\" /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /D "_DEBUG" /D "_MT" /D "_WINDOWS" /D "_LIB" /D "WIN32" /D "_MBCS" /D "HAVE_CONFIG_H" /YX /FD /GZ /c
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /i "..\interface" /i "..\libarc" /i "..\libunimod" /i "..\timidity" /i "..\utils" /i ".." /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../Debug\interface.lib"

!ENDIF 

# Begin Target

# Name "interface - Win32 Release"
# Name "interface - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\interface\dumb_c.c"
# End Source File
# Begin Source File

SOURCE="..\interface\vt100.c"
# End Source File
# Begin Source File

SOURCE="..\interface\vt100_c.c"
# End Source File
# Begin Source File

SOURCE="..\interface\VTPrsTbl.c"
# End Source File
# Begin Source File

SOURCE="..\interface\wrdt_dumb.c"
# End Source File
# Begin Source File

SOURCE="..\interface\wrdt_tty.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\interface\gtk_h.h"
# End Source File
# Begin Source File

SOURCE="..\interface\mac_c.h"
# End Source File
# Begin Source File

SOURCE="..\interface\mac_mag.h"
# End Source File
# Begin Source File

SOURCE="..\interface\mac_wrdwindow.h"
# End Source File
# Begin Source File

SOURCE="..\interface\motif.h"
# End Source File
# Begin Source File

SOURCE="..\interface\server_defs.h"
# End Source File
# Begin Source File

SOURCE="..\interface\soundspec.h"
# End Source File
# Begin Source File

SOURCE="..\interface\vt100.h"
# End Source File
# Begin Source File

SOURCE="..\interface\VTparse.h"
# End Source File
# Begin Source File

SOURCE="..\interface\w32g_utl.h"
# End Source File
# Begin Source File

SOURCE="..\interface\x_mag.h"
# End Source File
# Begin Source File

SOURCE="..\interface\x_sherry.h"
# End Source File
# Begin Source File

SOURCE="..\interface\x_wrdwindow.h"
# End Source File
# Begin Source File

SOURCE="..\interface\xaw.h"
# End Source File
# Begin Source File

SOURCE="..\interface\xskin.h"
# End Source File
# End Group
# End Target
# End Project
