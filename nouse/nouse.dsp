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

SOURCE="..\timidity\alsa_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\audriv_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\audriv_al.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\audriv_mme.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\audriv_none.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\bsd20_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\dl_dld.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\dl_dlopen.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\dl_hpux.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\dl_w32.c"
# End Source File
# Begin Source File

SOURCE="..\interface\dynamic_c.c"
# End Source File
# Begin Source File

SOURCE="..\interface\emacs_c.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\esd_a.c"
# End Source File
# Begin Source File

SOURCE="..\interface\gtk_c.c"
# End Source File
# Begin Source File

SOURCE="..\interface\gtk_i.c"
# End Source File
# Begin Source File

SOURCE="..\interface\gtk_p.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\hpux_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\hpux_d_a.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\mac_a.c"
# End Source File
# Begin Source File

SOURCE="..\interface\mac_c.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\mac_dlog.c"
# End Source File
# Begin Source File

SOURCE="..\interface\mac_mag.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\mac_main.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\mac_qt_a.c"
# End Source File
# Begin Source File

SOURCE="..\utils\mac_readdir.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\mac_soundspec.c"
# End Source File
# Begin Source File

SOURCE="..\interface\mac_trace.c"
# End Source File
# Begin Source File

SOURCE="..\utils\mac_util.c"
# End Source File
# Begin Source File

SOURCE="..\interface\mac_wrdwindow.c"
# End Source File
# Begin Source File

SOURCE="..\interface\motif_c.c"
# End Source File
# Begin Source File

SOURCE="..\interface\motif_i.c"
# End Source File
# Begin Source File

SOURCE="..\interface\motif_p.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\nas_a.c"
# End Source File
# Begin Source File

SOURCE="..\interface\ncurs_c.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\oss_a.c"
# End Source File
# Begin Source File

SOURCE="..\interface\server_c.c"
# End Source File
# Begin Source File

SOURCE="..\interface\slang_c.c"
# End Source File
# Begin Source File

SOURCE="..\interface\soundspec.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\sun_a.c"
# End Source File
# Begin Source File

SOURCE="..\interface\tk_c.c"
# End Source File
# Begin Source File

SOURCE="..\timidity\w32g_a.c"
# End Source File
# Begin Source File

SOURCE="..\interface\w32g_wrdwindow.c"
# End Source File
# Begin Source File

SOURCE="..\interface\wrdt_mac.c"
# End Source File
# Begin Source File

SOURCE="..\interface\wrdt_w32g.c"
# End Source File
# Begin Source File

SOURCE="..\interface\wrdt_wcon.c"
# End Source File
# Begin Source File

SOURCE="..\interface\wrdt_x.c"
# End Source File
# Begin Source File

SOURCE="..\interface\x_mag.c"
# End Source File
# Begin Source File

SOURCE="..\interface\x_sherry.c"
# End Source File
# Begin Source File

SOURCE="..\interface\x_wrdwindow.c"
# End Source File
# Begin Source File

SOURCE="..\interface\xaw_c.c"
# End Source File
# Begin Source File

SOURCE="..\interface\xaw_i.c"
# End Source File
# Begin Source File

SOURCE="..\interface\xaw_redef.c"
# End Source File
# Begin Source File

SOURCE="..\interface\xskin_c.c"
# End Source File
# Begin Source File

SOURCE="..\interface\xskin_i.c"
# End Source File
# Begin Source File

SOURCE="..\interface\xskin_loadBMP.c"
# End Source File
# Begin Source File

SOURCE="..\interface\xskin_spectrum.c"
# End Source File
# End Target
# End Project
