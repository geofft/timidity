#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include "interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <stddef.h>
#include <windows.h>
#undef RC_NONE
#include <shlobj.h>

#if defined(__CYGWIN32__) || defined(__MINGW32__)
#ifndef HAVE_NEW_MMSYSTEM
#include <commdlg.h>
#ifndef TPM_TOPALIGN
#define TPM_TOPALIGN	0x0000L	/* for old version of cygwin */
#endif
#define TIME_ONESHOT 0
#define TIME_PERIODIC 1
int WINAPI timeSetEvent(UINT uDelay, UINT uResolution,
			     void *fptc, DWORD dwUser, UINT fuEvent);
int WINAPI timeKillEvent(UINT uTimerID);
#endif
#else
#include <commctrl.h>
#endif /* __CYGWIN32__ */

#include <commctrl.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <shlobj.h>

#include <windowsx.h>	/* There is no <windowsx.h> on CYGWIN.
			 * Edit_* and ListBox_* are defined in
			 * <windowsx.h>
			 */

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"

#define HAVE_SYN_CONSOLE
#ifdef HAVE_SYN_CONSOLE
static HWND hConsoleWnd;
void InitConsoleWnd(HWND hParentWnd);
#endif

#if defined(__CYGWIN32__) || defined(__MINGW32__)
#define WIN32GCC
WINAPI void InitCommonControls(void);
#endif

#include "w32g.h"
#include "w32g_utl.h"
#include "w32g_pref.h"
#include "w32g_res.h"

#ifdef IA_W32G_SYN

typedef struct w32g_syn_t_ {
	UINT nid_uID;
	HWND nid_hWnd;
	HICON hIcon;
	int argc;
	char **argv;
	HANDLE gui_hThread;
	DWORD gui_dwThreadId;
	HANDLE syn_hThread;
	DWORD syn_dwThreadId;
//	int syn_ThreadPriority;
	HANDLE hMutex;
	int volatile quit_state;
} w32g_syn_t;
static w32g_syn_t w32g_syn;

#define MYWM_NOTIFYICON (WM_USER+501)
#define MYWM_QUIT (WM_USER+502)
#define W32G_SYN_NID_UID 12301
#define W32G_SYNWIN_CLASSNAME "TWSYNTH GUI"
#define W32G_MUTEX_NAME "TWSYNTH GUI"
#define W32G_SYN_TIP "TWSYNTH GUI"

// 各種変数 (^^;;;
HINSTANCE hInst = NULL;
int PlayerLanguage = LANGUAGE_ENGLISH;
int IniFileAutoSave = 1;
char *IniFile;
char *ConfigFile;
char *PlaylistFile;
char *PlaylistHistoryFile;
char *MidiFileOpenDir;
char *ConfigFileOpenDir;
char *PlaylistFileOpenDir;
int SecondMode = 0;
BOOL PosSizeSave = TRUE;
int DocMaxSize;
char *DocFileExt;
int AutoloadPlaylist = 0;
int AutosavePlaylist = 0;
int SeachDirRecursive = 0;
int DocWndIndependent = 0;
int DocWndAutoPopup = 0;
int TraceGraphicFlag;
int ProcessPriority;
int PlayerThreadPriority;
int MidiPlayerThreadPriority;
int MainThreadPriority;
int GUIThreadPriority;
int TracerThreadPriority;
int WrdThreadPriority;
int SubWindowMax = 5;
int InitMinimizeFlag = 0;
int DebugWndStartFlag = 1;
int ConsoleWndStartFlag = 0;
int ListWndStartFlag = 0;
int TracerWndStartFlag = 0;
int DocWndStartFlag = 0;
int WrdWndStartFlag = 0;
int DebugWndFlag = 1;
int ConsoleWndFlag = 1;
int ListWndFlag = 1;
int TracerWndFlag = 0;
int DocWndFlag = 1;
int WrdWndFlag = 0;
int SoundSpecWndFlag = 0;
int WrdGraphicFlag;
int TraceGraphicFlag;
int w32g_auto_output_mode = 0;
char *w32g_output_dir = NULL;


extern void CmdLineToArgv(LPSTR lpCmdLine, int *argc, CHAR ***argv);

static LRESULT CALLBACK SynWinProc ( HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam );
static int start_syn_thread ( void );
static void WINAPI syn_thread ( void );

// ポップアップメニュー
#define IDM_NOTHING	100
#define IDM_QUIT	101
#define IDM_START	102
// #define IDM_STOP	103
#define IDM_SYSTEM_RESET	104
#define IDM_GM_SYSTEM_RESET	105
#define IDM_GS_SYSTEM_RESET	106
#define IDM_XG_SYSTEM_RESET	107
#define	IDM_CHANGE_GM_SYSTEM	108
#define	IDM_CHANGE_GS_SYSTEM	109
#define	IDM_CHANGE_XG_SYSTEM	110
#define	IDM_CHANGE_DEFAULT_SYSTEM	111
#define IDM_PREFERENCE	112
#define IDM_CONSOLE_WND 113
#define IDM_SYN_THREAD_PRIORITY_LOWEST		121
#define IDM_SYN_THREAD_PRIORITY_BELOW_NORMAL	122
#define IDM_SYN_THREAD_PRIORITY_NORMAL	123
#define IDM_SYN_THREAD_PRIORITY_ABOVE_NORMAL 124
#define IDM_SYN_THREAD_PRIORITY_HIGHEST 125
#define IDM_VERSION 126
#define IDM_TIMIDITY 127

#define W32G_SYN_MESSAGE_MAX 100
#define W32G_SYN_NONE	0
#define W32G_SYN_QUIT	10
#define W32G_SYN_START	11		// 演奏状態へ移行
#define W32G_SYN_STOP	12		// 演奏停止状態へ移行
#define W32G_SYN_GS_SYSTEM_RESET 21
#define W32G_SYN_XG_SYSTEM_RESET 22
#define W32G_SYN_SYSTEM_RESET 23
#define W32G_SYN_GM_SYSTEM_RESET 24
#define W32G_SYN_CHANGE_GS_SYSTEM 25
#define W32G_SYN_CHANGE_XG_SYSTEM 26
#define W32G_SYN_CHANGE_GM_SYSTEM 27
#define W32G_SYN_CHANGE_DEFAULT_SYSTEM 28
typedef struct w32g_syn_message_t_ {
	int cmd;
} w32g_syn_message_t;
static volatile enum { stop, run, quit, none } w32g_syn_status, w32g_syn_status_prev; 
#ifndef MAX_PORT
#define MAX_PORT 4
#endif
int w32g_syn_id_port[MAX_PORT];
int w32g_syn_port_num = 2;

extern int ctl_pass_playing_list2(int n, char *args[]);
extern void winplaymidi(void);

w32g_syn_message_t msg_loopbuf[W32G_SYN_MESSAGE_MAX];
int msg_loopbuf_start = -1;
int msg_loopbuf_end = -1;
extern int system_mode;
HANDLE msg_loopbuf_hMutex = NULL; // 排他処理用
int syn_ThreadPriority;	// シンセスレッドのプライオリティ

extern int volatile stream_max_compute;	// play_event() の compute_data() で計算を許す最大時間。

static int w32g_syn_create_win ( void );
static int w32g_syn_main ( void );
static LRESULT CALLBACK SynWinProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static int start_syn_thread ( void );
static void WINAPI syn_thread ( void );
static void terminate_syn_thread ( void );
static int wait_for_termination_of_syn_thread ( void );
int w32g_message_set ( int cmd );
int w32g_message_get ( w32g_syn_message_t *msg );
void w32g_syn_ctl_pass_playing_list ( int n_, char *args_[] );
int w32g_syn_do_before_pref_apply ( void );
int w32g_syn_do_after_pref_apply ( void );

static void VersionWnd(HWND hParentWnd);
static void TiMidityWnd(HWND hParentWnd);

/*
  構造
	　メインスレッド：GUIのメッセージループ
	　シンセサイザースレッド：発音部分
*/


int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
				LPSTR lpCmdLine, int nCmdShow)
{
	{ // 今のところ２重起動はできないようにしとく。
		HANDLE hMutex = OpenMutex ( 0, FALSE, W32G_MUTEX_NAME );
		if ( hMutex != NULL ) {
			CloseHandle ( hMutex );
			return 0;
		}
		w32g_syn.hMutex = CreateMutex ( NULL, TRUE, W32G_MUTEX_NAME );
		if ( w32g_syn.hMutex == NULL ) {
			return 0;
		}
	}
	
	CmdLineToArgv ( lpCmdLine, &w32g_syn.argc, &w32g_syn.argv );
//	wrdt=wrdt_list[0];
	
	hInst = hInstance;
	w32g_syn.gui_hThread = GetCurrentThread ();
	w32g_syn.gui_dwThreadId = GetCurrentThreadId ();
	w32g_syn.quit_state = 0;
	w32g_syn_main ();
	
	ReleaseMutex ( w32g_syn.hMutex );
	CloseHandle ( w32g_syn.hMutex );
	
	return 0;
}

static int w32g_syn_create_win ( void )
{
	WNDCLASS wndclass ;
	wndclass.style         = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc   = SynWinProc ;
	wndclass.cbClsExtra    = 0 ;
	wndclass.cbWndExtra    = 0 ;
	wndclass.hInstance     = hInst ;
	wndclass.hIcon         = w32g_syn.hIcon;
	wndclass.hCursor       = LoadCursor(0,IDC_ARROW) ;
	wndclass.hbrBackground = (HBRUSH)(COLOR_SCROLLBAR + 1);
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName =  W32G_SYNWIN_CLASSNAME;
	RegisterClass(&wndclass);
	w32g_syn.nid_hWnd = CreateWindowEx ( WS_EX_DLGMODALFRAME, W32G_SYNWIN_CLASSNAME, 0,
		WS_SYSMENU,
		CW_USEDEFAULT,0, 10, 10,0,0,hInst,0 );
	if ( w32g_syn.nid_hWnd == NULL ) {
		return -1;
	}
	ShowWindow ( w32g_syn.nid_hWnd, SW_HIDE );
	UpdateWindow ( w32g_syn.nid_hWnd );		// 必要ないと思うんだけど。
	return 0;
}

// return
// 0 : OK
// -1 : FATAL ERROR
static int w32g_syn_main ( void )
{
	int i;
	MSG msg;

  InitCommonControls();

	w32g_syn.nid_uID = W32G_SYN_NID_UID;
	w32g_syn.nid_hWnd = NULL;
	w32g_syn.hIcon = LoadIcon(hInst,MAKEINTRESOURCE(IDI_ICON_TIMIDITY));
	syn_ThreadPriority = THREAD_PRIORITY_NORMAL;
	for ( i = 0; i <= MAX_PORT; i ++ ) {
		w32g_syn_id_port[i] = i + 1;
	}

	if ( w32g_syn_create_win() ) {
			MessageBox ( NULL, "Fatal Error", "ERROR", MB_OK );
			return -1;
	}

	while( GetMessage(&msg,NULL,0,0) ){
		if ( msg.message == MYWM_QUIT ) {
			if ( w32g_syn.quit_state < 1 ) w32g_syn.quit_state = 1;
			if ( hConsoleWnd != NULL ) DestroyWindow ( hConsoleWnd );
			DestroyWindow ( w32g_syn.nid_hWnd );
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	while ( w32g_syn.quit_state < 2 ) {
		Sleep ( 300 );
	}

	return 0;
}

static VOID CALLBACK forced_exit ( HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime )
{
	exit ( 0 );
}


static LRESULT CALLBACK
SynWinProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	BOOL bRes;
	static int have_popupmenu = 0;
	switch (uMess) {
	case WM_CREATE:
		{ // Add the icon into the status area of the task bar.
			NOTIFYICONDATA nid;
			nid.cbSize = sizeof ( NOTIFYICONDATA );
			nid.hWnd = w32g_syn.nid_hWnd = hwnd; 
			nid.uID = w32g_syn.nid_uID; 
			nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
			nid.uCallbackMessage = MYWM_NOTIFYICON; 
			nid.hIcon = w32g_syn.hIcon; 
			strcpy ( nid.szTip, W32G_SYN_TIP );
			bRes = Shell_NotifyIcon ( NIM_ADD, &nid );
			if ( bRes == FALSE ) {
				MessageBox ( NULL, "Fatal Error", "ERROR", MB_OK );
				DestroyWindow ( hwnd );
			  PostQuitMessage ( 0 );
				return -1;
			}
		}
		start_syn_thread ();
		break;
	case WM_DESTROY:
		{
			int i;
			terminate_syn_thread();
			for ( i = 0; i < 4; i ++ ) {
				if ( wait_for_termination_of_syn_thread() )
					break;
			}
		}
		{ // Delete the icon from the status area of the task bar.
			NOTIFYICONDATA nid;
			int i;
			nid.cbSize = sizeof ( NOTIFYICONDATA );
			nid.hWnd = w32g_syn.nid_hWnd; 
			nid.uID = w32g_syn.nid_uID; 
			for ( i = 1; i <= 10; i ++ ) {
				bRes = Shell_NotifyIcon ( NIM_DELETE, &nid );
				if ( bRes == TRUE )
					break;
				if ( i >= 10 ) {
					MessageBox ( NULL, "Fatal Error", "ERROR", MB_OK );
				}
			}
		}
	  PostQuitMessage ( 0 );
	  break;
	case MYWM_NOTIFYICON:
	{
		if ( (UINT)wParam == w32g_syn.nid_uID ) {
	    if ( (UINT)lParam == WM_RBUTTONDOWN || (UINT)lParam == WM_LBUTTONDOWN) { 
				int priority_flag[10];
				POINT point;
				HMENU hMenu, hMenuReset, hMenuChange, hMenuSynPriority;
				if ( have_popupmenu )
					break;
				have_popupmenu = 1;
				if ( syn_ThreadPriority == THREAD_PRIORITY_LOWEST )
					priority_flag[0] = MF_CHECKED;
				else
					priority_flag[0] = 0;
				if ( syn_ThreadPriority == THREAD_PRIORITY_BELOW_NORMAL )
					priority_flag[1] = MF_CHECKED;
				else
					priority_flag[1] = 0;
				if ( syn_ThreadPriority == THREAD_PRIORITY_NORMAL )
					priority_flag[2] = MF_CHECKED;
				else
					priority_flag[2] = 0;
				if ( syn_ThreadPriority == THREAD_PRIORITY_ABOVE_NORMAL )
					priority_flag[3] = MF_CHECKED;
				else
					priority_flag[3] = 0;
				if ( syn_ThreadPriority == THREAD_PRIORITY_HIGHEST )
					priority_flag[4] = MF_CHECKED;
				else
					priority_flag[4] = 0;
				hMenu = CreatePopupMenu ();
				hMenuReset = CreateMenu ();
				hMenuChange = CreateMenu ();
				hMenuSynPriority = CreateMenu ();
				if (PlayerLanguage == LANGUAGE_JAPANESE) {
					if ( w32g_syn_status == run ) {
						AppendMenu ( hMenu, MF_STRING, IDM_STOP, "シンセ停止");
					} else if ( w32g_syn_status == stop ) {
						AppendMenu ( hMenu, MF_STRING, IDM_START, "シンセ開始");
					} else if ( w32g_syn_status == quit ) { 
						AppendMenu ( hMenu, MF_STRING | MF_GRAYED, IDM_START, "終了中……");
					}
					AppendMenu ( hMenu, MF_STRING, IDM_SYSTEM_RESET, "システムリセット");
					switch ( system_mode ) {
					case GM_SYSTEM_MODE:
						AppendMenu ( hMenuReset, MF_STRING | MF_CHECKED, IDM_GM_SYSTEM_RESET, "GM リセット");
						AppendMenu ( hMenuReset, MF_STRING, IDM_GS_SYSTEM_RESET, "GS リセット");
						AppendMenu ( hMenuReset, MF_STRING, IDM_XG_SYSTEM_RESET, "XG リセット");
						AppendMenu ( hMenuChange, MF_STRING | MF_CHECKED, IDM_CHANGE_GM_SYSTEM, "GM システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GS_SYSTEM, "GS システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_XG_SYSTEM, "XG システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_DEFAULT_SYSTEM, "デフォルトのシステムへ変更");
						break;
					case GS_SYSTEM_MODE:
						AppendMenu ( hMenuReset, MF_STRING, IDM_GM_SYSTEM_RESET, "GM リセット");
						AppendMenu ( hMenuReset, MF_STRING | MF_CHECKED, IDM_GS_SYSTEM_RESET, "GS リセット");
						AppendMenu ( hMenuReset, MF_STRING, IDM_XG_SYSTEM_RESET, "XG リセット");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GM_SYSTEM, "GM システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING | MF_CHECKED, IDM_CHANGE_GS_SYSTEM, "GS システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_XG_SYSTEM, "XG システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_DEFAULT_SYSTEM, "デフォルトのシステムへ変更");
						break;
					case XG_SYSTEM_MODE:
						AppendMenu ( hMenuReset, MF_STRING, IDM_GM_SYSTEM_RESET, "GM リセット");
						AppendMenu ( hMenuReset, MF_STRING, IDM_GS_SYSTEM_RESET, "GS リセット");
						AppendMenu ( hMenuReset, MF_STRING | MF_CHECKED, IDM_XG_SYSTEM_RESET, "XG リセット");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GM_SYSTEM, "GM システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GS_SYSTEM, "GS システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING | MF_CHECKED, IDM_CHANGE_XG_SYSTEM, "XG システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_DEFAULT_SYSTEM, "デフォルトのシステムへ変更");
						break;
					default:
					case DEFAULT_SYSTEM_MODE:
						AppendMenu ( hMenuReset, MF_STRING, IDM_GM_SYSTEM_RESET, "GM リセット");
						AppendMenu ( hMenuReset, MF_STRING, IDM_GS_SYSTEM_RESET, "GS リセット");
						AppendMenu ( hMenuReset, MF_STRING, IDM_XG_SYSTEM_RESET, "XG リセット");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GM_SYSTEM, "GM システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GS_SYSTEM, "GS システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_XG_SYSTEM, "XG システムへ変更");
						AppendMenu ( hMenuChange, MF_STRING | MF_CHECKED, IDM_CHANGE_DEFAULT_SYSTEM, "デフォルトのシステムへ変更");
						break;
					}
					AppendMenu ( hMenuSynPriority, MF_STRING | priority_flag[0], IDM_SYN_THREAD_PRIORITY_LOWEST, "低い");
					AppendMenu ( hMenuSynPriority, MF_STRING | priority_flag[1], IDM_SYN_THREAD_PRIORITY_BELOW_NORMAL, "少し低い");
					AppendMenu ( hMenuSynPriority, MF_STRING | priority_flag[2], IDM_SYN_THREAD_PRIORITY_NORMAL, "普通");
					AppendMenu ( hMenuSynPriority, MF_STRING | priority_flag[3], IDM_SYN_THREAD_PRIORITY_ABOVE_NORMAL, "少し高い");
					AppendMenu ( hMenuSynPriority, MF_STRING | priority_flag[4], IDM_SYN_THREAD_PRIORITY_HIGHEST, "高い");
					AppendMenu ( hMenu, MF_SEPARATOR, 0, 0 );
					AppendMenu ( hMenu, MF_POPUP, (UINT)hMenuReset, "各種システムリセット" );
					AppendMenu ( hMenu, MF_POPUP, (UINT)hMenuChange, "特定のシステムへ変更" );
					AppendMenu ( hMenu, MF_POPUP, (UINT)hMenuSynPriority, "シンセスレッドプライオリティ設定" );
					AppendMenu ( hMenu, MF_SEPARATOR, 0, 0 );
					AppendMenu ( hMenu, MF_STRING, IDM_PREFERENCE, "設定");
					AppendMenu ( hMenu, MF_STRING, IDM_CONSOLE_WND, "コンソール");
					AppendMenu ( hMenu, MF_SEPARATOR, 0, 0 );
					AppendMenu ( hMenu, MF_STRING, IDM_VERSION, "バージョン情報");
					AppendMenu ( hMenu, MF_STRING, IDM_TIMIDITY, "TiMidity について");
					AppendMenu ( hMenu, MF_SEPARATOR, 0, 0 );
					AppendMenu ( hMenu, MF_STRING, IDM_QUIT, "終了");
				} else {
					if ( w32g_syn_status == run ) {
						AppendMenu ( hMenu, MF_STRING, IDM_STOP, "Stop synthesizer");
					} else if ( w32g_syn_status == stop ) {
						AppendMenu ( hMenu, MF_STRING, IDM_START, "Start synthesizer");
					} else if ( w32g_syn_status == quit ) { 
						AppendMenu ( hMenu, MF_STRING | MF_GRAYED, IDM_START, "Quitting...");
					}
					AppendMenu ( hMenu, MF_STRING, IDM_SYSTEM_RESET, "System Reset");
					switch ( system_mode ) {
					case GM_SYSTEM_MODE:
						AppendMenu ( hMenuReset, MF_STRING | MF_CHECKED, IDM_GM_SYSTEM_RESET, "GM Reset");
						AppendMenu ( hMenuReset, MF_STRING, IDM_GS_SYSTEM_RESET, "GS Reset");
						AppendMenu ( hMenuReset, MF_STRING, IDM_XG_SYSTEM_RESET, "XG Reset");
						AppendMenu ( hMenuChange, MF_STRING | MF_CHECKED, IDM_CHANGE_GM_SYSTEM, "Change GM system");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GS_SYSTEM, "Change GS system");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_XG_SYSTEM, "Change XG system");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_DEFAULT_SYSTEM, "Change default system");
						break;
					case GS_SYSTEM_MODE:
						AppendMenu ( hMenuReset, MF_STRING, IDM_GM_SYSTEM_RESET, "GM Reset");
						AppendMenu ( hMenuReset, MF_STRING | MF_CHECKED, IDM_GS_SYSTEM_RESET, "GS Reset");
						AppendMenu ( hMenuReset, MF_STRING, IDM_XG_SYSTEM_RESET, "XG Reset");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GM_SYSTEM, "Change GM system");
						AppendMenu ( hMenuChange, MF_STRING | MF_CHECKED, IDM_CHANGE_GS_SYSTEM, "Change GS system");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_XG_SYSTEM, "Change XG system");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_DEFAULT_SYSTEM, "Change default system");
						break;
					case XG_SYSTEM_MODE:
						AppendMenu ( hMenuReset, MF_STRING, IDM_GM_SYSTEM_RESET, "GM Reset");
						AppendMenu ( hMenuReset, MF_STRING, IDM_GS_SYSTEM_RESET, "GS Reset");
						AppendMenu ( hMenuReset, MF_STRING | MF_CHECKED, IDM_XG_SYSTEM_RESET, "XG Reset");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GM_SYSTEM, "Change GM system");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GS_SYSTEM, "Change GS system");
						AppendMenu ( hMenuChange, MF_STRING | MF_CHECKED, IDM_CHANGE_XG_SYSTEM, "Change XG system");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_DEFAULT_SYSTEM, "Change default system");
						break;
					default:
					case DEFAULT_SYSTEM_MODE:
						AppendMenu ( hMenuReset, MF_STRING, IDM_GM_SYSTEM_RESET, "GM Reset");
						AppendMenu ( hMenuReset, MF_STRING, IDM_GS_SYSTEM_RESET, "GS Reset");
						AppendMenu ( hMenuReset, MF_STRING, IDM_XG_SYSTEM_RESET, "XG Reset");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GM_SYSTEM, "Change GM system");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_GS_SYSTEM, "Change GS system");
						AppendMenu ( hMenuChange, MF_STRING, IDM_CHANGE_XG_SYSTEM, "Change XG system");
						AppendMenu ( hMenuChange, MF_STRING | MF_CHECKED, IDM_CHANGE_DEFAULT_SYSTEM, "Change default system");
						break;
					}
					AppendMenu ( hMenuSynPriority, MF_STRING | priority_flag[0], IDM_SYN_THREAD_PRIORITY_LOWEST, "lowest");
					AppendMenu ( hMenuSynPriority, MF_STRING | priority_flag[1], IDM_SYN_THREAD_PRIORITY_BELOW_NORMAL, "below normal");
					AppendMenu ( hMenuSynPriority, MF_STRING | priority_flag[2], IDM_SYN_THREAD_PRIORITY_NORMAL, "normal");
					AppendMenu ( hMenuSynPriority, MF_STRING | priority_flag[3], IDM_SYN_THREAD_PRIORITY_ABOVE_NORMAL, "above normal");
					AppendMenu ( hMenuSynPriority, MF_STRING | priority_flag[4], IDM_SYN_THREAD_PRIORITY_HIGHEST, "highest");
					AppendMenu ( hMenu, MF_SEPARATOR, 0, 0 );
					AppendMenu ( hMenu, MF_POPUP, (UINT)hMenuReset, "Specific system reset" );
					AppendMenu ( hMenu, MF_POPUP, (UINT)hMenuChange, "Change Specific system" );
					AppendMenu ( hMenu, MF_POPUP, (UINT)hMenuSynPriority, "Change synthesizer thread priority" );
					AppendMenu ( hMenu, MF_SEPARATOR, 0, 0 );
					AppendMenu ( hMenu, MF_STRING, IDM_PREFERENCE, "Preference");
					AppendMenu ( hMenu, MF_STRING, IDM_CONSOLE_WND, "Console");
					AppendMenu ( hMenu, MF_SEPARATOR, 0, 0 );
					AppendMenu ( hMenu, MF_STRING, IDM_VERSION, "Version Info");
					AppendMenu ( hMenu, MF_STRING, IDM_TIMIDITY, "About TiMidity");
					AppendMenu ( hMenu, MF_SEPARATOR, 0, 0 );
					AppendMenu ( hMenu, MF_STRING, IDM_QUIT, "Quit");
				}
				GetCursorPos ( &point );
				// ポップアップメニューがきちんと消えるための操作。 
				// http://support.microsoft.com/default.aspx?scid=KB;EN-US;Q135788& 参照
#if 0		// Win 98/2000 以降用？
				{
					DWORD dwThreadID = GetWindowThreadProcessId ( hwnd, NULL );
					if ( dwThreadID != w32g_syn.gui_dwThreadId ) {
						AttachThreadInput ( w32g_syn.gui_dwThreadId, dwThreadID, TRUE );
						SetForegroundWindow ( hwnd );
						AttachThreadInput ( w32g_syn.gui_dwThreadId, dwThreadID, FALSE );
					} else {
						SetForegroundWindow ( hwnd );
					}
				}
#else	// これでいいらしい？
				SetForegroundWindow ( hwnd );
#endif
				TrackPopupMenu ( hMenu, TPM_TOPALIGN | TPM_LEFTALIGN,
					point.x, point.y, 0, hwnd, NULL );
				PostMessage ( hwnd, WM_NULL, 0, 0 );	// これもポップアップメニューのテクニックらしい。
				DestroyMenu ( hMenu );
				have_popupmenu = 0;
				return 0;
			}
    } 
	}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDM_QUIT:
#if 1/* 強制終了 */
			SetTimer ( NULL, 0, 20000, forced_exit );
#endif
			w32g_message_set (W32G_SYN_QUIT);
			break;
		case IDM_START:
			w32g_message_set (W32G_SYN_START);
			break;
		case IDM_STOP:
			w32g_message_set (W32G_SYN_STOP);
			break;
		case IDM_SYSTEM_RESET:
			w32g_message_set (W32G_SYN_SYSTEM_RESET);
			break;
		case IDM_GM_SYSTEM_RESET:
			w32g_message_set (W32G_SYN_GM_SYSTEM_RESET);
			break;
		case IDM_GS_SYSTEM_RESET:
			w32g_message_set (W32G_SYN_GS_SYSTEM_RESET);
			break;
		case IDM_XG_SYSTEM_RESET:
			w32g_message_set (W32G_SYN_XG_SYSTEM_RESET);
			break;
		case	IDM_CHANGE_GM_SYSTEM:
			w32g_message_set (W32G_SYN_CHANGE_GM_SYSTEM);
			break;
		case	IDM_CHANGE_GS_SYSTEM:
			w32g_message_set (W32G_SYN_CHANGE_GS_SYSTEM);
			break;
		case IDM_CHANGE_XG_SYSTEM:
			w32g_message_set (W32G_SYN_CHANGE_XG_SYSTEM);
			break;
		case IDM_CHANGE_DEFAULT_SYSTEM:
			w32g_message_set (W32G_SYN_CHANGE_DEFAULT_SYSTEM);
			break;
		case IDM_PREFERENCE:
			PrefWndCreate ( w32g_syn.nid_hWnd );
			break;
		case IDM_VERSION:
			VersionWnd ( w32g_syn.nid_hWnd );
			break;
		case IDM_TIMIDITY:
			TiMidityWnd ( w32g_syn.nid_hWnd );
			break;
		case IDM_SYN_THREAD_PRIORITY_LOWEST:
			syn_ThreadPriority = THREAD_PRIORITY_LOWEST;
			if ( w32g_syn_status == run ) {
				SetThreadPriority ( w32g_syn.syn_hThread, syn_ThreadPriority );
			}
			break;
		case IDM_SYN_THREAD_PRIORITY_BELOW_NORMAL:
			syn_ThreadPriority = THREAD_PRIORITY_BELOW_NORMAL;
			if ( w32g_syn_status == run ) {
				SetThreadPriority ( w32g_syn.syn_hThread, syn_ThreadPriority );
			}
			break;
		case IDM_SYN_THREAD_PRIORITY_NORMAL:
			syn_ThreadPriority = THREAD_PRIORITY_NORMAL;
			if ( w32g_syn_status == run ) {
				SetThreadPriority ( w32g_syn.syn_hThread, syn_ThreadPriority );
			}
			break;
		case IDM_SYN_THREAD_PRIORITY_ABOVE_NORMAL:
			syn_ThreadPriority = THREAD_PRIORITY_ABOVE_NORMAL;
			if ( w32g_syn_status == run ) {
				SetThreadPriority ( w32g_syn.syn_hThread, syn_ThreadPriority );
			}
			break;
		case IDM_SYN_THREAD_PRIORITY_HIGHEST:
			syn_ThreadPriority = THREAD_PRIORITY_HIGHEST;
			if ( w32g_syn_status == run ) {
				SetThreadPriority ( w32g_syn.syn_hThread, syn_ThreadPriority );
			}
			break;
#ifdef HAVE_SYN_CONSOLE
		case IDM_CONSOLE_WND:
			if ( hConsoleWnd == NULL ) {
				InitConsoleWnd ( w32g_syn.nid_hWnd );
			}
			if ( IsWindowVisible ( hConsoleWnd ) )
				ShowWindow ( hConsoleWnd, SW_HIDE );
			else
				ShowWindow ( hConsoleWnd, SW_SHOW );
			break;
#endif
		default:
			break;
		}
		break;
	default:
	  return DefWindowProc ( hwnd, uMess, wParam, lParam );
	}
	return 0L;
}

static int volatile syn_thread_started = 0;
static int start_syn_thread ( void )
{
	w32g_syn.syn_hThread = crt_beginthreadex ( NULL, 0,
				    (LPTHREAD_START_ROUTINE) syn_thread,
				    NULL, 0, & w32g_syn.syn_dwThreadId );
	if ( w32g_syn.syn_hThread == NULL ) {
		return -1;
	}
	for (;;) {
		if ( syn_thread_started == 1 )
			break;
		if ( syn_thread_started == 2 )
			return -1;
		Sleep ( 200 );
	}
	if ( syn_thread_started == 2 )
		return -1;
	return 0;
}

extern int win_main(int argc, char **argv);
static void WINAPI syn_thread ( void )
{
	syn_thread_started = 1;
	win_main ( w32g_syn.argc, w32g_syn.argv );
	syn_thread_started = 2;
}

static void terminate_syn_thread ( void )
{
	w32g_message_set ( W32G_SYN_QUIT );
}

static int wait_for_termination_of_syn_thread ( void )
{
	int i;
	int ok = 0;
	for ( i = 0; i < 10; i++ ) {
		if ( WaitForSingleObject ( w32g_syn.syn_hThread, 200 ) == WAIT_TIMEOUT ) {
			w32g_message_set ( W32G_SYN_QUIT );
		} else {
			ok = 1;
			break;
		}
	}
	return ok;
}

// 可変長引数にする予定……
// 0: 成功、1: 追加できなかった
int w32g_message_set ( int cmd )
{
	int res = 0;
	if ( msg_loopbuf_hMutex == NULL ) {
		msg_loopbuf_hMutex = CreateMutex ( NULL, TRUE, NULL );
	} else {
		WaitForSingleObject ( msg_loopbuf_hMutex, INFINITE );
	}
	if ( cmd == W32G_SYN_QUIT || cmd == W32G_SYN_START || cmd == W32G_SYN_STOP ) {	// 優先するメッセージ。
			msg_loopbuf_start = 0;
			msg_loopbuf_end = 0;
			msg_loopbuf[msg_loopbuf_end].cmd = cmd;
			ReleaseMutex ( msg_loopbuf_hMutex );
			return res;
	} else if ( cmd != W32G_SYN_NONE ) {
		if ( msg_loopbuf_end < 0 ) {
			msg_loopbuf_start = 0;
			msg_loopbuf_end = 0;
		} else if ( msg_loopbuf_start <= msg_loopbuf_end ) {
			if ( msg_loopbuf_end < W32G_SYN_MESSAGE_MAX - 1)
				msg_loopbuf_end ++;
			else
				res = 1;
		} else if ( msg_loopbuf_end < msg_loopbuf_start - 1 ) {
			msg_loopbuf_end ++;
		} else {
				res = 1;
		}
		if ( res == 0 ) {
			msg_loopbuf[msg_loopbuf_end].cmd = cmd;
		}
	}
	ReleaseMutex ( msg_loopbuf_hMutex );
	Sleep ( 100 );
	return res;
}

int w32g_message_get ( w32g_syn_message_t *msg )
{
	int have_msg = 0;
	if ( msg_loopbuf_hMutex == NULL ) {
		msg_loopbuf_hMutex = CreateMutex ( NULL, TRUE, NULL );
	} else {
		WaitForSingleObject ( msg_loopbuf_hMutex, INFINITE );
	}
	if ( msg_loopbuf_start >= 0 ) {
		memcpy ( msg, &msg_loopbuf[msg_loopbuf_start], sizeof ( w32g_syn_message_t ) );
		have_msg = 1;
		msg_loopbuf_start ++;
		if ( msg_loopbuf_end < msg_loopbuf_start ) {
			msg_loopbuf_start = msg_loopbuf_end = -1;
		} else if ( msg_loopbuf_start >= W32G_SYN_MESSAGE_MAX ) {
			msg_loopbuf_start = 0;
		}
	}
	ReleaseMutex ( msg_loopbuf_hMutex );
	return have_msg;
}

extern int seq_quit;
extern int playdone;
extern void seq_play_event(MidiEvent *);
void w32g_syn_doit(void)
{
	w32g_syn_message_t msg;
	MidiEvent ev;
	DWORD sleep_time;
	playdone=0;
	while(seq_quit==0) {
		int have_msg = 0;
		sleep_time = 0;
		have_msg = w32g_message_get ( &msg );
		if ( have_msg ) {
			switch ( msg.cmd ) {
			case W32G_SYN_QUIT:
				seq_quit=~0;
				w32g_syn_status = quit;
				sleep_time = 100;
				break;
			case W32G_SYN_START:
				seq_quit=~0;
				w32g_syn_status = run;
				sleep_time = 100;
				break;
			case W32G_SYN_STOP:
				seq_quit=~0;
				w32g_syn_status = stop;
				sleep_time = 100;
				break;
			case W32G_SYN_GM_SYSTEM_RESET:
					server_reset();
					ev.type=ME_RESET;
					ev.a=GM_SYSTEM_MODE;
					seq_play_event(&ev);
					sleep_time = 100;
				break;
			case W32G_SYN_GS_SYSTEM_RESET:
				server_reset();
				ev.type=ME_RESET;
				ev.a=GS_SYSTEM_MODE;
				seq_play_event(&ev);
				sleep_time = 100;
				break;
			case W32G_SYN_XG_SYSTEM_RESET:
				server_reset();
				ev.type=ME_RESET;
				ev.a=XG_SYSTEM_MODE;
				seq_play_event(&ev);
				sleep_time = 100;
				break;
			case W32G_SYN_SYSTEM_RESET:
				server_reset();
				ev.type=ME_RESET;
				ev.a=system_mode;
				seq_play_event(&ev);
				sleep_time = 100;
				break;
			case W32G_SYN_CHANGE_GM_SYSTEM:
				system_mode=GM_SYSTEM_MODE;
				server_reset();
				ev.type=ME_RESET;
				ev.a=GM_SYSTEM_MODE;
				seq_play_event(&ev);
				change_system_mode(system_mode);
				sleep_time = 100;
				break;
			case W32G_SYN_CHANGE_GS_SYSTEM:
				system_mode=GS_SYSTEM_MODE;
				server_reset();
				ev.type=ME_RESET;
				ev.a=GS_SYSTEM_MODE;
				seq_play_event(&ev);
				change_system_mode(system_mode);
				sleep_time = 100;
				break;
			case W32G_SYN_CHANGE_XG_SYSTEM:
				system_mode=XG_SYSTEM_MODE;
				server_reset();
				ev.type=ME_RESET;
				ev.a=XG_SYSTEM_MODE;
				seq_play_event(&ev);
				change_system_mode(system_mode);
				sleep_time = 100;
				break;
			case W32G_SYN_CHANGE_DEFAULT_SYSTEM:
				system_mode=DEFAULT_SYSTEM_MODE;
				server_reset();
				ev.type=ME_RESET;
				ev.a=GS_SYSTEM_MODE;
				seq_play_event(&ev);
				change_system_mode(system_mode);
				sleep_time = 100;
				break;
			default:
				break;
			}
		}

		winplaymidi();
		Sleep ( sleep_time );
	}
}

void w32g_syn_ctl_pass_playing_list ( int n_, char *args_[] )
{
	int i;
	w32g_syn_status = stop;
	for (;;) {
		int breakflag = 0;
		switch ( w32g_syn_status ) {
		default:
		case quit:
			breakflag = 1;
			break;
		case run:
			{
				int result;
				char args_[MAX_PORT][10];
				char *args[MAX_PORT];
				if ( w32g_syn_port_num <= 0 ) {
					w32g_syn_status = stop;
					break;
				} else if ( w32g_syn_port_num > MAX_PORT ) {
					w32g_syn_port_num = MAX_PORT;
				}
				for ( i = 0; i < MAX_PORT; i ++ ) {
					args[i] = args_[i];
					sprintf ( args[i], "%d", w32g_syn_id_port[i] );
				}
				SetThreadPriority ( w32g_syn.syn_hThread, syn_ThreadPriority );
				result = ctl_pass_playing_list2 ( w32g_syn_port_num, args );
				SetThreadPriority ( w32g_syn.syn_hThread, THREAD_PRIORITY_NORMAL );
				if ( result == 2 ) {
					w32g_syn_status = stop;
				}
			}
			break;
		case stop:
			{
			w32g_syn_message_t msg;
			if ( w32g_message_get ( &msg ) ) {
				if ( msg.cmd == W32G_SYN_START ) {
					w32g_syn_status = run;
					break;
				} else {
					if ( msg.cmd == W32G_SYN_QUIT ) {
						w32g_syn_status = quit;
						break;
					}
				}
			}
			Sleep ( 500 );
			}
			break;
		}
		if ( breakflag )
			break;
	}
	while ( w32g_syn.quit_state < 1 ) {
		PostThreadMessage ( w32g_syn.gui_dwThreadId, MYWM_QUIT, 0, 0 );
		Sleep ( 300 );
	}
	if ( w32g_syn.quit_state < 2 ) w32g_syn.quit_state = 2;
}

int w32g_syn_do_before_pref_apply ( void )
{
	w32g_syn_status_prev = none;
	for (;;) {
		if ( w32g_syn_status == quit )
			return -1;
		if ( msg_loopbuf_hMutex == NULL ) {
			msg_loopbuf_hMutex = CreateMutex ( NULL, TRUE, NULL );
		} else {
			WaitForSingleObject ( msg_loopbuf_hMutex, INFINITE );
		}
		if ( w32g_syn_status_prev == none ) 
			w32g_syn_status_prev = w32g_syn_status;
		if ( w32g_syn_status == stop ) {
			return 0;
		}
		ReleaseMutex ( msg_loopbuf_hMutex );
		w32g_message_set ( W32G_SYN_STOP );
		Sleep ( 100 );
	}
}

int w32g_syn_do_after_pref_apply ( void )
{
	ReleaseMutex ( msg_loopbuf_hMutex );
	if ( w32g_syn_status_prev == run ) {
		w32g_message_set ( W32G_SYN_START );
		Sleep ( 100 );
	}
	return 0;
}

#ifdef HAVE_SYN_CONSOLE

// ****************************************************************************
// Edit Ctl.

void VprintfEditCtlWnd(HWND hwnd, char *fmt, va_list argList)
{
	 char buffer[BUFSIZ], out[BUFSIZ];
	 char *in;
	 int i;

	 if(!IsWindow(hwnd))
		  return;

	 vsnprintf(buffer, sizeof(buffer), fmt, argList);
	 in = buffer;
	 i = 0;
	 for(;;){
		  if(*in == '\0' || i>sizeof(out)-3){
				out[i] = '\0';
				break;
		  }
		  if(*in=='\n'){
				out[i] = 13;
				out[i+1] = 10;
				in++;
				i += 2;
				continue;
		  }
		  out[i] = *in;
		  in++;
		  i++;
	 }
	 Edit_SetSel(hwnd,-1,-1);
	 Edit_ReplaceSel(hwnd,out);
}

void PrintfEditCtlWnd(HWND hwnd, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    VprintfEditCtlWnd(hwnd,fmt,ap);
    va_end(ap);
}

#if 1
void PutsEditCtlWnd(HWND hwnd, char *str)
{
	char *in = str;
	int i;
	char out[BUFSIZ];
	i = 0;
	for(;;){
		if(*in == '\0' || i>sizeof(out)-3){
			out[i] = '\0';
			break;
    }
  	if(*in=='\n'){
    	out[i] = 13;
    	out[i+1] = 10;
			in++;
      i += 2;
      continue;
    }
    out[i] = *in;
		in++;
    i++;
  }
	if(IsWindow(hwnd)){
		Edit_SetSel(hwnd,-1,-1);
		Edit_ReplaceSel(hwnd,out);
	}
}
#else
void PutsEditCtlWnd(HWND hwnd, char *str)
{
	if(!IsWindow(hwnd))
		return;
	PrintfEditCtlWnd(hwnd,"%s",str);
}
#endif

void ClearEditCtlWnd(HWND hwnd)
{
	char pszVoid[]="";
	if(!IsWindow(hwnd))
		return;
	if(IsWindow(hwnd)){
//		Edit_SetSel(hwnd,0,-1);
		Edit_SetSel(hwnd,-1,-1);
	}
	Edit_SetText(hwnd,pszVoid);
}

static void VersionWnd(HWND hParentWnd)
{
	char VersionText[2024];
  sprintf(VersionText,
"TiMidity++ version %s" NLS NLS
"TiMidity-0.2i by Tuukka Toivonen <tt@cgs.fi>." NLS
"TiMidity Win32 version by Davide Moretti <dave@rimini.com>." NLS
"TiMidity Windows 95 port by Nicolas Witczak." NLS
"Twsynth by Keishi Suenaga <s_keishi@mutt.freemail.ne.jp>." NLS
"Twsynth GUI by Daisuke Aoki <dai@y7.net>." NLS
" Japanese menu, dialog, etc by Saito <timidity@flashmail.com>." NLS
"TiMidity++ by Masanao Izumo <mo@goice.co.jp>." NLS
,timidity_version);
	MessageBox(hParentWnd, VersionText, "Version", MB_OK);
}

static void TiMidityWnd(HWND hParentWnd)
{
	char TiMidityText[2024];
  sprintf(TiMidityText,
" TiMidity++ version %s -- MIDI to WAVE converter and player" NLS
" Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>" NLS
" Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>" NLS
NLS
" Win32 version by Davide Moretti <dmoretti@iper.net>" NLS
" GUI by Daisuke Aoki <dai@y7.net>." NLS
" Modified by Masanao Izumo <mo@goice.co.jp>." NLS
NLS
" This program is free software; you can redistribute it and/or modify" NLS
" it under the terms of the GNU General Public License as published by" NLS
" the Free Software Foundation; either version 2 of the License, or" NLS
" (at your option) any later version." NLS
NLS
" This program is distributed in the hope that it will be useful," NLS
" but WITHOUT ANY WARRANTY; without even the implied warranty of"NLS
" MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" NLS
" GNU General Public License for more details." NLS
NLS
" You should have received a copy of the GNU General Public License" NLS
" along with this program; if not, write to the Free Software" NLS
" Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA" NLS
,
timidity_version
	);
	MessageBox(hParentWnd, TiMidityText, "TiMidity", MB_OK);
}


// ***************************************************************************
//
// Console Window
//
// ***************************************************************************

// ---------------------------------------------------------------------------
// variables
static int ConsoleWndMaxSize = 64 * 1024;
static HFONT hFontConsoleWnd = NULL;

// ---------------------------------------------------------------------------
// prototypes of functions
static BOOL CALLBACK ConsoleWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
static void ConsoleWndAllUpdate(void);
static void ConsoleWndVerbosityUpdate(void);
static void ConsoleWndVerbosityApply(void);
static void ConsoleWndValidUpdate(void);
static void ConsoleWndValidApply(void);
static void ConsoleWndVerbosityApplyIncDec(int num);
static int ConsoleWndInfoReset(HWND hwnd);
static int ConsoleWndInfoApply(void);

void ClearConsoleWnd(void);

// ---------------------------------------------------------------------------
// Global Functions

// Initialization
void InitConsoleWnd(HWND hParentWnd)
{
	if (hConsoleWnd != NULL)
		DestroyWindow(hConsoleWnd);
	switch(PlayerLanguage){
  	case LANGUAGE_ENGLISH:
		hConsoleWnd = CreateDialog
  			(hInst,MAKEINTRESOURCE(IDD_DIALOG_CONSOLE_EN),hParentWnd,ConsoleWndProc);
		break;
 	default:
	case LANGUAGE_JAPANESE:
		hConsoleWnd = CreateDialog
  			(hInst,MAKEINTRESOURCE(IDD_DIALOG_CONSOLE),hParentWnd,ConsoleWndProc);
	break;
	}
	ShowWindow(hConsoleWnd,SW_HIDE);
	UpdateWindow(hConsoleWnd);

	ConsoleWndVerbosityApplyIncDec(0);
	CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, ConsoleWndFlag);
	Edit_LimitText(GetDlgItem(hConsoleWnd,IDC_EDIT), ConsoleWndMaxSize);
}

// Window Procedure
static BOOL CALLBACK
ConsoleWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
		PutsConsoleWnd("Console Window\n");
		ConsoleWndAllUpdate();
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			ShowWindow(hwnd, SW_HIDE);
			break;
		case IDCLEAR:
			ClearConsoleWnd();
			break;
		case IDC_CHECKBOX_VALID:
			ConsoleWndValidApply();
			break;
		case IDC_BUTTON_VERBOSITY:
			ConsoleWndVerbosityApply();
			break;
		case IDC_BUTTON_INC:
			ConsoleWndVerbosityApplyIncDec(1);
			break;
		case IDC_BUTTON_DEC:
			ConsoleWndVerbosityApplyIncDec(-1);
			break;
		default:
			break;
		}
		switch (HIWORD(wParam)) {
		case EN_ERRSPACE:
			ClearConsoleWnd();
			PutsConsoleWnd("### EN_ERRSPACE -> Clear! ###\n");
			break;
		default:
			break;
		}
		break;
	case WM_SIZE:
		ConsoleWndAllUpdate();
		return FALSE;
	case WM_MOVE:
		break;
	// See PreDispatchMessage() in w32g2_main.c
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		int nVirtKey = (int)wParam;
		switch(nVirtKey){
			case VK_ESCAPE:
				SendMessage(hwnd,WM_CLOSE,0,0);
				break;
		}
	}
		break;
	case WM_DESTROY:
		break;
	case WM_CLOSE:
		ShowWindow(hConsoleWnd, SW_HIDE);
		break;
	case WM_SETFOCUS:
		HideCaret(hwnd);
		break;
	case WM_KILLFOCUS:
		ShowCaret(hwnd);
		break;
	default:
		return FALSE;
	}
	return FALSE;
}

// puts()
void PutsConsoleWnd(char *str)
{
	HWND hwnd;
	if(!IsWindow(hConsoleWnd) || !ConsoleWndFlag)
		return;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT);
	PutsEditCtlWnd(hwnd,str);
}

// printf()
void PrintfConsoleWnd(char *fmt, ...)
{
	HWND hwnd;
	va_list ap;
	if(!IsWindow(hConsoleWnd) || !ConsoleWndFlag)
		return;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT);
	va_start(ap, fmt);
	VprintfEditCtlWnd(hwnd,fmt,ap);
	va_end(ap);
}

// Clear
void ClearConsoleWnd(void)
{
	HWND hwnd;
	if(!IsWindow(hConsoleWnd))
		return;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT);
	ClearEditCtlWnd(hwnd);
}

// ---------------------------------------------------------------------------
// Static Functions

static void ConsoleWndAllUpdate(void)
{
	ConsoleWndVerbosityUpdate();
	ConsoleWndValidUpdate();
	Edit_LimitText(GetDlgItem(hConsoleWnd,IDC_EDIT_VERBOSITY),3);
	Edit_LimitText(GetDlgItem(hConsoleWnd,IDC_EDIT),ConsoleWndMaxSize);
}

static void ConsoleWndValidUpdate(void)
{
	if(ConsoleWndFlag)
		CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, 1);
	else
		CheckDlgButton(hConsoleWnd, IDC_CHECKBOX_VALID, 0);
}

static void ConsoleWndValidApply(void)
{
	if(IsDlgButtonChecked(hConsoleWnd,IDC_CHECKBOX_VALID))
		ConsoleWndFlag = 1;
	else
		ConsoleWndFlag = 0;
}

static void ConsoleWndVerbosityUpdate(void)
{
	SetDlgItemInt(hConsoleWnd,IDC_EDIT_VERBOSITY,(UINT)ctl->verbosity, TRUE);
}

static void ConsoleWndVerbosityApply(void)
{
	char buffer[64];
	HWND hwnd;
	hwnd = GetDlgItem(hConsoleWnd,IDC_EDIT_VERBOSITY);
	if(!IsWindow(hConsoleWnd)) return;
	if(Edit_GetText(hwnd,buffer,60)<=0) return;
	ctl->verbosity = atoi(buffer);
	ConsoleWndVerbosityUpdate();
}

static void ConsoleWndVerbosityApplyIncDec(int num)
{
	if(!IsWindow(hConsoleWnd)) return;
	ctl->verbosity += num;
	RANGE(ctl->verbosity, -1, 4);
	ConsoleWndVerbosityUpdate();
}

#endif

#endif // IA_W32G_SYN
