# Microsoft Developer Studio Project File - Name="utils" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=utils - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "utils.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "utils.mak" CFG="utils - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "utils - Win32 Release" ("Win32 (x86) Static Library" 用)
!MESSAGE "utils - Win32 Debug" ("Win32 (x86) Static Library" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "utils - Win32 Release"

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
# ADD LIB32 /nologo /out:"../Release\utils.lib"

!ELSEIF  "$(CFG)" == "utils - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\.." /I ".." /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /D "_DEBUG" /D "_MT" /D "_WINDOWS" /D "_LIB" /D "WIN32" /D "_MBCS" /D "HAVE_CONFIG_H" /YX /FD /GZ /c
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /i "..\interface" /i "..\libarc" /i "..\libunimod" /i "..\timidity" /i "..\utils" /i ".." /i "timidity" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../Debug\utils.lib"

!ENDIF 

# Begin Target

# Name "utils - Win32 Release"
# Name "utils - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\utils\bitset.c"
# End Source File
# Begin Source File

SOURCE="..\utils\fft.c"
# End Source File
# Begin Source File

SOURCE="..\utils\fft4g.c"
# End Source File
# Begin Source File

SOURCE="..\utils\mblock.c"
# End Source File
# Begin Source File

SOURCE="..\utils\memb.c"
# End Source File
# Begin Source File

SOURCE="..\utils\net.c"
# End Source File
# Begin Source File

SOURCE="..\utils\nkflib.c"
# End Source File
# Begin Source File

SOURCE="..\utils\readdir_win.c"
# End Source File
# Begin Source File

SOURCE="..\utils\strtab.c"
# End Source File
# Begin Source File

SOURCE="..\utils\support.c"
# End Source File
# Begin Source File

SOURCE="..\utils\timer.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\utils\bitset.h"
# End Source File
# Begin Source File

SOURCE="..\utils\fft.h"
# End Source File
# Begin Source File

SOURCE="..\utils\mac_readdir.h"
# End Source File
# Begin Source File

SOURCE="..\utils\mac_util.h"
# End Source File
# Begin Source File

SOURCE="..\utils\mblock.h"
# End Source File
# Begin Source File

SOURCE="..\utils\memb.h"
# End Source File
# Begin Source File

SOURCE="..\utils\net.h"
# End Source File
# Begin Source File

SOURCE="..\utils\nkflib.h"
# End Source File
# Begin Source File

SOURCE="..\utils\readdir.h"
# End Source File
# Begin Source File

SOURCE="..\utils\strtab.h"
# End Source File
# Begin Source File

SOURCE="..\utils\support.h"
# End Source File
# Begin Source File

SOURCE="..\utils\timer.h"
# End Source File
# End Group
# End Target
# End Project
