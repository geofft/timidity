# Microsoft Developer Studio Project File - Name="nouse" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Generic Project" 0x010a

CFG=nouse - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "nouse.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "nouse.mak" CFG="nouse - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "nouse - Win32 Release" ("Win32 (x86) Generic Project" 用)
!MESSAGE "nouse - Win32 Debug" ("Win32 (x86) Generic Project" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
MTL=midl.exe

!IF  "$(CFG)" == "nouse - Win32 Release"

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

!ELSEIF  "$(CFG)" == "nouse - Win32 Debug"

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

# Name "nouse - Win32 Release"
# Name "nouse - Win32 Debug"
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\alsa_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\audriv_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\audriv_al.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\audriv_mme.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\audriv_none.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\bsd20_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\dl_dld.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\dl_dlopen.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\dl_hpux.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\dl_w32.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\dynamic_c.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\emacs_c.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\esd_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\gtk_c.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\gtk_i.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\gtk_p.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\hpux_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\hpux_d_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\mac_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\mac_c.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\mac_dlog.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\mac_mag.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\mac_main.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\mac_qt_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\utils\mac_readdir.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\mac_soundspec.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\mac_trace.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\utils\mac_util.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\mac_wrdwindow.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\motif_c.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\motif_i.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\motif_p.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\nas_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\ncurs_c.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\oss_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\server_c.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\slang_c.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\soundspec.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\sun_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\tk_c.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\timidity\w32g_a.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\w32g_wrdwindow.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\wrdt_mac.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\wrdt_w32g.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\wrdt_wcon.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\wrdt_x.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\x_mag.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\x_sherry.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\x_wrdwindow.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\xaw_c.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\xaw_i.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\xaw_redef.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\xskin_c.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\xskin_i.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\xskin_loadBMP.c"
# End Source File
# Begin Source File

SOURCE="..\..\TiMidity++\interface\xskin_spectrum.c"
# End Source File
# End Target
# End Project
