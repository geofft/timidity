# Microsoft Developer Studio Project File - Name="timw32gd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=timw32gd - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "timw32gd.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "timw32gd.mak" CFG="timw32gd - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "timw32gd - Win32 Release" ("Win32 (x86) Application" 用)
!MESSAGE "timw32gd - Win32 Debug" ("Win32 (x86) Application" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "timw32gd - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I ".." /I "..\..\timidity++" /I "..\..\timidity++\interface" /I "..\..\timidity++\libarc" /I "..\..\timidity++\libunimod" /I "..\..\timidity++\timidity" /I "..\..\timidity++\utils" /I "..\..\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "HAVE_CONFIG_H" /D "_MT" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 comctl32.lib winmm.lib comdlg32.lib kernel32.lib user32.lib gdi32.lib winspool.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386 /nodefaultlib:"libc.lib" /out:"../Release/timw32gd.exe"

!ELSEIF  "$(CFG)" == "timw32gd - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /Zp16 /MTd /W3 /Gm /GX /ZI /Od /Ob0 /I "..\interface" /I "..\libarc" /I "..\libunimod" /I "..\timidity" /I "..\utils" /I ".." /I "..\..\timidity++" /I "..\..\timidity++\interface" /I "..\..\timidity++\libarc" /I "..\..\timidity++\libunimod" /I "..\..\timidity++\timidity" /I "..\..\timidity++\utils" /I "..\..\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "HAVE_CONFIG_H" /D "_MT" /D "W32GUI_DEBUG" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 libcmtd.lib comctl32.lib winmm.lib comdlg32.lib kernel32.lib user32.lib gdi32.lib winspool.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:windows /profile /debug /machine:I386 /nodefaultlib:"libc.lib" /nodefaultlib:"LIBCD" /out:"../Debug/timw32gd.exe"

!ENDIF 

# Begin Target

# Name "timw32gd - Win32 Release"
# Name "timw32gd - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_c.c"
# End Source File
# Begin Source File

SOURCE=..\interface\w32g_dib.c
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_i.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_ini.c"
# End Source File
# Begin Source File

SOURCE=..\interface\w32g_mag.c
# End Source File
# Begin Source File

SOURCE=..\timidity\w32g_ogg_dll.c
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_playlist.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_pref.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_subwin.c"
# End Source File
# Begin Source File

SOURCE=..\interface\w32g_subwin2.c
# End Source File
# Begin Source File

SOURCE=..\interface\w32g_subwin3.c
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_ut2.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_utl.c"
# End Source File
# Begin Source File

SOURCE=..\timidity\w32g_vorbis_dll.c
# End Source File
# Begin Source File

SOURCE=..\timidity\w32g_vorbisenc_dll.c
# End Source File
# Begin Source File

SOURCE=..\interface\wrdt_w32g.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE="..\..\TiMidity++\config.h"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\config_w32vc.h"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface.h"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface_w32vc.h"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g.h"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_pref.h"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_rec.h"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_res.h"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_subwin.h"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_ut2.h"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_utl.h"
# End Source File
# Begin Source File

SOURCE=..\interface\w32g_wrd.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_res.rc"
# End Source File
# End Group
# Begin Source File

SOURCE=..\Debug\utils.lib
# End Source File
# Begin Source File

SOURCE=..\Debug\libarc.lib
# End Source File
# Begin Source File

SOURCE=..\Debug\libunimod.lib
# End Source File
# Begin Source File

SOURCE=..\Debug\timidity.lib
# End Source File
# Begin Source File

SOURCE=..\Debug\interface.lib
# End Source File
# End Target
# End Project
