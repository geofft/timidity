/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    w32g_subwin3.c: Written by Daisuke Aoki <dai@y7.net>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <stddef.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "tables.h"
#include "miditrace.h"
#include "reverb.h"
#ifdef SUPPORT_SOUNDSPEC
#include "soundspec.h"
#endif /* SUPPORT_SOUNDSPEC */
#include "recache.h"
#include "arc.h"
#include "strtab.h"
#include "mid.defs"

#include "w32g.h"
#include <shlobj.h>
#include <commctrl.h>
#include <windowsx.h>
#include "w32g_res.h"
#include "w32g_utl.h"
#include "w32g_pref.h"
#include "w32g_subwin.h"
#include "w32g_ut2.h"

#include "w32g_tracer.h"

extern int gdi_lock_ex ( DWORD timeout );
#if 0
static int gdi_lock_result;
#define GDI_LOCK_EX(timeout) ( \
	ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "GDI_LOCK_EX(%s: %d)", __FILE__, __LINE__ ), \
	gdi_lock_result = gdi_lock_ex(timeout), \
	ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "GDI_LOCK_EX_RESULT(%d)", gdi_lock_result ), \
	gdi_lock_result \
)
#define GDI_LOCK() { \
	ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "GDI_LOCK(%s: %d)", __FILE__, __LINE__ ); \
	gdi_lock(); \
}
#define GDI_UNLOCK() { \
	ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "GDI_UNLOCK(%s: %d)", __FILE__, __LINE__ ); \
	gdi_unlock(); \
}
#else
#define GDI_LOCK() { gdi_lock(); }
#define GDI_LOCK_EX(timeout) gdi_lock_ex(timeout)
#define GDI_UNLOCK() { gdi_unlock(); }
#endif

#if defined(__CYGWIN32__) || defined(__MINGW32__)
#ifndef TPM_TOPALIGN
#define TPM_TOPALIGN	0x0000L
#endif
#endif

extern void VprintfEditCtlWnd(HWND hwnd, char *fmt, va_list argList);
extern void PrintfEditCtlWnd(HWND hwnd, char *fmt, ...);
extern void PutsEditCtlWnd(HWND hwnd, char *str);
extern void ClearEditCtlWnd(HWND hwnd);

#define C_BACK RGB(0x00,0x00,0x00)
// #define C_BAR_FORE RGB(0xFF,0xFF,0x00)
#define C_BAR_FORE RGB(0xD0,0xD0,0x00)
#define C_BAR_FORE2 RGB(0xD0,0xB0,0x00)
#define C_BAR_BACK RGB(0xA0,0xA0,0xA0)
#define C_BAR_LEFT RGB(0xD0,0xD0,0x00)
#define C_BAR_RIGHT RGB(0x80,0xD0,0x40)
#define C_TEXT_FORE RGB(0x00,0x00,0x00)
#define C_TEXT_BACK RGB(0xC0,0xC0,0xC0)
#define C_TEXT_BACK_DARK RGB(0x80,0x80,0x80)
#define C_TEXT_BACK_VERY_DARK RGB(0x40,0x40,0x40)

#define CVV_TYPE_NONE 0
#define CVV_TYPE_LEFT 1		// 左から右
#define CVV_TYPE_RIGHT 2	// 右から左
#define CVV_TYPE_TOP 3		// 上から下
#define CVV_TYPE_BOTTOM 4	// 下から上

#define VEL_MAX 128*4

#define TWI_MODE_1_32CH		1
#define TWI_MODE_1_16CH		2
#define TWI_MODE_17_32CH	3

extern ControlMode *ctl;

#define CSV_LEFT	0
#define CSV_RIGHT	1
#define CSV_CENTER 2

static struct tracer_bmp_ {
	HBITMAP hbmp;
	HDC hmdc;
	RECT rc_volume;
	RECT rc_expression;
	RECT rc_pan;
	RECT rc_sustain;
	RECT rc_pitch_bend;
	RECT rc_mod_wheel;
	RECT rc_chorus_effect;
	RECT rc_reverb_effect;
	RECT rc_velocity[2];
	RECT rc_notes;
	RECT rc_gm_on;
	RECT rc_gm_off;
	RECT rc_gs_on;
	RECT rc_gs_off;
	RECT rc_xg_on;
	RECT rc_xg_off;
	int volume_max;
	int expression_max;
	int pan_max;
	int sustain_max;
	int pitch_bend_max;
	int mod_wheel_max;
	int chorus_effect_max;
	int reverb_effect_max;
	int velocity_max;
	int notes_max;
	int gm_max;
	int gs_max;
	int xg_max;
} tracer_bmp;

static int get_head_rc ( RECT *rc, RECT *rc_base );
static int get_ch_rc ( int ch, RECT *rc, RECT *rc_base );
static int cheap_notes_view_draw ( RECT *lprc, int note, int vel, int back_draw );
static int cheap_volume_view_draw ( RECT *lprc, int vol, int max, COLORREF fore, COLORREF back, int type );
static int cheap_string_view_draw ( RECT *lprc, char *str, COLORREF fore, COLORREF back, int mode );
static int cheap_half_string_view_draw ( RECT *lprc, char *str, COLORREF fore, COLORREF back, int mode );

static int change_tracer_wnd_mode ( int mode );

static int init_tracer_bmp ( HDC hdc );

static int tracer_ch_program_draw ( int ch, int bank, int program, char *instrument, int mapID );
static int tracer_velocity_draw ( RECT *lprc, int vol, int max );
static int tracer_volume_draw ( RECT *lprc, int vol, int max );
static int tracer_expression_draw ( RECT *lprc, int vol, int max );
static int tracer_pan_draw ( RECT *lprc, int vol, int max );
static int tracer_sustain_draw ( RECT *lprc, int vol );
static int tracer_pitch_bend_draw ( RECT *lprc, int vol, int max );
static int tracer_mod_wheel_draw ( RECT *lprc, int vol, int max );
static int tracer_chorus_effect_draw ( RECT *lprc, int vol, int max );
static int tracer_reverb_effect_draw ( RECT *lprc, int vol, int max );
static int tracer_gm_draw ( RECT *lprc, int flag );
static int tracer_gs_draw ( RECT *lprc, int flag );
static int tracer_xg_draw ( RECT *lprc, int flag );

// ****************************************************************************
// Tracer Window

TRACERWNDINFO TracerWndInfo;

static int TracerWndInfoReset(HWND hwnd);
static int TracerWndInfoApply(void);

w32g_tracer_wnd_t w32g_tracer_wnd;

BOOL SetTracerWndActive(void)
{
	if ( IsWindowVisible(hTracerWnd) ) {
		w32g_tracer_wnd.active = TRUE;
	} else {
		w32g_tracer_wnd.active = FALSE;
	}
	return w32g_tracer_wnd.active;
}

BOOL CALLBACK TracerWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK TracerCanvasWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void InitTracerWnd(HWND hParentWnd)
{
	WNDCLASS wndclass ;
	int height, space;
	RECT rc, rc2;
	HICON hIcon;

	if (hTracerWnd != NULL) {
		DestroyWindow(hTracerWnd);
		DeleteObject ( (HGDIOBJ)w32g_tracer_wnd.hFontCommon );
		DeleteObject ( (HGDIOBJ)w32g_tracer_wnd.hFontHalf );
		DeleteDC ( w32g_tracer_wnd.hmdc );
		DeleteObject ( w32g_tracer_wnd.hNullBrush );
		DeleteObject ( w32g_tracer_wnd.hNullPen );
	}

	w32g_tracer_wnd.active = FALSE;
	w32g_tracer_wnd.hNullBrush = GetStockObject ( NULL_BRUSH );
	w32g_tracer_wnd.hNullPen = GetStockObject ( NULL_PEN );
	INILoadTracerWnd();
	hTracerWnd = CreateDialog
		(hInst,MAKEINTRESOURCE(IDD_DIALOG_TRACER),hParentWnd,TracerWndProc);
	TracerWndInfoReset(hTracerWnd);
	ShowWindow(hTracerWnd,SW_HIDE);
	hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON_TIMIDITY));
	if (hIcon!=NULL) SendMessage(hTracerWnd,WM_SETICON,FALSE,(LPARAM)hIcon);
	INILoadTracerWnd();

	w32g_tracer_wnd.font_common_height = 14; 
	w32g_tracer_wnd.font_common_width = 0;
	SetRect ( &w32g_tracer_wnd.rc_head, 1, 2,  0, 0 );
	SetRect ( &w32g_tracer_wnd.rc_all_channels, 1,  20 + 2,  0, 0 );
	w32g_tracer_wnd.width = 2 + 860 + 2; 
	w32g_tracer_wnd.height = 1 + 19 + 1 + (19 + 1) * 32 + 1; 

	w32g_tracer_wnd.ch_height = 19;
	w32g_tracer_wnd.ch_space = 1;
	height = w32g_tracer_wnd.ch_height;
	space = w32g_tracer_wnd.ch_space;

#if 0
	SetRect ( &w32g_tracer_wnd.rc_current_time, 2, 1, 80, height );
	SetRect ( &w32g_tracer_wnd.rc_tempo, 82, 1, 160, height );
	SetRect ( &w32g_tracer_wnd.rc_master_volume, 162, 1, 200, height / 2 - 1 );
	SetRect ( &w32g_tracer_wnd.rc_maxvoices, 162, height / 2 + 1, 200, height );
#endif

	wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_CLASSDC;
	wndclass.lpfnWndProc   = TracerCanvasWndProc ;
	wndclass.cbClsExtra    = 0 ;
	wndclass.cbWndExtra    = 0 ;
	wndclass.hInstance     = hInst ;
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = LoadCursor(0,IDC_ARROW) ;
	wndclass.hbrBackground = w32g_tracer_wnd.hNullBrush;
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = "tracer canvas wnd";
	RegisterClass(&wndclass);
  	w32g_tracer_wnd.hwnd = CreateWindowEx(0,"tracer canvas wnd",0,WS_CHILD,
		CW_USEDEFAULT,0,w32g_tracer_wnd.width,w32g_tracer_wnd.height,
		hTracerWnd,0,hInst,0);

	w32g_tracer_wnd.hdc = GetDC(w32g_tracer_wnd.hwnd);

	SelectObject ( w32g_tracer_wnd.hdc, w32g_tracer_wnd.hNullBrush );
	SelectObject ( w32g_tracer_wnd.hdc, w32g_tracer_wnd.hNullPen );

	w32g_tracer_wnd.hbitmap = CreateCompatibleBitmap(w32g_tracer_wnd.hdc,w32g_tracer_wnd.width,w32g_tracer_wnd.height);
	w32g_tracer_wnd.hmdc = CreateCompatibleDC(w32g_tracer_wnd.hdc);
	w32g_tracer_wnd.hgdiobj_hmdcprev = SelectObject(w32g_tracer_wnd.hmdc,w32g_tracer_wnd.hbitmap);
	SelectObject ( w32g_tracer_wnd.hmdc, w32g_tracer_wnd.hNullBrush );
	SelectObject ( w32g_tracer_wnd.hmdc, w32g_tracer_wnd.hNullPen );
	init_tracer_bmp ( w32g_tracer_wnd.hdc );

	ReleaseDC(w32g_tracer_wnd.hwnd,w32g_tracer_wnd.hdc);

	{
		char fontname[128];
		if ( PlayerLanguage == LANGUAGE_JAPANESE )
			strcpy(fontname,"ＭＳ Ｐ明朝");
		else {
//			strcpy(fontname,"Times New Roman");
			strcpy(fontname,"Arial");
			w32g_tracer_wnd.font_common_height = 16; 
		}
		w32g_tracer_wnd.hFontCommon = CreateFont(w32g_tracer_wnd.font_common_height,w32g_tracer_wnd.font_common_width,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
	      	DEFAULT_PITCH | FF_MODERN ,fontname);
		w32g_tracer_wnd.hFontHalf = CreateFont(-10,0,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
	      	DEFAULT_PITCH | FF_MODERN ,"Courier");
	}
	TracerWndReset();

	SetRect ( &w32g_tracer_wnd.rc_channel_top, 1, 0, 20, height );
	SetRect ( &w32g_tracer_wnd.rc_instrument, 21, 0, 179, height );	
	SetRect ( &w32g_tracer_wnd.rc_inst_map, 180, 0, 233, height );	
	SetRect ( &w32g_tracer_wnd.rc_bank, 234, 0, 264, height );
	SetRect ( &w32g_tracer_wnd.rc_program, 265, 0, 295, height );
	SetRect ( &w32g_tracer_wnd.rc_velocity, 296, 0, 326, height );
	SetRect ( &w32g_tracer_wnd.rc_volume, 327, 0, 347, height / 2 - 0 );
	SetRect ( &w32g_tracer_wnd.rc_expression, 327, height / 2 + 1, 347, height );
	SetRect ( &w32g_tracer_wnd.rc_panning, 348, 0, 368, height / 2 - 0 );
	SetRect ( &w32g_tracer_wnd.rc_sustain, 348, height / 2 + 1, 368, height );
	SetRect ( &w32g_tracer_wnd.rc_pitch_bend, 369, 0, 389, height / 2 - 0 );
	SetRect ( &w32g_tracer_wnd.rc_mod_wheel, 369, height / 2 + 1, 389, height );
	SetRect ( &w32g_tracer_wnd.rc_chorus_effect, 390, 0, 410, height / 2 - 0 );
	SetRect ( &w32g_tracer_wnd.rc_reverb_effect, 390, height / 2 + 1, 410, height );
	SetRect ( &w32g_tracer_wnd.rc_notes, 411, 0, 860 + 10, height + 0 );
	SetRect ( &w32g_tracer_wnd.rc_gm, 411, 0, 411 + tracer_bmp.rc_gm_on.right - tracer_bmp.rc_gm_on.left, height );
	SetRect ( &w32g_tracer_wnd.rc_gs, w32g_tracer_wnd.rc_gm.right + 1, 0, w32g_tracer_wnd.rc_gm.right + 1 +  tracer_bmp.rc_gs_on.right - tracer_bmp.rc_gs_on.left, height );
	SetRect ( &w32g_tracer_wnd.rc_xg, w32g_tracer_wnd.rc_gs.right + 1, 0, w32g_tracer_wnd.rc_gs.right + 1 +  tracer_bmp.rc_xg_on.right - tracer_bmp.rc_xg_on.left, height );
	SetRect ( &w32g_tracer_wnd.rc_head_rest, w32g_tracer_wnd.rc_xg.right + 1, 0, 860 + 10, height );

	GetWindowRect ( hTracerWnd, &rc );
	GetClientRect ( hTracerWnd, &rc2 );
	rc.left = rc.left;
	rc.top = rc.top;
	rc.right = (rc.right - rc.left) - (rc2.right - rc2.left) + w32g_tracer_wnd.width;
	rc.bottom = (rc.bottom - rc.top) - (rc2.bottom - rc2.top) + w32g_tracer_wnd.height;
	MoveWindow ( hTracerWnd, rc.left, rc.top, rc.right, rc.bottom, TRUE);
	MoveWindow(w32g_tracer_wnd.hwnd,0,0,w32g_tracer_wnd.width,w32g_tracer_wnd.height,TRUE);
	ShowWindow(w32g_tracer_wnd.hwnd,SW_SHOW);
	UpdateWindow(w32g_tracer_wnd.hwnd);
	UpdateWindow(hTracerWnd);

	INILoadTracerWnd();
	TracerWndInfoApply();
}

static int init_tracer_bmp ( HDC hdc )
{
	static int init = 1;
	if ( init ) {
		tracer_bmp.hbmp = NULL;
		tracer_bmp.hmdc = NULL;
		init = 0;
	}
	if ( tracer_bmp.hmdc != NULL )
		DeleteDC ( tracer_bmp.hmdc );
	if ( tracer_bmp.hbmp != NULL )
		DeleteObject ( (HGDIOBJ) tracer_bmp.hbmp );
	tracer_bmp.hbmp = LoadBitmap ( hInst, MAKEINTRESOURCE(IDB_BITMAP_TRACER) );
	tracer_bmp.hmdc = CreateCompatibleDC ( hdc );
	SelectObject ( tracer_bmp.hmdc, tracer_bmp.hbmp );

	SetRect ( &tracer_bmp.rc_volume, 8, 16, 28, 195 );
	SetRect ( &tracer_bmp.rc_expression, 32, 16, 52, 195 );
	SetRect ( &tracer_bmp.rc_pan, 56, 16, 76, 195 );
	SetRect ( &tracer_bmp.rc_sustain, 80, 16, 100, 195 );
	SetRect ( &tracer_bmp.rc_pitch_bend, 104, 16, 124, 195 );
	SetRect ( &tracer_bmp.rc_mod_wheel, 128, 16, 148, 195 );
	SetRect ( &tracer_bmp.rc_chorus_effect, 152, 16, 172, 195 );
	SetRect ( &tracer_bmp.rc_reverb_effect, 176, 16, 196, 195 );
	SetRect ( &tracer_bmp.rc_velocity[0], 200, 16, 230, 215 );
	SetRect ( &tracer_bmp.rc_velocity[1], 231, 16, 261, 215 );
	SetRect ( &tracer_bmp.rc_notes, 16, 224, 58, 243 );
#if 0
	SetRect ( &tracer_bmp.rc_gm_on, 64, 224, 88, 243 );
	SetRect ( &tracer_bmp.rc_gm_off, 64, 248, 88, 267 );
	SetRect ( &tracer_bmp.rc_gs_on, 96, 224, 122, 243 );
	SetRect ( &tracer_bmp.rc_gs_off, 96, 248, 122, 267 );
	SetRect ( &tracer_bmp.rc_xg_on, 128, 224, 160, 243 );
	SetRect ( &tracer_bmp.rc_xg_off, 128, 248, 160, 267 );
#else
	SetRect ( &tracer_bmp.rc_gm_on, 64, 224, 88, 243 );
	SetRect ( &tracer_bmp.rc_gm_off, 64, 248, 88, 267 );
	SetRect ( &tracer_bmp.rc_gs_on, 96, 224, 120, 243 );
	SetRect ( &tracer_bmp.rc_gs_off, 96, 248, 120, 267 );
	SetRect ( &tracer_bmp.rc_xg_on, 128, 224, 152, 243 );
	SetRect ( &tracer_bmp.rc_xg_off, 128, 248, 152, 267 );
#endif

	tracer_bmp.volume_max = 20;
	tracer_bmp.expression_max = 20;
	tracer_bmp.pan_max = 20;
	tracer_bmp.sustain_max = 20;
	tracer_bmp.pitch_bend_max = 20;
	tracer_bmp.mod_wheel_max = 20;
	tracer_bmp.chorus_effect_max = 20;
	tracer_bmp.reverb_effect_max = 20;
	tracer_bmp.velocity_max = 20;
	tracer_bmp.notes_max = 20;
	
	return 0;
}

void TracerWndReset(void)
{
	int i, j;
	strcpy ( w32g_tracer_wnd.titlename, "" );
	strcpy ( w32g_tracer_wnd.filename, "" );
#if 0
	sprintf ( w32g_tracer_wnd.current_time, "00:00:00" );
	w32g_tracer_wnd.current_time_sec = 0;
	w32g_tracer_wnd.tempo = 0;
	w32g_tracer_wnd.master_volume = 0;
	w32g_tracer_wnd.maxvoices = 0;
#endif
	for ( i = 0; i < TRACER_CHANNELS; i ++ ) {
		strcpy ( w32g_tracer_wnd.instrument[i],"  ----" );
		w32g_tracer_wnd.mapID[i] = INST_NO_MAP;
		w32g_tracer_wnd.bank[i] = 0;
		w32g_tracer_wnd.program[i] = 0;
		w32g_tracer_wnd.velocity[i] = 0;
		w32g_tracer_wnd.volume[i] = 0;
		w32g_tracer_wnd.expression[i] = 0;
		w32g_tracer_wnd.panning[i] = 64;
		w32g_tracer_wnd.sustain[i] = 0;
		w32g_tracer_wnd.pitch_bend[i] = 0x2000;
		w32g_tracer_wnd.mod_wheel[i] = 0;
		w32g_tracer_wnd.chorus_effect[i] = 0;
		w32g_tracer_wnd.reverb_effect[i] = 0;
		for ( j = 0; j < 256; j ++ ) {
			w32g_tracer_wnd.notes[i][j] = -1;
		}
	}
	w32g_tracer_wnd.play_system_mode = play_system_mode;
	w32g_tracer_wnd.quietchannels = quietchannels;
	w32g_tracer_wnd.channel_mute = channel_mute;

	InvalidateRect(w32g_tracer_wnd.hwnd,NULL, FALSE);
}

void TracerWndReset2(void)
{
	int i, j;
	for ( i = 0; i < TRACER_CHANNELS; i ++ ) {
		w32g_tracer_wnd.velocity[i] = 0;
		for ( j = 0; j < 128; j ++ ) {
			w32g_tracer_wnd.notes[i][j] = -1;
		}
	}
	for ( i = 0; i < TRACER_CHANNELS; i ++ ) {
		for ( j = 0; j < 128; j ++ ) {
			RECT rc;
			if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_notes ) == 0 )
				cheap_notes_view_draw ( &rc, j, w32g_tracer_wnd.notes[i][j], TRUE );
		}
	}
}


// 画面消去
void TracerWndClear(int lockflag)
{
	HPEN hPen;
	HBRUSH hBrush;
	HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;
	RECT rc;

	if ( lockflag ) GDI_LOCK();
	hPen = CreatePen(PS_SOLID,1,C_BACK);
	hBrush = CreateSolidBrush(C_BACK);
	hgdiobj_hpen = SelectObject(w32g_tracer_wnd.hmdc, hPen);
	hgdiobj_hbrush = SelectObject(w32g_tracer_wnd.hmdc, hBrush);
	GetClientRect(w32g_tracer_wnd.hwnd,&rc);
	Rectangle(w32g_tracer_wnd.hmdc,rc.left,rc.top,rc.right,rc.bottom);
	InvalidateRect( w32g_tracer_wnd.hwnd, NULL, FALSE );
	SelectObject(w32g_tracer_wnd.hmdc, hgdiobj_hpen);
	DeleteObject(hPen);
	SelectObject(w32g_tracer_wnd.hmdc, hgdiobj_hbrush);
	DeleteObject(hBrush);
	if ( lockflag ) GDI_UNLOCK();

}

#define TRACER_VOICE_OFF -1
#define TRACER_VOICE_SUSTAINED -2

void w32_tracer_ctl_event(CtlEvent *e)
{
	RECT rc;
	int v1, v2;
    switch ( e->type ) {
	case CTLE_NOW_LOADING:
	{
		char * title;
		TracerWndReset ();
		TracerWndPaintAll (TRUE);
		strncpy ( w32g_tracer_wnd.filename, (char *)e->v1, 1000 );
		w32g_tracer_wnd.filename[1000] = '\0';
		title = get_midi_title(w32g_tracer_wnd.filename);
		if ( title == NULL ) title = w32g_tracer_wnd.filename;
		strcpy ( w32g_tracer_wnd.titlename, "  " );
		strncat ( w32g_tracer_wnd.titlename, title, 1000 );
		w32g_tracer_wnd.titlename[1000] = '\0';
		get_head_rc ( &rc, &w32g_tracer_wnd.rc_head_rest );
		cheap_string_view_draw ( &rc, w32g_tracer_wnd.titlename, C_TEXT_FORE, C_TEXT_BACK, CSV_LEFT );
	}
		break;
	case CTLE_LOADING_DONE:
		break;
	case CTLE_PLAY_START:
#if 0
		{
		int i;
		for ( i = 0; i < TRACER_CHANNELS; i++ ) {
//			tracer_ch_program_draw ( i, -1, -1, (char *)-1, -1 );
			w32g_tracer_wnd.volume[i] = channel[i].volume;
			w32g_tracer_wnd.expression[i] = channel[i].expression;
			w32g_tracer_wnd.panning[i] = channel[i].panning;
			w32g_tracer_wnd.sustain[i] = channel[i].sustain;
			w32g_tracer_wnd.pitch_bend[i] = channel[i].pitchbend;
			w32g_tracer_wnd.mod_wheel[i] = channel[i].modulation_wheel;
			w32g_tracer_wnd.chorus_effect[i] = channel[i].chorus_level;
			w32g_tracer_wnd.reverb_effect[i] = channel[i].reverb_level;
		}
#endif
		break;
	case CTLE_PLAY_END:
		break;
	case CTLE_TEMPO:
#if 0
		{
		char buff[64];
		w32g_tracer_wnd.tempo = (int)e->v1;
		sprintf ( buff, "%08ld", w32g_tracer_wnd.current_time);
		cheap_string_view_draw ( &w32g_tracer_wnd.rc_tempo, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
		}
#endif
		break;
	case CTLE_METRONOME:
		break;
	case CTLE_CURRENT_TIME:
#if 0
		{
		int sec, min, hour;
		if(midi_trace.flush_flag)
			return;
		if(ctl->trace_playing)
			sec = (int)e->v1;
		else {
			sec = current_trace_samples();
			if(sec < 0)
				sec = (int)e->v1;
			else
				sec = sec / play_mode->rate;
		}
		min = sec / 60; sec %= 60;
		hour = min / 60; min %= 60;
		sprintf ( w32g_tracer_wnd.current_time, "%02d:%02d:%02d", hour, min, sec );
		w32g_tracer_wnd.current_time_sec = sec;
		cheap_string_view_draw ( &w32g_tracer_wnd.rc_current_time, w32g_tracer_wnd.current_time, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
		cheap_volume_view_draw ( &w32g_tracer_wnd.rc_maxvoices, (int)e->v2, voices, C_BAR_FORE, C_BAR_BACK, CVV_TYPE_LEFT  );
	}
#endif
		break;
	case CTLE_NOTE:
		{
		int vel;
		switch ( (int)e->v1 ) {
		case VOICE_ON:
			w32g_tracer_wnd.velocity[(int)e->v2] += (int)e->v4;
			break;
		case VOICE_SUSTAINED:
			vel = TRACER_VOICE_SUSTAINED;
			break;
		case VOICE_DIE:
		case VOICE_FREE:
		case VOICE_OFF:
			w32g_tracer_wnd.velocity[(int)e->v2] -= (int)e->v4;
			vel = TRACER_VOICE_OFF;
			break;
		}
		if ( w32g_tracer_wnd.velocity[(int)e->v2]  < 0 )
			w32g_tracer_wnd.velocity[(int)e->v2]  = 0;
		if ( get_ch_rc ( (int)e->v2, &rc, &w32g_tracer_wnd.rc_notes ) == 0 )
			cheap_notes_view_draw ( &rc, (int)e->v3, vel, FALSE );
		w32g_tracer_wnd.notes[(int)e->v2][(int)e->v3] = vel;

		if ( get_ch_rc ( (int)e->v2, &rc, &w32g_tracer_wnd.rc_velocity ) == 0 )
			tracer_velocity_draw ( &rc, w32g_tracer_wnd.velocity[(int)e->v2], VEL_MAX );
		}
		break;
	case CTLE_MASTER_VOLUME:
//		ctl_master_volume((int)e->v1);
#if 0
		w32g_tracer_wnd.master_volume = (int)e->v1;
		cheap_volume_view_draw ( &w32g_tracer_wnd.rc_master_volume, w32g_tracer_wnd.master_volume, 256, C_BAR_FORE, C_BAR_BACK, CVV_TYPE_LEFT );
#endif
		break;
	case CTLE_PROGRAM:
		v1 = (int)e->v1; v2 = (int)e->v2;
		tracer_ch_program_draw ( v1, -1, v2, (char *)e->v3, -1 );
		break;
	case CTLE_DRUMPART:
		break;
	case CTLE_VOLUME:
		v1 = (int)e->v1; v2 = (int)e->v2;
		if ( w32g_tracer_wnd.volume[v1] != v2 )
			if ( get_ch_rc ( v1, &rc, &w32g_tracer_wnd.rc_volume ) == 0 )
				tracer_volume_draw ( &rc, v2, 128 );
		w32g_tracer_wnd.volume[v1] = v2;
		break;
	case CTLE_EXPRESSION:
		v1 = (int)e->v1; v2 = (int)e->v2;
		if ( w32g_tracer_wnd.expression[v1] != v2 )
			if ( get_ch_rc ( v1, &rc, &w32g_tracer_wnd.rc_expression ) == 0 )
				tracer_expression_draw ( &rc, v2, 128 );
		w32g_tracer_wnd.expression[v1] = v2;
		break;
	case CTLE_PANNING:
		v1 = (int)e->v1; v2 = (int)e->v2;
		if ( w32g_tracer_wnd.panning[v1] != v2 )
			if ( get_ch_rc ( v1, &rc, &w32g_tracer_wnd.rc_panning ) == 0 )
				tracer_pan_draw ( &rc, v2, 128 );
		w32g_tracer_wnd.panning[v1] = v2;
		break;
	case CTLE_SUSTAIN:
		v1 = (int)e->v1; v2 = (int)e->v2;
		if ( w32g_tracer_wnd.sustain[v1] != v2 )
			if ( get_ch_rc ( v1, &rc, &w32g_tracer_wnd.rc_sustain ) == 0 )
				tracer_sustain_draw ( &rc, v2 );
		w32g_tracer_wnd.sustain[v1] = v2;
		break;
	case CTLE_PITCH_BEND:
		v1 = (int)e->v1; v2 = (int)e->v2;
		if ( w32g_tracer_wnd.pitch_bend[v1] != v2 )
			if ( get_ch_rc ( v1, &rc, &w32g_tracer_wnd.rc_pitch_bend ) == 0 )
				tracer_pitch_bend_draw ( &rc, v2, 0x4000 );
		w32g_tracer_wnd.pitch_bend[v1] = v2;
		break;
	case CTLE_MOD_WHEEL:
		v1 = (int)e->v1; v2 = (int)e->v2;
		if ( w32g_tracer_wnd.mod_wheel[v1] != v2 )
			if ( get_ch_rc ( v1, &rc, &w32g_tracer_wnd.rc_mod_wheel ) == 0 )
				tracer_mod_wheel_draw ( &rc, v2, 32 );
		w32g_tracer_wnd.mod_wheel[v1] = v2;
		break;
	case CTLE_CHORUS_EFFECT:
		v1 = (int)e->v1; v2 = (int)e->v2;
		if ( w32g_tracer_wnd.chorus_effect[v1] != v2 )
			if ( get_ch_rc ( v1, &rc, &w32g_tracer_wnd.rc_chorus_effect ) == 0 )
				tracer_chorus_effect_draw ( &rc, v2, 128 );
		w32g_tracer_wnd.chorus_effect[v1] = v2;
		break;
	case CTLE_REVERB_EFFECT:
		v1 = (int)e->v1; v2 = (int)e->v2;
		if ( w32g_tracer_wnd.reverb_effect[v1] != v2 )
			if ( get_ch_rc ( v1, &rc, &w32g_tracer_wnd.rc_reverb_effect ) == 0 )
				tracer_reverb_effect_draw ( &rc, v2, 128 );
		w32g_tracer_wnd.reverb_effect[v1] = v2;
		break;
	case CTLE_LYRIC:
		break;
	case CTLE_REFRESH:
		break;
	case CTLE_RESET:
		TracerWndReset2();
		if ( w32g_tracer_wnd.play_system_mode != play_system_mode ) {
			get_head_rc ( &rc, &w32g_tracer_wnd.rc_gm );
			tracer_gm_draw ( &rc, play_system_mode == GM_SYSTEM_MODE ? 1 : 0 );
			get_head_rc ( &rc, &w32g_tracer_wnd.rc_gs );
			tracer_gs_draw ( &rc, play_system_mode == GS_SYSTEM_MODE ? 1 : 0 );
			get_head_rc ( &rc, &w32g_tracer_wnd.rc_xg );
			tracer_xg_draw ( &rc, play_system_mode == XG_SYSTEM_MODE ? 1 : 0 );
		}
		w32g_tracer_wnd.play_system_mode = play_system_mode;
		break;
	case CTLE_SPEANA:
		break;
	case CTLE_PAUSE:
		break;
	case CTLE_MAXVOICES:
#if 0
		w32g_tracer_wnd.maxvoices = (int)e->v1;
#endif
		break;
	case CTLE_GSLCD:
		break;
    }
}

static int get_head_rc ( RECT *rc, RECT *rc_base )
{
	rc->top = w32g_tracer_wnd.rc_head.top + rc_base->top;
	rc->bottom = w32g_tracer_wnd.rc_head.top + rc_base->bottom;
	rc->left = w32g_tracer_wnd.rc_head.left + rc_base->left;
	rc->right = w32g_tracer_wnd.rc_head.left + rc_base->right;
	return 0;
}

static int get_ch_rc ( int ch, RECT *rc, RECT *rc_base )
{
	switch (TracerWndInfo.mode) {
	default:
	case TWI_MODE_1_32CH:
		break;
	case TWI_MODE_1_16CH:
		if ( ch >= 16 )
			return -1;
		break;
	case TWI_MODE_17_32CH:
		if ( ch < 16 )
			return -1;
		ch = ch - 16;
		break;
	}
	rc->top = w32g_tracer_wnd.rc_all_channels.top + 
		rc_base->top + ( w32g_tracer_wnd.ch_height + w32g_tracer_wnd.ch_space ) * ( ch - 0 );
	rc->bottom = w32g_tracer_wnd.rc_all_channels.top + 
		rc_base->bottom + ( w32g_tracer_wnd.ch_height + w32g_tracer_wnd.ch_space ) * ( ch - 0 );
	rc->left = w32g_tracer_wnd.rc_all_channels.left + rc_base->left;
	rc->right = w32g_tracer_wnd.rc_all_channels.left + rc_base->right;
	return 0;
}

static int cheap_notes_view_draw ( RECT *lprc, int note, int vel, int back_draw )
{
	HDC hdc = w32g_tracer_wnd.hmdc;
	RECT rc1;
	int note1, left, top;

	if ( !w32g_tracer_wnd.active )
		return 0;
	note1 = note / 12;
	left = rc1.left = lprc->left + 6 * 7 * note1 + 0;
	top = rc1.top = lprc->top + 0;
	rc1.right = lprc->left + 6 * 7 * (note1 + 1) - 0;
	rc1.bottom = lprc->top + 19 + 1;

	if ( back_draw ) {
		GDI_LOCK();
		BitBlt ( hdc, rc1.left, rc1.top, rc1.right - rc1.left, rc1.bottom - rc1.top,
			tracer_bmp.hmdc, tracer_bmp.rc_notes.left, tracer_bmp.rc_notes.top, SRCCOPY );
		GDI_UNLOCK();
		InvalidateRect ( w32g_tracer_wnd.hwnd, &rc1, FALSE );
	}
	switch ( note % 12 ) {
	case 1:
		rc1.right = rc1.left + 8 + 6 * 0;
		rc1.left += 4 + 6 * 0;
		rc1.bottom = rc1.top + 11;
		rc1.top += 4;
		break;
	case 3:
		rc1.right = rc1.left + 8 + 6 * 1;
		rc1.left += 4 + 6 * 1;
		rc1.bottom = rc1.top + 11;
		rc1.top += 4;
		break;
	case 6:
		rc1.right = rc1.left + 8 + 6 * 3;
		rc1.left += 4 + 6 * 3;
		rc1.bottom = rc1.top + 11;
		rc1.top += 4;
		break;
	case 8:
		rc1.right = rc1.left + 8 + 6 * 4;
		rc1.left += 4 + 6 * 4;
		rc1.bottom = rc1.top + 11;
		rc1.top += 4;
		break;
	case 10:
		rc1.right = rc1.left + 8 + 6 * 5;
		rc1.left += 4 + 6 * 5;
		rc1.bottom = rc1.top + 11;
		rc1.top += 4;
		break;
	case 0:
		rc1.right = rc1.left + 5 + 6 * 0;
		rc1.left += 1 + 6 * 0;
		rc1.bottom = rc1.top + 18;
		rc1.top += 11;
		break;
	case 2:
		rc1.right = rc1.left + 5 + 6 * 1;
		rc1.left += 1 + 6 * 1;
		rc1.bottom = rc1.top + 18;
		rc1.top += 11;
		break;
	case 4:
		rc1.right = rc1.left + 5 + 6 * 2;
		rc1.left += 1 + 6 * 2;
		rc1.bottom = rc1.top + 18;
		rc1.top += 11;
		break;
	case 5:
		rc1.right = rc1.left + 5 + 6 * 3;
		rc1.left += 1 + 6 * 3;
		rc1.bottom = rc1.top + 18;
		rc1.top += 11;
		break;
	case 7:
		rc1.right = rc1.left + 5 + 6 * 4;
		rc1.left += 1 + 6 * 4;
		rc1.bottom = rc1.top + 18;
		rc1.top += 11;
		break;
	case 9:
		rc1.right = rc1.left + 5 + 6 * 5;
		rc1.left += 1 + 6 * 5;
		rc1.bottom = rc1.top + 18;
		rc1.top += 11;
		break;
	case 11:
		rc1.right = rc1.left + 5 + 6 * 6;
		rc1.left += 1 + 6 * 6;
		rc1.bottom = rc1.top + 18;
		rc1.top += 11;
		break;
	default:
		break;
	}
	GDI_LOCK();
	if ( vel >= 0 ) {
		HBRUSH hOn = CreateSolidBrush ( RGB(0xFF,0x00,0x00) );
		FillRect ( hdc, &rc1, hOn );
		DeleteObject ( (HGDIOBJ) hOn );
	} else if ( vel  == TRACER_VOICE_SUSTAINED ) {
		HBRUSH hOn = CreateSolidBrush ( RGB(0xA0,0x00,0x00) );
		FillRect ( hdc, &rc1, hOn );
		DeleteObject ( (HGDIOBJ) hOn );
	} else {
		BitBlt ( hdc, rc1.left, rc1.top, rc1.right - rc1.left, rc1.bottom - rc1.top,
			tracer_bmp.hmdc, tracer_bmp.rc_notes.left + rc1.left - left, tracer_bmp.rc_notes.top + rc1.top - top, SRCCOPY );
	}
	GDI_UNLOCK();

	InvalidateRect ( w32g_tracer_wnd.hwnd, &rc1, FALSE );

	return 0;
}

static int tracer_ch_program_draw ( int ch, int bank, int program, char *instrument, int mapID )
{
	RECT rc;
	char buff[64];
	char *p_buff;
	if ( bank >= 0 ) {
		w32g_tracer_wnd.bank[ch] = bank;
	} else {
		if ( ISDRUMCHANNEL(ch) )
			bank = w32g_tracer_wnd.bank[ch] = 128;
		else
			bank = w32g_tracer_wnd.bank[ch] = channel[ch].bank;
	}
	if ( bank == 128 ) {
		if ( get_ch_rc ( ch, &rc, &w32g_tracer_wnd.rc_bank ) == 0 ) {
			sprintf ( buff, "drum");
			cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
		}
	} else {
		if ( get_ch_rc ( ch, &rc, &w32g_tracer_wnd.rc_bank ) == 0 ) {
			sprintf ( buff, "%03d", bank );
			cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
		}
	}
	if ( program >= 0 ) {
		w32g_tracer_wnd.program[ch] = program;
	} else {
		program = w32g_tracer_wnd.program[ch] = channel[ch].program;
	}
	if ( get_ch_rc ( ch, &rc, &w32g_tracer_wnd.rc_program ) == 0 ) {
		sprintf ( buff, "%03d", program );
		cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	}

	if ( instrument == NULL )
		instrument = " ----";
	if ( w32g_tracer_wnd.instrument[ch] != instrument ) { 
		w32g_tracer_wnd.instrument[ch][0] = ' ';
		strncpy ( w32g_tracer_wnd.instrument[ch] + 1, (char *)instrument, 250 );
		w32g_tracer_wnd.instrument[ch][250] = '\0';
	}
	if ( get_ch_rc ( ch, &rc, &w32g_tracer_wnd.rc_instrument ) == 0 )
		cheap_string_view_draw ( &rc, w32g_tracer_wnd.instrument[ch], C_TEXT_FORE, C_TEXT_BACK, CSV_LEFT );

	if ( mapID >= 0 ) {
		w32g_tracer_wnd.mapID[ch] = mapID;
	} else {
		mapID = w32g_tracer_wnd.mapID[ch] = channel[ch].mapID;
	}
	switch ( mapID ) {
	default:
    case INST_NO_MAP:
    case NUM_INST_MAP:
		p_buff = "----";
		break;
    case SC_55_TONE_MAP:
		p_buff = "55 T";
		break;
    case SC_55_DRUM_MAP:
		p_buff = "55 D";
		break;
    case SC_88_TONE_MAP:
		p_buff = "88 T";
		break;
    case SC_88_DRUM_MAP:
		p_buff = "88 D";
		break;
    case SC_88PRO_TONE_MAP:
		p_buff = "88Pro T";
		break;
    case SC_88PRO_DRUM_MAP:
		p_buff = "88Pro D";
		break;
    case XG_NORMAL_MAP:
		p_buff = "XG";
		break;
    case XG_SFX64_MAP:
		p_buff = "XG SFX64";
		break;
    case XG_SFX126_MAP:
		p_buff = "XG SFX126";
		break;
    case XG_DRUM_MAP:
		p_buff = "XG D";
		break;
	}
	if ( get_ch_rc ( ch, &rc, &w32g_tracer_wnd.rc_inst_map ) == 0 )
		cheap_string_view_draw ( &rc, p_buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );

	return 0;
}

static int tracer_velocity_draw ( RECT *lprc, int vol, int max )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	if ( vol >= max ) vol = max - 1;
	vol = vol * 20 / max;
	if ( vol < 10 ) {
		BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
			tracer_bmp.hmdc, tracer_bmp.rc_velocity[0].left, tracer_bmp.rc_velocity[0].top + vol * ( 19 + 1 ), SRCCOPY );
	} else {
		vol -= 10;
		if ( vol >= 10 ) vol = 9;
		BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
			tracer_bmp.hmdc, tracer_bmp.rc_velocity[1].left, tracer_bmp.rc_velocity[1].top + vol * ( 19 + 1 ), SRCCOPY );
	}
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int tracer_volume_draw ( RECT *lprc, int vol, int max )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	if ( vol >= max ) vol = max - 1;
	vol = vol * tracer_bmp.pan_max / max;
	BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
		tracer_bmp.hmdc, tracer_bmp.rc_volume.left, tracer_bmp.rc_volume.top + vol * ( 9 + 1 ), SRCCOPY );
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int tracer_expression_draw ( RECT *lprc, int vol, int max )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	if ( vol >= max ) vol = max - 1;
	vol = vol * tracer_bmp.pan_max / max;
	BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
		tracer_bmp.hmdc, tracer_bmp.rc_expression.left, tracer_bmp.rc_expression.top + vol * ( 9 + 1 ), SRCCOPY );
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int tracer_pan_draw ( RECT *lprc, int vol, int max )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	if ( vol >= max ) vol = max - 1;
	vol = vol * tracer_bmp.pan_max / max;
	BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
		tracer_bmp.hmdc, tracer_bmp.rc_pan.left, tracer_bmp.rc_pan.top + vol * ( 9 + 1 ), SRCCOPY );
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int tracer_sustain_draw ( RECT *lprc, int vol )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	if ( vol <= 0 ) vol = 0;
	if ( vol >= 1 ) vol = 1;
	BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
		tracer_bmp.hmdc, tracer_bmp.rc_sustain.left, tracer_bmp.rc_sustain.top + vol * ( 9 + 1 ), SRCCOPY );
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int tracer_pitch_bend_draw ( RECT *lprc, int vol, int max )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	if ( vol >= max ) vol = max - 1;
	vol = vol * tracer_bmp.pan_max / max;
	BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
		tracer_bmp.hmdc, tracer_bmp.rc_pitch_bend.left, tracer_bmp.rc_pitch_bend.top + vol * ( 9 + 1 ), SRCCOPY );
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int tracer_mod_wheel_draw ( RECT *lprc, int vol, int max )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	if ( vol >= max ) vol = max - 1;
	vol = vol * tracer_bmp.pan_max / max;
	BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
		tracer_bmp.hmdc, tracer_bmp.rc_mod_wheel.left, tracer_bmp.rc_mod_wheel.top + vol * ( 9 + 1 ), SRCCOPY );
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int tracer_chorus_effect_draw ( RECT *lprc, int vol, int max )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	if ( vol >= max ) vol = max - 1;
	vol = vol * tracer_bmp.pan_max / max;
	BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
		tracer_bmp.hmdc, tracer_bmp.rc_chorus_effect.left, tracer_bmp.rc_chorus_effect.top + vol * ( 9 + 1 ), SRCCOPY );
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int tracer_reverb_effect_draw ( RECT *lprc, int vol, int max )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	if ( vol >= max ) vol = max - 1;
	vol = vol * tracer_bmp.pan_max / max;
	BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
		tracer_bmp.hmdc, tracer_bmp.rc_reverb_effect.left, tracer_bmp.rc_reverb_effect.top + vol * ( 9 + 1 ), SRCCOPY );
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int tracer_gm_draw ( RECT *lprc, int flag )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	switch ( flag ) {
	default:
	case 0:
		BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
			tracer_bmp.hmdc, tracer_bmp.rc_gm_off.left, tracer_bmp.rc_gm_off.top, SRCCOPY );
		break;
	case 1:
		BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
			tracer_bmp.hmdc, tracer_bmp.rc_gm_on.left, tracer_bmp.rc_gm_on.top, SRCCOPY );
		break;
	}
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int tracer_gs_draw ( RECT *lprc, int flag )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	switch ( flag ) {
	default:
	case 0:
		BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
			tracer_bmp.hmdc, tracer_bmp.rc_gs_off.left, tracer_bmp.rc_gs_off.top, SRCCOPY );
		break;
	case 1:
		BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
			tracer_bmp.hmdc, tracer_bmp.rc_gs_on.left, tracer_bmp.rc_gs_on.top, SRCCOPY );
		break;
	}
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int tracer_xg_draw ( RECT *lprc, int flag )
{
	HDC hdc;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	switch ( flag ) {
	default:
	case 0:
		BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
			tracer_bmp.hmdc, tracer_bmp.rc_xg_off.left, tracer_bmp.rc_xg_off.top, SRCCOPY );
		break;
	case 1:
		BitBlt ( hdc, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
			tracer_bmp.hmdc, tracer_bmp.rc_xg_on.left, tracer_bmp.rc_xg_on.top, SRCCOPY );
	break;
	}
	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );
	return 0;
}

static int cheap_volume_view_draw ( RECT *lprc, int vol, int max, COLORREF fore, COLORREF back, int type )
{
	RECT rc1;
	HDC hdc;
	HBRUSH hFore, hBack;

	if ( !w32g_tracer_wnd.active )
		return 0;
	rc1;
	hdc = w32g_tracer_wnd.hmdc;
	hFore = CreateSolidBrush ( fore );
	hBack = CreateSolidBrush ( back );
	if ( vol > max ) vol = max;
	if ( vol < 0 ) vol = 0;
	switch (type) {
	default:
	case CVV_TYPE_LEFT:
		rc1.left = lprc->left;
		rc1.right = lprc->left + (lprc->right - lprc->left) * vol / max;
		rc1.top = lprc->top;
		rc1.bottom = lprc->bottom;
		break;
	case CVV_TYPE_RIGHT:
		rc1.left = lprc->left + (lprc->right - lprc->left) * (max - vol) / max;
		rc1.right = lprc->right;
		rc1.top = lprc->top;
		rc1.bottom = lprc->bottom;
		break;
	case CVV_TYPE_TOP:
		rc1.left = lprc->left;
		rc1.right = lprc->right;
		rc1.top = lprc->top;
		rc1.bottom = lprc->top + (lprc->bottom - lprc->top) * vol / max;
		break;
	case CVV_TYPE_BOTTOM:
		rc1.left = lprc->left;
		rc1.right = lprc->right;
		rc1.top = lprc->top + (lprc->bottom - lprc->top) * (max - vol) / max;
		rc1.bottom = lprc->bottom;
		break;
	}
	FillRect(hdc, &rc1, hFore);
	InvalidateRect ( w32g_tracer_wnd.hwnd, &rc1, FALSE );

	switch (type) {
	default:
	case CVV_TYPE_LEFT:
		rc1.left = rc1.right;
		rc1.right = lprc->right;
		break;
	case CVV_TYPE_RIGHT:
		rc1.left = lprc->left;
		rc1.right = rc1.left;
		break;
	case CVV_TYPE_TOP:
		rc1.top = rc1.bottom;
		rc1.bottom = lprc->bottom;
		break;
	case CVV_TYPE_BOTTOM:
		rc1.top = lprc->top;
		rc1.bottom = rc1.top;
		break;
	}
	FillRect(hdc, &rc1, hBack);
	InvalidateRect ( w32g_tracer_wnd.hwnd, &rc1, FALSE );

	DeleteObject ( (HGDIOBJ) hFore );
	DeleteObject ( (HGDIOBJ) hBack );

	return 0;
}

static int cheap_string_view_draw_font ( RECT *lprc, char *str, COLORREF fore, COLORREF back, int mode, HFONT hFont )
{
	HDC hdc;
	COLORREF old_fore, old_back;
	HGDIOBJ hgdiobj;
	UINT old_mode;
	int left, top, bottom;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	if ( mode == CSV_CENTER ) {
		old_mode = SetTextAlign(hdc, TA_CENTER | TA_BOTTOM );
		left = ( lprc->left + lprc->right ) / 2;
		top = lprc->top;
		bottom = lprc->bottom;
	} else if ( mode == CSV_LEFT ) {
		old_mode = SetTextAlign(hdc, TA_LEFT | TA_BOTTOM );
		left = lprc->left;
		top = lprc->top;
		bottom = lprc->bottom;
	} else if ( mode == CSV_RIGHT ) {
		old_mode = SetTextAlign(hdc, TA_RIGHT | TA_BOTTOM );
		left = lprc->right;
		top = lprc->top;
		bottom = lprc->bottom;
	}
	old_fore = SetTextColor ( hdc, fore ); 
	old_back = SetBkColor ( hdc, back ); 
	hgdiobj = SelectObject( hdc, hFont );
//	ExtTextOut ( hdc, left, top, ETO_CLIPPED | ETO_OPAQUE, lprc, str, strlen(str), NULL);
	ExtTextOut ( hdc, left, bottom - 2, ETO_CLIPPED | ETO_OPAQUE, lprc, str, strlen(str), NULL);
	SetTextColor ( hdc, old_fore ); 
	SetBkColor ( hdc, old_back ); 
	SelectObject( hdc, hgdiobj );
	SetTextAlign(hdc, old_mode );

	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );

	return 0;
}

static int cheap_string_view_draw ( RECT *lprc, char *str, COLORREF fore, COLORREF back, int mode )
{
	return cheap_string_view_draw_font ( lprc, str, fore, back, mode, w32g_tracer_wnd.hFontCommon );
}

static int cheap_half_string_view_draw ( RECT *lprc, char *str, COLORREF fore, COLORREF back, int mode )
{
	HDC hdc;
	COLORREF old_fore, old_back;
	HGDIOBJ hgdiobj;
	UINT old_mode;
	HFONT hFont = w32g_tracer_wnd.hFontHalf;
	int left, top;
	if ( !w32g_tracer_wnd.active )
		return 0;
	hdc = w32g_tracer_wnd.hmdc;
	if ( mode == CSV_CENTER ) {
		old_mode = SetTextAlign(hdc, TA_CENTER );
		left = ( lprc->left + lprc->right ) / 2;
		top = lprc->top;
	} else if ( mode == CSV_LEFT ) {
		old_mode = SetTextAlign(hdc, TA_LEFT );
		left = lprc->left;
		top = lprc->top;
	} else if ( mode == CSV_RIGHT ) {
		old_mode = SetTextAlign(hdc, TA_RIGHT );
		left = lprc->right;
		top = lprc->top;
	}
	old_fore = SetTextColor ( hdc, fore ); 
	old_back = SetBkColor ( hdc, back ); 
	hgdiobj = SelectObject( hdc, hFont );
	ExtTextOut ( hdc, left, top-3, ETO_CLIPPED | ETO_OPAQUE, lprc, str, strlen(str), NULL);
	SetTextColor ( hdc, old_fore ); 
	SetBkColor ( hdc, old_back ); 
	SelectObject( hdc, hgdiobj );
	SetTextAlign(hdc, old_mode );

	InvalidateRect ( w32g_tracer_wnd.hwnd, lprc, FALSE );

	return 0;
}



void TracerWndPaintAll(int lockflag)
{
	int i, j;
	char buff[1024];
	RECT rc;
	if ( !w32g_tracer_wnd.active )
		return;
	if ( lockflag ) GDI_LOCK();
	// タイトル
	strcpy ( buff, "ch" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_channel_top );
	cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "  instrument  " );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_instrument );
	cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "  map  " );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_inst_map );
	cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "bank" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_bank );
	cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "prog" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_program );
	cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "vel" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_velocity );
	cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "vo" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_volume );
	cheap_half_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "ex" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_expression );
	cheap_half_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "pa" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_panning );
	cheap_half_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "su" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_sustain );
	cheap_half_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "pb" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_pitch_bend );
	cheap_half_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "mw" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_mod_wheel );
	cheap_half_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "ch" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_chorus_effect );
	cheap_half_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	strcpy ( buff, "re" );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_reverb_effect );
	cheap_half_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_gm );
	tracer_gm_draw ( &rc, w32g_tracer_wnd.play_system_mode == GM_SYSTEM_MODE ? 1 : 0 );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_gs );
	tracer_gs_draw ( &rc, w32g_tracer_wnd.play_system_mode == GS_SYSTEM_MODE ? 1 : 0 );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_xg );
	tracer_xg_draw ( &rc, w32g_tracer_wnd.play_system_mode == XG_SYSTEM_MODE ? 1 : 0 );
	strcpy ( buff, w32g_tracer_wnd.titlename );
	get_head_rc ( &rc, &w32g_tracer_wnd.rc_head_rest );
	cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_LEFT );

	// 各チャンネル
	for ( i = 0; i < TRACER_CHANNELS ; i ++ ) {
		if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_channel_top ) == 0 ) {
			sprintf ( buff, "%02d", i + 1);
			if ( IS_SET_CHANNELMASK ( w32g_tracer_wnd.quietchannels, i ) )
				cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK_VERY_DARK, CSV_CENTER );
			else {
				if ( IS_SET_CHANNELMASK ( w32g_tracer_wnd.channel_mute, i ) )
					cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK_DARK, CSV_CENTER );
				else
					cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
			}
		}

		tracer_ch_program_draw ( i, w32g_tracer_wnd.bank[i], w32g_tracer_wnd.program[i], w32g_tracer_wnd.instrument[i], w32g_tracer_wnd.mapID[i] );

		if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_velocity ) == 0 )
			tracer_velocity_draw ( &rc, w32g_tracer_wnd.velocity[i], VEL_MAX );

		if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_volume ) == 0 )
			tracer_volume_draw ( &rc, w32g_tracer_wnd.volume[i], 128 );

		if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_expression ) == 0 )
			tracer_expression_draw ( &rc, w32g_tracer_wnd.expression[i], 128 );

		if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_panning ) == 0 )
			tracer_pan_draw ( &rc, w32g_tracer_wnd.panning[i], 128 );

		if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_sustain ) == 0 )
			tracer_sustain_draw ( &rc, w32g_tracer_wnd.sustain[i] );

		if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_pitch_bend ) == 0 )
			tracer_pitch_bend_draw ( &rc, w32g_tracer_wnd.pitch_bend[i], 0x4000 );

		if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_mod_wheel ) == 0 )
			tracer_mod_wheel_draw ( &rc, w32g_tracer_wnd.mod_wheel[i], 32 );

		if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_chorus_effect ) == 0 )
			tracer_chorus_effect_draw ( &rc, w32g_tracer_wnd.chorus_effect[i], 128 );

		if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_reverb_effect ) == 0 )
			tracer_reverb_effect_draw ( &rc, w32g_tracer_wnd.reverb_effect[i], 128 );

		for ( j = 0; j < 128; j ++ ) {
			if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_notes ) == 0 )
				cheap_notes_view_draw ( &rc, j, w32g_tracer_wnd.notes[i][j], TRUE );
		}

	}

	// ...
	if ( lockflag ) GDI_UNLOCK();
	InvalidateRect( w32g_tracer_wnd.hwnd,NULL, FALSE );
}

// GUI スレッドからのみ呼べる
void TracerWndPaintDo(int flag)
{
	RECT rc;
	if ( !w32g_tracer_wnd.active )
		return;
	if ( flag ) InvalidateRect( w32g_tracer_wnd.hwnd,NULL, FALSE );
	if ( GetUpdateRect(w32g_tracer_wnd.hwnd, &rc, FALSE) ) {
		PAINTSTRUCT ps;
		if ( GDI_LOCK_EX(0) == 0 ) {
			w32g_tracer_wnd.hdc = BeginPaint(w32g_tracer_wnd.hwnd, &ps);
			BitBlt(w32g_tracer_wnd.hdc,rc.left,rc.top,rc.right,rc.bottom,w32g_tracer_wnd.hmdc,rc.left,rc.top,SRCCOPY);
			EndPaint(w32g_tracer_wnd.hwnd, &ps);
			GDI_UNLOCK();
		} else {
			InvalidateRect ( w32g_tracer_wnd.hwnd, &rc, FALSE );
		}
	}
}

BOOL CALLBACK
TracerCanvasWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess)
	{
		case WM_CREATE:
			break;
		case WM_PAINT:
	      	TracerWndPaintDo(FALSE);
	    	return 0;
		case WM_DROPFILES:
			SendMessage(hMainWnd,WM_DROPFILES,wParam,lParam);
			return 0;
		case WM_RBUTTONDBLCLK:
		case WM_LBUTTONDBLCLK:
			{
			int i, mode;
			int xPos = LOWORD(lParam);
			int yPos = HIWORD(lParam);
			RECT rc;
			int flag = FALSE;
			get_head_rc ( &rc, &w32g_tracer_wnd.rc_channel_top );
			if ( rc.left <= xPos && xPos <= rc.right && rc.top <= yPos && yPos <= rc.bottom ) {
				char buff[64];
				for ( i = 0; i < TRACER_CHANNELS; i ++ ) {
					if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_channel_top ) == 0 ) {
						sprintf ( buff, "%02d", i + 1 );
						if ( uMess == WM_RBUTTONDBLCLK )
							UNSET_CHANNELMASK ( channel_mute, i );
						else
							TOGGLE_CHANNELMASK ( channel_mute, i );
						if ( IS_SET_CHANNELMASK ( quietchannels, i ) )
							cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK_VERY_DARK, CSV_CENTER );
						else {
							flag = TRUE;
							if ( IS_SET_CHANNELMASK ( channel_mute, i ) )
								cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK_DARK, CSV_CENTER );
							else
								cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
						}
					}
				}
				if ( flag )
					w32g_send_rc ( RC_SYNC_RESTART, 0 );
				w32g_tracer_wnd.channel_mute = channel_mute;
				w32g_tracer_wnd.quietchannels = quietchannels;
				flag = TRUE;
			}
			if ( uMess == WM_RBUTTONDBLCLK )
				break;
			if ( flag )
				break;
			for ( i = 0; i < TRACER_CHANNELS; i ++ ) {
				if ( get_ch_rc ( i, &rc, &w32g_tracer_wnd.rc_channel_top ) == 0 ) {
					if ( rc.left <= xPos && xPos <= rc.right && rc.top <= yPos && yPos <= rc.bottom ) {
						char buff[64];
						sprintf ( buff, "%02d", i + 1 );
						TOGGLE_CHANNELMASK ( channel_mute, i );
						if ( IS_SET_CHANNELMASK ( quietchannels, i ) )
							cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK_VERY_DARK, CSV_CENTER );
						else {
							flag = TRUE;
							if ( IS_SET_CHANNELMASK ( channel_mute, i ) )
								cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK_DARK, CSV_CENTER );
							else
								cheap_string_view_draw ( &rc, buff, C_TEXT_FORE, C_TEXT_BACK, CSV_CENTER );
						}
						w32g_tracer_wnd.channel_mute = channel_mute;
						w32g_tracer_wnd.quietchannels = quietchannels;
						if ( flag )
							w32g_send_rc ( RC_SYNC_RESTART, 0 );
						flag = TRUE;
						break;
					}
				}
			}
			if ( flag )
				break;
			switch ( TracerWndInfo.mode ) {
			case TWI_MODE_1_32CH:
				mode = TWI_MODE_1_16CH;
				break;
			case TWI_MODE_1_16CH:
				mode = TWI_MODE_17_32CH;
				break;
			default:
			case TWI_MODE_17_32CH:
				mode = TWI_MODE_1_32CH;
				break;
			}
			change_tracer_wnd_mode ( mode );
			}
			break;
		case WM_CHAR:
//		case WM_KEYDOWN:
//			ctl->cmsg(CMSG_ERROR, VERB_NORMAL,
//				 "%x", wParam );
			switch ( wParam ) {
			case 0x50:	// P
			case 0x70:	// p
				SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_PREV,0),0);
				return 0;
			case 0x4e:	// N
			case 0x6e:	// n
				SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_NEXT,0),0);
				return 0;
			case 0x45:	// E
			case 0x65:	// e
				w32g_send_rc ( RC_RESTART, 0);
				SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_STOP,0),0);
				return 0;
			case 0x48:	// H
			case 0x68:	// h
				if ( PlayerLanguage == LANGUAGE_JAPANESE ){
				MessageBox(hTracerWnd,
					"キーコマンド\n"
					"トレーサウインドウコマンド\n"
					"  ESC: ヘルプを閉じる      H: ヘルプを出す\n"
					"  +: キーアップ    -: キーダウン\n"
					"  >: スピードアップ    <: スピードダウン\n"
					"プレイヤーコマンド\n"
					"  SPACE/ENTER: 演奏開始    E: 停止    S: 一時停止\n"
					"  P: 前の曲    N: 次の曲\n"
					"TiMidity コマンド\n"
					"  Q: 終了\n"
					,"ヘルプ", MB_OK);
				} else {
				MessageBox(hTracerWnd,
					"Usage of key.\n"
					"Tracer window command.\n"
					"  ESC: Close Help      H: Help\n"
					"  +: Key up    -: Key down\n"
					"  >: Speed up    <: Speed down\n"
					"Player command.\n"
					"  SPACE/ENTER: PLAY    E: Stop    S: Pause\n"
					"  P: Prev    N: Next\n"
					"TiMidity command.\n"
					"  Q: Quit\n"
					,"Help", MB_OK);
				}
				return 0;
			case 0x52:	// R
			case 0x72:	// r
				return 0;
			case 0x53:	// S
			case 0x73:	// s
				SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_PAUSE,0),0);
				return 0;
			case VK_ESCAPE:
				SendMessage(hTracerWnd,WM_COMMAND,MAKEWPARAM(0,IDCLOSE),0);
				return 0;
			case 0x51:	// Q
			case 0x71:	// q
				if(MessageBox(hTracerWnd,"Quit TiMidity?","TiMidity",MB_ICONQUESTION|MB_YESNO)==IDYES)
					SendMessage(hMainWnd,WM_CLOSE,0,0);
				return 0;
			case VK_SPACE:
			case VK_RETURN:
				SendMessage(hMainWnd,WM_COMMAND,MAKEWPARAM(IDM_PLAY,0),0);
				return 0;
			case 0x3E:		// <
				w32g_send_rc ( RC_SPEEDUP, 1);
				return 0;
			case 0x3C:		// <
				w32g_send_rc ( RC_SPEEDDOWN, 1);
				return 0;
			case 0x2B:		// +
				w32g_send_rc ( RC_KEYUP, 1);
				return 0;
			case 0x2D:		// -
//				w32g_send_rc ( RC_KEYDOWN, 1);
				w32g_send_rc ( RC_KEYDOWN, -1);
				return 0;
#if 0
			case 0x4F:		// O
				w32g_send_rc ( RC_VOICEINCR, 1);
				return 0;
			case 0x6F:		// o
				w32g_send_rc ( RC_VOICEDECR, 1);
				return 0;
#endif
			}
		default:
			return DefWindowProc(hwnd,uMess,wParam,lParam) ;
	}
	return 0L;
}

extern void MainWndUpdateTracerButton(void);

BOOL CALLBACK
TracerWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
		SetWindowPosSize(GetDesktopWindow(),hwnd,TracerWndInfo.PosX, TracerWndInfo.PosY );
		return FALSE;
	case WM_DESTROY:
		{
			RECT rc;
			GetWindowRect(hwnd,&rc);
			TracerWndInfo.PosX = rc.left;
			TracerWndInfo.PosY = rc.top;
		}
		INISaveTracerWnd();
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			ShowWindow(hwnd, SW_HIDE);
			MainWndUpdateTracerButton();
			break;
		default:
			return FALSE;
		}
	case WM_MOVE:
		// TracerWndInfo.PosX = (int) LOWORD(lParam);
		// TracerWndInfo.PosY = (int) HIWORD(lParam);
		{
			RECT rc;
			GetWindowRect(hwnd,&rc);
			TracerWndInfo.PosX = rc.left;
			TracerWndInfo.PosY = rc.top;
		}
		break;
	case WM_SHOWWINDOW:
		if ( wParam ) {
			w32g_tracer_wnd.active = TRUE;
			TracerWndClear( TRUE );
			TracerWndPaintAll ( TRUE );
		} else {
			w32g_tracer_wnd.active = FALSE;
		}
		break;
	case WM_CLOSE:
		ShowWindow(hTracerWnd, SW_HIDE);
		MainWndUpdateTracerButton();
		break;
	case WM_DROPFILES:
		SendMessage(hMainWnd,WM_DROPFILES,wParam,lParam);
		return 0;
	case WM_LBUTTONDBLCLK:
		{
		int mode;
		switch ( TracerWndInfo.mode ) {
		case TWI_MODE_1_32CH:
			mode = TWI_MODE_1_16CH;
			break;
		case TWI_MODE_1_16CH:
			mode = TWI_MODE_17_32CH;
			break;
		default:
		case TWI_MODE_17_32CH:
			mode = TWI_MODE_1_32CH;
			break;
		}
		change_tracer_wnd_mode ( mode );
		}
		break;
	default:
		return FALSE;
	}
	return FALSE;
}


extern int PosSizeSave;

#define SEC_TRACERWND "TracerWnd"
int INISaveTracerWnd(void)
{
	char *section = SEC_TRACERWND;
	char *inifile = TIMIDITY_WINDOW_INI_FILE;
	char buffer[256];
	if ( PosSizeSave ) {
		if ( TracerWndInfo.PosX >= 0 || TracerWndInfo.PosY >= 0 ) {
			if ( TracerWndInfo.PosX < 0 )
				TracerWndInfo.PosX = 0;
			if ( TracerWndInfo.PosY < 0 )
				TracerWndInfo.PosY = 0;
		}
		sprintf(buffer,"%d",TracerWndInfo.PosX);
		if ( TracerWndInfo.PosX >= 0 )
		WritePrivateProfileString(section,"PosX",buffer,inifile);
		sprintf(buffer,"%d",TracerWndInfo.PosY);
		if ( TracerWndInfo.PosY >= 0 )
		WritePrivateProfileString(section,"PosY",buffer,inifile);
	}
	sprintf(buffer,"%d",TracerWndInfo.mode);
	WritePrivateProfileString(section,"mode",buffer,inifile);
	WritePrivateProfileString(NULL,NULL,NULL,inifile);		// Write Flush
	return 0;
}

int INILoadTracerWnd(void)
{
	char *section = SEC_TRACERWND;
	char *inifile = TIMIDITY_WINDOW_INI_FILE;
	int num;
	num = GetPrivateProfileInt(section,"PosX",-1,inifile);
	TracerWndInfo.PosX = num;
	num = GetPrivateProfileInt(section,"PosY",-1,inifile);
	TracerWndInfo.PosY = num;
	num = GetPrivateProfileInt(section,"mode",TWI_MODE_1_32CH,inifile);
	TracerWndInfo.mode = num;
	return 0;
}

static int TracerWndInfoReset(HWND hwnd)
{
	memset(&TracerWndInfo,0,sizeof(TRACERWNDINFO));
	TracerWndInfo.PosX = - 1;
	TracerWndInfo.PosY = - 1;
	TracerWndInfo.mode = TWI_MODE_1_32CH;
	return 0;
}

static int TracerWndInfoApply(void)
{
	change_tracer_wnd_mode ( TracerWndInfo.mode );
	return 0;
}

static int change_tracer_wnd_mode ( int mode )
{
	RECT rc, rc2;
	switch (mode) {
	default:
	case TWI_MODE_1_32CH:
		w32g_tracer_wnd.height = 1 + 19 + 1 + (19 + 1) * 32 + 1; 
		break;
	case TWI_MODE_1_16CH:
	case TWI_MODE_17_32CH:
		w32g_tracer_wnd.height = 1 + 19 + 1 + (19 + 1) * 16 + 1; 
		break;
	}
	GetWindowRect ( hTracerWnd, &rc );
	GetClientRect ( hTracerWnd, &rc2 );
	rc.left = rc.left;
	rc.top = rc.top;
	rc.right = (rc.right - rc.left) - (rc2.right - rc2.left) + w32g_tracer_wnd.width;
	rc.bottom = (rc.bottom - rc.top) - (rc2.bottom - rc2.top) + w32g_tracer_wnd.height;
	MoveWindow ( hTracerWnd, rc.left, rc.top, rc.right, rc.bottom, TRUE);
	MoveWindow (w32g_tracer_wnd.hwnd,0,0,w32g_tracer_wnd.width,w32g_tracer_wnd.height,TRUE);
	TracerWndInfo.mode = mode;
	TracerWndClear(TRUE);
	TracerWndPaintAll ( TRUE );
	return 0;
}

void TracerWndApplyQuietChannel( ChannelBitMask quietchannels_ )
{
	w32g_tracer_wnd.quietchannels = quietchannels_;
	TracerWndPaintAll(TRUE);
}


