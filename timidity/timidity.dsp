# Microsoft Developer Studio Project File - Name="timidity" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=timidity - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "timidity.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "timidity.mak" CFG="timidity - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "timidity - Win32 Release" ("Win32 (x86) Static Library" 用)
!MESSAGE "timidity - Win32 Debug" ("Win32 (x86) Static Library" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "timidity - Win32 Release"

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
# ADD CPP /nologo /Zp16 /MT /W3 /GX /O2 /Ob2 /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I ".." /I ".." /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I "..\..\include" /D "NDEBUG" /D "_MT" /D "_WINDOWS" /D "_LIB" /D "WIN32" /D "_MBCS" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /i "..\interface" /i "..\libarc" /i "..\libunimod" /i "..\timidity" /i "..\utils" /i ".." /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../Release\timidity.lib"

!ELSEIF  "$(CFG)" == "timidity - Win32 Debug"

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
# ADD CPP /nologo /Zp16 /MTd /W3 /Gm /GX /ZI /Od /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I ".." /I ".." /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I "..\..\include" /D "_DEBUG" /D "_MT" /D "_WINDOWS" /D "_LIB" /D "WIN32" /D "_MBCS" /D "HAVE_CONFIG_H" /YX /FD /GZ /c
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /i "..\interface" /i "..\libarc" /i "..\libunimod" /i "..\timidity" /i "..\utils" /i ".." /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../Debug\timidity.lib"

!ENDIF 

# Begin Target

# Name "timidity - Win32 Release"
# Name "timidity - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\timidity\aiff_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\aq.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\au_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\audio_cnv.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\common.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\controls.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\effect.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\filter.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\freq.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\gogo_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\instrum.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\list_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\loadtab.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\m2m.c"
# End Source File
# Begin Source File

SOURCE=.\mfi.c
# End Source File
# Begin Source File

SOURCE="..\timidity\mfnode.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\miditrace.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\mix.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\mod.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\mod2midi.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\modmid_a.c"
# End Source File
# Begin Source File

SOURCE=.\mt19937ar.c
# End Source File
# Begin Source File

SOURCE=.\optcode.c
# End Source File
# Begin Source File

SOURCE="..\timidity\output.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\playmidi.c"
# End Source File
# Begin Source File

SOURCE=.\quantity.c
# End Source File
# Begin Source File

SOURCE="..\timidity\raw_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\rcp.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\readmidi.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\recache.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\resample.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\reverb.c"
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

SOURCE="..\timidity\smfconv.c"
# End Source File
# Begin Source File

SOURCE=.\smplfile.c
# End Source File
# Begin Source File

SOURCE="..\timidity\sndfont.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\tables.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\timidity.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\version.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\vorbis_a.c"

!IF  "$(CFG)" == "timidity - Win32 Release"

# ADD CPP /Zp16 /O2

!ELSEIF  "$(CFG)" == "timidity - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\timidity\w32_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\w32_gogo.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\wave_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\wrd_read.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\wrdt.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\timidity\aenc.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\aq.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\audio_cnv.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\audriv.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\common.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\controls.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\dlutils.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\filter.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\gogo_a.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\instrum.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\mac_com.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\mac_main.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\mfnode.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\miditrace.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\mix.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\mod.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\mod2midi.h"
# End Source File
# Begin Source File

SOURCE=.\mt19937ar.h
# End Source File
# Begin Source File

SOURCE=.\optcode.h
# End Source File
# Begin Source File

SOURCE="..\timidity\output.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\playmidi.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\readmidi.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\recache.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\resample.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\reverb.h"
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

SOURCE="..\timidity\smfconv.h"
# End Source File
# Begin Source File

SOURCE=.\sysdep.h
# End Source File
# Begin Source File

SOURCE="..\timidity\tables.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\timidity.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\w32_gogo.h"
# End Source File
# Begin Source File

SOURCE="..\timidity\wrd.h"
# End Source File
# End Group
# End Target
# End Project
