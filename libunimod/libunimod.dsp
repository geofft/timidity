# Microsoft Developer Studio Project File - Name="libunimod" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libunimod - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "libunimod.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "libunimod.mak" CFG="libunimod - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "libunimod - Win32 Release" ("Win32 (x86) Static Library" 用)
!MESSAGE "libunimod - Win32 Debug" ("Win32 (x86) Static Library" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libunimod - Win32 Release"

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
# ADD LIB32 /nologo /out:"../Release\libunimod.lib"

!ELSEIF  "$(CFG)" == "libunimod - Win32 Debug"

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
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I ".." /I ".." /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /D "_DEBUG" /D "_MT" /D "_WINDOWS" /D "_LIB" /D "WIN32" /D "_MBCS" /D "HAVE_CONFIG_H" /YX /FD /GZ /c
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /i "..\interface" /i "..\libarc" /i "..\libunimod" /i "..\timidity" /i "..\utils" /i ".." /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../Debug\libunimod.lib"

!ENDIF 

# Begin Target

# Name "libunimod - Win32 Release"
# Name "libunimod - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\libunimod\load_669.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_amf.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_dsm.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_far.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_gdm.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_imf.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_it.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_m15.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_med.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_mod.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_mtm.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_okt.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_s3m.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_stm.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_stx.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_ult.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_uni.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\load_xm.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\mloader.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\mlutil.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\mmsup.c"
# End Source File
# Begin Source File

SOURCE="..\libunimod\munitrk.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\libunimod\unimod.h"
# End Source File
# Begin Source File

SOURCE="..\libunimod\unimod_priv.h"
# End Source File
# End Group
# End Target
# End Project
