# Microsoft Developer Studio Project File - Name="libarc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libarc - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "libarc.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "libarc.mak" CFG="libarc - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "libarc - Win32 Release" ("Win32 (x86) Static Library" 用)
!MESSAGE "libarc - Win32 Debug" ("Win32 (x86) Static Library" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libarc - Win32 Release"

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
# ADD LIB32 /nologo /out:"../Release\libarc.lib"

!ELSEIF  "$(CFG)" == "libarc - Win32 Debug"

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
# ADD LIB32 /nologo /out:"../Debug\libarc.lib"

!ENDIF 

# Begin Target

# Name "libarc - Win32 Release"
# Name "libarc - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\libarc\arc.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\arc_lzh.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\arc_mime.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\arc_tar.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\arc_zip.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\deflate.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\explode.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\inflate.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\unlzh.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_b64decode.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_buff.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_cache.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_dir.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_file.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_ftp.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_hqxdecode.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_http.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_inflate.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_mem.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_news.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_newsgroup.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_pipe.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_qsdecode.c"
# End Source File
# Begin Source File

SOURCE="..\libarc\url_uudecode.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\libarc\arc.h"
# End Source File
# Begin Source File

SOURCE="..\libarc\explode.h"
# End Source File
# Begin Source File

SOURCE="..\libarc\unlzh.h"
# End Source File
# Begin Source File

SOURCE="..\libarc\url.h"
# End Source File
# Begin Source File

SOURCE="..\libarc\zip.h"
# End Source File
# End Group
# End Target
# End Project
