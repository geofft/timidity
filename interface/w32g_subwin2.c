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

    w32g2_subwin.c: Written by Daisuke Aoki <dai@y7.net>
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
#include "wrd.h"
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

#include "w32g_wrd.h"

#if defined(__CYGWIN32__) || defined(__MINGW32__)
#ifndef TPM_TOPALIGN
#define TPM_TOPALIGN	0x0000L
#endif
#endif

extern void VprintfEditCtlWnd(HWND hwnd, char *fmt, va_list argList);
extern void PrintfEditCtlWnd(HWND hwnd, char *fmt, ...);
extern void PutsEditCtlWnd(HWND hwnd, char *str);
extern void ClearEditCtlWnd(HWND hwnd);



// ****************************************************************************
// Wrd Window

static void WrdWndClear2(int lockflag);

w32g_wrd_wnd_t w32g_wrd_wnd;

BOOL SetWrdWndActive(void)
{
	if ( IsWindowVisible(hWrdWnd) ) {
		w32g_wrd_wnd.active = TRUE;
	} else {
		w32g_wrd_wnd.active = FALSE;
	}
	return w32g_wrd_wnd.active;
}



BOOL CALLBACK WrdWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK WrdCanvasWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam);
void InitWrdWnd(HWND hParentWnd)
{
	WNDCLASS wndclass ;

	hWrdWnd = CreateDialog
		(hInst,MAKEINTRESOURCE(IDD_DIALOG_WRD),hParentWnd,WrdWndProc);
	ShowWindow(hWrdWnd,SW_HIDE);
	w32g_wrd_wnd.font_height = 16; 
	w32g_wrd_wnd.font_width = 8; 
	w32g_wrd_wnd.row = 80; 
	w32g_wrd_wnd.col = 25;
	w32g_wrd_wnd.height = w32g_wrd_wnd.font_height * w32g_wrd_wnd.col; 
	w32g_wrd_wnd.width = w32g_wrd_wnd.font_width * w32g_wrd_wnd.row; 
	w32g_wrd_wnd.pals[W32G_WRDWND_BLACK] = 0x000000;
	w32g_wrd_wnd.pals[W32G_WRDWND_RED] = 0xFF0000;
	w32g_wrd_wnd.pals[W32G_WRDWND_BLUE] = 0x0000FF;
	w32g_wrd_wnd.pals[W32G_WRDWND_PURPLE] = 0xFF00FF;
	w32g_wrd_wnd.pals[W32G_WRDWND_GREEN] = 0x00FF00;
	w32g_wrd_wnd.pals[W32G_WRDWND_YELLOW] = 0xFFFF00;
	w32g_wrd_wnd.pals[W32G_WRDWND_LIGHTBLUE] = 0x00FFFF;
	w32g_wrd_wnd.pals[W32G_WRDWND_WHITE] = 0xFFFFFF;

	wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_CLASSDC;
	wndclass.lpfnWndProc   = WrdCanvasWndProc ;
	wndclass.cbClsExtra    = 0 ;
	wndclass.cbWndExtra    = 0 ;
	wndclass.hInstance     = hInst ;
	wndclass.hIcon         = NULL;
	wndclass.hCursor       = LoadCursor(0,IDC_ARROW) ;
	wndclass.hbrBackground = (HBRUSH)(COLOR_SCROLLBAR + 1);
	wndclass.lpszMenuName  = NULL;
	wndclass.lpszClassName = "wrd canvas wnd";
	RegisterClass(&wndclass);
  	w32g_wrd_wnd.hwnd = CreateWindowEx(0,"wrd canvas wnd",0,WS_CHILD,
		CW_USEDEFAULT,0,w32g_wrd_wnd.width,w32g_wrd_wnd.height,
		hWrdWnd,0,hInst,0);
	w32g_wrd_wnd.hdc = GetDC(w32g_wrd_wnd.hwnd);
	w32g_wrd_wnd.hbitmap = CreateCompatibleBitmap(w32g_wrd_wnd.hdc,w32g_wrd_wnd.width,w32g_wrd_wnd.height);
	w32g_wrd_wnd.hmdc = CreateCompatibleDC(w32g_wrd_wnd.hdc);
	w32g_wrd_wnd.hgdiobj_hmdcprev = SelectObject(w32g_wrd_wnd.hmdc,w32g_wrd_wnd.hbitmap);
	ReleaseDC(w32g_wrd_wnd.hwnd,w32g_wrd_wnd.hdc);
	{
		char fontname[1024];
		if ( PlayerLanguage == LANGUAGE_JAPANESE )
			strcpy(fontname,"ＭＳ 明朝");
		else
			strcpy(fontname,"Times New Roman");
		w32g_wrd_wnd.hFont = CreateFont(w32g_wrd_wnd.font_height,w32g_wrd_wnd.font_width,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
	      	FIXED_PITCH | FF_MODERN	,fontname);
	}

	WrdWndReset();
	w32g_wrd_wnd.active = FALSE;
	MoveWindow(w32g_wrd_wnd.hwnd,0,0,w32g_wrd_wnd.width,w32g_wrd_wnd.height,TRUE);
	WrdWndClear2(TRUE);
	ShowWindow(w32g_wrd_wnd.hwnd,SW_SHOW);
	UpdateWindow(w32g_wrd_wnd.hwnd);
	UpdateWindow(hWrdWnd);
}

void WrdWndReset(void)
{
	int i;
	w32g_wrd_wnd.curposx = 0;
	w32g_wrd_wnd.curposy = 0;
	w32g_wrd_wnd.curforecolor = W32G_WRDWND_WHITE;
	w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
	w32g_wrd_wnd.curattr = 0;
	for ( i = 0; i < w32g_wrd_wnd.col; i++ ) {
		memset(w32g_wrd_wnd.textbuf[i],0x20,w32g_wrd_wnd.row);
		memset(w32g_wrd_wnd.forecolorbuf[i],w32g_wrd_wnd.curforecolor,w32g_wrd_wnd.row);
		memset(w32g_wrd_wnd.backcolorbuf[i],w32g_wrd_wnd.curbackcolor,w32g_wrd_wnd.row);
		memset(w32g_wrd_wnd.attrbuf[i],0,w32g_wrd_wnd.row);
	}
	WrdWndClear(TRUE);
	InvalidateRect(w32g_wrd_wnd.hwnd,NULL, FALSE);
}

void WrdWndCurStateSaveAndRestore(int saveflag)
{
	static int saved_curposx = 0;
	static int saved_curposy = 0;
	static int saved_curforecolor = W32G_WRDWND_WHITE;
	static int saved_curbackcolor = W32G_WRDWND_BLACK;
	static int saved_curattr = 0;
	if ( saveflag ) {
		saved_curforecolor = w32g_wrd_wnd.curforecolor;
		saved_curbackcolor = w32g_wrd_wnd.curbackcolor;
		saved_curattr = w32g_wrd_wnd.curattr;
		saved_curposx = w32g_wrd_wnd.curposx;
		saved_curposy = w32g_wrd_wnd.curposy;
	} else {
		w32g_wrd_wnd.curforecolor = saved_curforecolor;
		w32g_wrd_wnd.curbackcolor = saved_curbackcolor;
		w32g_wrd_wnd.curattr = saved_curattr;
		WrdWndGoto( saved_curposx, saved_curposy );
	}
}

// from 行を to 行にコピー。
void WrdWndCopyLine(int from, int to, int lockflag)
{
	RECT rc;

	if ( !w32g_wrd_wnd.active ) return;
	if ( from == to ) return;
	if ( from < 0 || from >= w32g_wrd_wnd.col ) return;
	if ( to < 0 || to >= w32g_wrd_wnd.col ) return;
	memcpy(w32g_wrd_wnd.textbuf[to],w32g_wrd_wnd.textbuf[from],w32g_wrd_wnd.row);
	memcpy(w32g_wrd_wnd.forecolorbuf[to],w32g_wrd_wnd.forecolorbuf[from],w32g_wrd_wnd.row);
	memcpy(w32g_wrd_wnd.backcolorbuf[to],w32g_wrd_wnd.backcolorbuf[from],w32g_wrd_wnd.row);
	memcpy(w32g_wrd_wnd.attrbuf[to],w32g_wrd_wnd.attrbuf[from],w32g_wrd_wnd.row);
	SetRect(&rc,0,to*w32g_wrd_wnd.font_height,w32g_wrd_wnd.row*w32g_wrd_wnd.font_width,(to+1)*w32g_wrd_wnd.font_height);
	InvalidateRect( w32g_wrd_wnd.hwnd, &rc, FALSE );
	if ( lockflag ) gdi_lock();
	BitBlt(w32g_wrd_wnd.hmdc, 0, from * w32g_wrd_wnd.font_height,
		w32g_wrd_wnd.row * w32g_wrd_wnd.font_width,1 * w32g_wrd_wnd.font_height,
		w32g_wrd_wnd.hmdc, 0, to * w32g_wrd_wnd.font_height, SRCCOPY);	
	if ( lockflag ) gdi_unlock();
}

// from行から to 行まで削除
void WrdWndClearLineFromTo(int from, int to, int lockflag)
{
	HPEN hPen;
	HBRUSH hBrush;
	HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;
	RECT rc;
	int i;

	if ( !w32g_wrd_wnd.active ) return;
	if ( from < 0 ) from = 0;
	if ( from >= w32g_wrd_wnd.col ) from = w32g_wrd_wnd.col - 1;
	if ( to < 0 ) to = 0;
	if ( to >= w32g_wrd_wnd.col ) to = w32g_wrd_wnd.col - 1;
	if ( to < from ) return;

//	w32g_wrd_wnd.curforecolor = W32G_WRDWND_WHITE;
//	w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
//	w32g_wrd_wnd.curattr = 0;
	for ( i = from; i <= to; i++ ) {
		memset(w32g_wrd_wnd.textbuf[i],0x20,w32g_wrd_wnd.row);
		memset(w32g_wrd_wnd.forecolorbuf[i],W32G_WRDWND_WHITE,w32g_wrd_wnd.row);
		memset(w32g_wrd_wnd.backcolorbuf[i],W32G_WRDWND_BLACK,w32g_wrd_wnd.row);
		memset(w32g_wrd_wnd.attrbuf[i],0,w32g_wrd_wnd.row);
	}
	if ( lockflag ) gdi_lock();
	hPen = CreatePen(PS_SOLID,1,w32g_wrd_wnd.pals[W32G_WRDWND_BLACK]);
	hBrush = CreateSolidBrush(w32g_wrd_wnd.pals[W32G_WRDWND_BLACK]);
	hgdiobj_hpen = SelectObject(w32g_wrd_wnd.hmdc, hPen);
	hgdiobj_hbrush = SelectObject(w32g_wrd_wnd.hmdc, hBrush);
	SetRect(&rc,0,from*w32g_wrd_wnd.font_height,w32g_wrd_wnd.row*w32g_wrd_wnd.font_width,(to+1)*w32g_wrd_wnd.font_height);
	InvalidateRect( w32g_wrd_wnd.hwnd, &rc, FALSE );
	Rectangle(w32g_wrd_wnd.hmdc,rc.left,rc.top,rc.right,rc.bottom);
	SelectObject(w32g_wrd_wnd.hmdc, hgdiobj_hpen);
	DeleteObject(hPen);
	SelectObject(w32g_wrd_wnd.hmdc, hgdiobj_hbrush);
	DeleteObject(hBrush);
	if ( lockflag ) gdi_unlock();
}

// from 行を to 行に移動。
void WrdWndMoveLine(int from, int to, int lockflag)
{
	if ( !w32g_wrd_wnd.active ) return;
	if ( from == to ) return;
	if ( from < 0 || from >= w32g_wrd_wnd.col ) return;
	if ( to < 0 || to >= w32g_wrd_wnd.col ) return;
	if ( lockflag ) gdi_lock();
	WrdWndCopyLine(from, to, FALSE);
	WrdWndClearLineFromTo(from,from,FALSE);
	if ( lockflag ) gdi_unlock();
}

// スクロールダウンする。
void WrdWndScrollDown(int lockflag)
{
	int i;
	if ( !w32g_wrd_wnd.active ) return;
	if ( lockflag ) gdi_lock();
	for ( i = 0; i < w32g_wrd_wnd.col - 1; i++ ) {
		WrdWndCopyLine(i,i+1,FALSE);
	}
	WrdWndClearLineFromTo(0,0,FALSE);
	if ( lockflag ) gdi_unlock();
}

// スクロールアップする。
void WrdWndScrollUp(int lockflag)
{
	int i;
	if ( !w32g_wrd_wnd.active ) return;
	if ( lockflag ) gdi_lock();
	for ( i = 0; i < w32g_wrd_wnd.col - 1; i++ ) {
		WrdWndCopyLine(i+1,i,FALSE);
	}
	WrdWndClearLineFromTo(w32g_wrd_wnd.col-1,w32g_wrd_wnd.col-1,FALSE);
	if ( lockflag ) gdi_unlock();
}

// 画面消去
void WrdWndClear(int lockflag)
{
	if ( !w32g_wrd_wnd.active ) return;
	if ( lockflag ) gdi_lock();
	WrdWndClearLineFromTo(0,w32g_wrd_wnd.col-1,FALSE);
	if ( lockflag ) gdi_unlock();
}
void WrdWndClear2(int lockflag)
{
	HPEN hPen;
	HBRUSH hBrush;
	HGDIOBJ hgdiobj_hpen, hgdiobj_hbrush;
	RECT rc;

	if ( lockflag ) gdi_lock();
	hPen = CreatePen(PS_SOLID,1,w32g_wrd_wnd.pals[W32G_WRDWND_BLACK]);
	hBrush = CreateSolidBrush(w32g_wrd_wnd.pals[W32G_WRDWND_BLACK]);
	hgdiobj_hpen = SelectObject(w32g_wrd_wnd.hmdc, hPen);
	hgdiobj_hbrush = SelectObject(w32g_wrd_wnd.hmdc, hBrush);
	GetClientRect(w32g_wrd_wnd.hwnd,&rc);
	Rectangle(w32g_wrd_wnd.hmdc,rc.left,rc.top,rc.right,rc.bottom);
	InvalidateRect( w32g_wrd_wnd.hwnd, NULL, FALSE );
	SelectObject(w32g_wrd_wnd.hmdc, hgdiobj_hpen);
	DeleteObject(hPen);
	SelectObject(w32g_wrd_wnd.hmdc, hgdiobj_hbrush);
	DeleteObject(hBrush);
	if ( lockflag ) gdi_unlock();

}

// 文字出力
void WrdWndPutString(char *str, int lockflag)
{
	if ( !w32g_wrd_wnd.active ) return;
	WrdWndPutStringN(str, strlen(str),lockflag);
}
#ifndef _mbbtype
#ifndef _MBC_SINGLE
#define _MBC_SINGLE (0)
#endif
#ifndef _MBC_LEAD
#define _MBC_LEAD (1)
#endif
#ifndef _MBC_TRAIL
#define _MBC_TRAIL (2)
#endif
#ifndef _MBC_ILLEGAL
#define _MBC_ILLEGAL (-1)
#endif
#define is_sjis_kanji1(x) ((((unsigned char)(x))>=0x81 && ((unsigned char)(x))<=0x9f) || (((unsigned char)(x))>=0xe0 && ((unsigned char)(x))<=0xfc))
#define is_sjis_kanji2(x) ((((unsigned char)(x))>=0x40 && ((unsigned char)(x))<=0x7e) || (((unsigned char)(x))>=0x80 && ((unsigned char)(x))<=0xfc))
static int ___sjis_mbbtype(unsigned char c, int mbtype)
{
	if(mbtype==_MBC_LEAD){
		if(is_sjis_kanji2(c)) return _MBC_TRAIL; else return _MBC_ILLEGAL;
	} else { if(is_sjis_kanji1(c)) return _MBC_LEAD; else return _MBC_SINGLE; }
}
static int _mbbtype(unsigned char c, int mbtype)
{
	return ___sjis_mbbtype(c,mbtype);
}
#endif
// 文字出力(n文字)
void WrdWndPutStringN(char *str, int n, int lockflag)
{
	RECT rc;
	COLORREF prevforecolor;
	COLORREF prevbackcolor;
	HGDIOBJ hgdiobj;
	int i;

	if ( !w32g_wrd_wnd.active ) return;
	if ( lockflag ) gdi_lock();
	hgdiobj = SelectObject( w32g_wrd_wnd.hmdc, w32g_wrd_wnd.hFont );
	prevforecolor = SetTextColor( w32g_wrd_wnd.hmdc, w32g_wrd_wnd.pals[w32g_wrd_wnd.curforecolor]);
	prevbackcolor = SetBkColor( w32g_wrd_wnd.hmdc, w32g_wrd_wnd.pals[w32g_wrd_wnd.curbackcolor]);
	for(;;){
		if ( w32g_wrd_wnd.curposx + n <= w32g_wrd_wnd.row ) {
			memcpy( w32g_wrd_wnd.textbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx, str, n );
			memset( w32g_wrd_wnd.forecolorbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx,
				w32g_wrd_wnd.curforecolor, n );
			memset( w32g_wrd_wnd.backcolorbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx,
				w32g_wrd_wnd.curbackcolor, n );
			memset( w32g_wrd_wnd.backcolorbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx,
				w32g_wrd_wnd.curattr, n );
			SetRect(&rc,w32g_wrd_wnd.curposx*w32g_wrd_wnd.font_width,w32g_wrd_wnd.curposy*w32g_wrd_wnd.font_height,
				(w32g_wrd_wnd.curposx+n)*w32g_wrd_wnd.font_width,(w32g_wrd_wnd.curposy+1)*w32g_wrd_wnd.font_height);
			InvalidateRect( w32g_wrd_wnd.hwnd, &rc, FALSE );
			ExtTextOut( w32g_wrd_wnd.hmdc, rc.left, rc.top, ETO_OPAQUE | ETO_CLIPPED, &rc, str, n, NULL);
//			ExtTextOut( w32g_wrd_wnd.hmdc, rc.left, rc.top, 0, &rc, str, n, NULL);
			w32g_wrd_wnd.curposx += n;
			if ( w32g_wrd_wnd.curposx == w32g_wrd_wnd.row ) {
				w32g_wrd_wnd.curposx = 0;
				w32g_wrd_wnd.curposy++;
				if ( w32g_wrd_wnd.curposy == w32g_wrd_wnd.col ) {
					WrdWndScrollUp(FALSE);
				}
			}
			break;
		} else {
			int len = w32g_wrd_wnd.row - w32g_wrd_wnd.curposx;
			char mbt = _MBC_SINGLE;
			if ( PlayerLanguage == LANGUAGE_JAPANESE ) {
				for(i=0;i<len;i++){
					mbt = _mbbtype(str[i],mbt);
				}
				if ( mbt == _MBC_LEAD )
					len -= 1;
			}
			memcpy( w32g_wrd_wnd.textbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx, str, len );
			memset( w32g_wrd_wnd.forecolorbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx,
				w32g_wrd_wnd.curforecolor, len );
			memset( w32g_wrd_wnd.backcolorbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx,
				w32g_wrd_wnd.curbackcolor, len );
			memset( w32g_wrd_wnd.backcolorbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx,
				w32g_wrd_wnd.curattr, len );
			if ( mbt == _MBC_LEAD ) {
				w32g_wrd_wnd.textbuf[w32g_wrd_wnd.curposy][w32g_wrd_wnd.row-1] = 0;
				w32g_wrd_wnd.forecolorbuf[w32g_wrd_wnd.curposy][w32g_wrd_wnd.row-1] = W32G_WRDWND_BLACK;
				w32g_wrd_wnd.backcolorbuf[w32g_wrd_wnd.curposy][w32g_wrd_wnd.row-1] = W32G_WRDWND_BLACK;
				w32g_wrd_wnd.attrbuf[w32g_wrd_wnd.curposy][w32g_wrd_wnd.row-1] = 0;
			}
			SetRect(&rc,w32g_wrd_wnd.curposx*w32g_wrd_wnd.font_width,w32g_wrd_wnd.curposy*w32g_wrd_wnd.font_height,
				(w32g_wrd_wnd.curposx+len)*w32g_wrd_wnd.font_width,(w32g_wrd_wnd.curposy+1)*w32g_wrd_wnd.font_height);
			ExtTextOut( w32g_wrd_wnd.hmdc, rc.left, rc.top, ETO_OPAQUE, &rc, str, len, NULL);
			InvalidateRect( w32g_wrd_wnd.hwnd, &rc, FALSE );
			n -= len;
			str += len;
			w32g_wrd_wnd.curposx = 0;
			w32g_wrd_wnd.curposy++;
			if ( w32g_wrd_wnd.curposy == w32g_wrd_wnd.col ) {
				WrdWndScrollUp(FALSE);
			}
		}
	}
	SetTextColor( w32g_wrd_wnd.hmdc, prevforecolor);
	SetBkColor( w32g_wrd_wnd.hmdc, prevbackcolor);
	SelectObject( w32g_wrd_wnd.hmdc, hgdiobj );
	if ( lockflag ) gdi_unlock();
}
// left == TRUE : 行の左消去
// left != TRUE : 行の右消去
void WrdWndLineClearFrom(int left, int lockflag)
{
	RECT rc;
	COLORREF forecolor = w32g_wrd_wnd.pals[W32G_WRDWND_BLACK];
	COLORREF backcolor = w32g_wrd_wnd.pals[W32G_WRDWND_BLACK];
	COLORREF prevforecolor;
	COLORREF prevbackcolor;
	HGDIOBJ hgdiobj;

	if ( !w32g_wrd_wnd.active ) return;
	if ( lockflag ) gdi_lock();
	prevforecolor = SetTextColor( w32g_wrd_wnd.hmdc, forecolor );
	prevbackcolor = SetBkColor( w32g_wrd_wnd.hmdc, backcolor );
	hgdiobj = SelectObject( w32g_wrd_wnd.hmdc, w32g_wrd_wnd.hFont );
	if ( left ) {
		memset( w32g_wrd_wnd.textbuf[w32g_wrd_wnd.curposy] , 0x20 , w32g_wrd_wnd.curposx + 1 );
		memset( w32g_wrd_wnd.forecolorbuf[w32g_wrd_wnd.curposy], W32G_WRDWND_BLACK, w32g_wrd_wnd.curposx + 1 );
		memset( w32g_wrd_wnd.backcolorbuf[w32g_wrd_wnd.curposy], W32G_WRDWND_BLACK, w32g_wrd_wnd.curposx + 1 );
		memset( w32g_wrd_wnd.backcolorbuf[w32g_wrd_wnd.curposy], 0, w32g_wrd_wnd.curposx + 1 );
		SetRect(&rc,0,w32g_wrd_wnd.curposy*w32g_wrd_wnd.font_height,
			(w32g_wrd_wnd.curposx+1)*w32g_wrd_wnd.font_width,(w32g_wrd_wnd.curposy+1)*w32g_wrd_wnd.font_height);
		InvalidateRect( w32g_wrd_wnd.hwnd, &rc, FALSE );
		ExtTextOut( w32g_wrd_wnd.hmdc, rc.left, rc.top, ETO_OPAQUE, &rc,
			w32g_wrd_wnd.textbuf[w32g_wrd_wnd.curposy], w32g_wrd_wnd.curposx + 1, NULL);
	} else {
		memset( w32g_wrd_wnd.textbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx,
			0x20 , w32g_wrd_wnd.row - w32g_wrd_wnd.curposx );
		memset( w32g_wrd_wnd.forecolorbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx,
			W32G_WRDWND_BLACK, w32g_wrd_wnd.row - w32g_wrd_wnd.curposx );
		memset( w32g_wrd_wnd.backcolorbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx,
			W32G_WRDWND_BLACK, w32g_wrd_wnd.row - w32g_wrd_wnd.curposx );
		memset( w32g_wrd_wnd.backcolorbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx,
			0, w32g_wrd_wnd.row - w32g_wrd_wnd.curposx );
		SetRect(&rc,w32g_wrd_wnd.curposx*w32g_wrd_wnd.font_width,w32g_wrd_wnd.curposy*w32g_wrd_wnd.font_height,
			w32g_wrd_wnd.row*w32g_wrd_wnd.font_width,(w32g_wrd_wnd.curposy+1)*w32g_wrd_wnd.font_height);
		InvalidateRect( w32g_wrd_wnd.hwnd, &rc, FALSE );
		ExtTextOut( w32g_wrd_wnd.hmdc, rc.left, rc.top, ETO_OPAQUE, &rc,
			w32g_wrd_wnd.textbuf[w32g_wrd_wnd.curposy] + w32g_wrd_wnd.curposx, w32g_wrd_wnd.row - w32g_wrd_wnd.curposx, NULL);
	}
	SetTextColor( w32g_wrd_wnd.hmdc, prevforecolor);
	SetBkColor( w32g_wrd_wnd.hmdc, prevbackcolor);
	SelectObject( w32g_wrd_wnd.hmdc, hgdiobj );
	if ( lockflag ) gdi_unlock();
}
// PC98 のアトリビュートで設定
void WrdWndSetAttr98(int attr)
{
	if ( !w32g_wrd_wnd.active ) return;
	switch ( attr ) {
	case 0:	// 規定値
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_WHITE;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 1: // ハイライト
		w32g_wrd_wnd.curattr = 0;
		break;
	case 2: // バーティカルライン
		w32g_wrd_wnd.curattr = 0;
		break;
	case 4: // アンダーライン
		w32g_wrd_wnd.curattr = 0;
		break;
	case 5: // ブリンク
		w32g_wrd_wnd.curattr = 0;
		break;
	case 7: // リバース
		{
			char tmp = w32g_wrd_wnd.curbackcolor;
			w32g_wrd_wnd.curbackcolor = w32g_wrd_wnd.curforecolor;
			w32g_wrd_wnd.curforecolor = tmp;
			w32g_wrd_wnd.curattr = 0;
		}
		break;
	case 8: // シークレット
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 16:	// 黒
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 17:	// 赤
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_RED;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 18:	// 青
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLUE;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 19:		// 緑
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_GREEN;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 20:	// 紫
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_PURPLE;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;	
	case 21:	// 黄色
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_YELLOW;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 22:	// 水色
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_LIGHTBLUE;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 23: // 白
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_WHITE;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 30:	// 黒
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 31:	// 赤
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_RED;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 32:	// 緑
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_GREEN;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 33:	// 黄色
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_YELLOW;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 34:	// 青
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLUE;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 35:	// 紫
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_PURPLE;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 36:	// 水色
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_LIGHTBLUE;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 37:	// 白
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_WHITE;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 40:	// 黒反転
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 41:	// 赤反転
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_RED;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 42:	// 緑反転
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_GREEN;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 43:	// 黄色反転
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_YELLOW;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 44:	// 青反転
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLUE;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 45:	// 紫反転
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_PURPLE;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 46:	// 水色反転
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_LIGHTBLUE;
		w32g_wrd_wnd.curattr = 0;
		break;
	case 47:	// 白反転
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_WHITE;
		w32g_wrd_wnd.curattr = 0;
		break;
	default:
		w32g_wrd_wnd.curforecolor = W32G_WRDWND_WHITE;
		w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
		w32g_wrd_wnd.curattr = 0;
		break;
	}
}
// アトリビュートのリセット
void WrdWndSetAttrReset(void)
{
	if ( !w32g_wrd_wnd.active ) return;
	w32g_wrd_wnd.curforecolor = W32G_WRDWND_WHITE;
	w32g_wrd_wnd.curbackcolor = W32G_WRDWND_BLACK;
	w32g_wrd_wnd.curattr = 0;
}
// カーソルポジションの移動
void WrdWndGoto(int x, int y)
{
	if ( !w32g_wrd_wnd.active ) return;
	if ( x < 0 ) x = 0;
	if ( x >= w32g_wrd_wnd.row ) x = w32g_wrd_wnd.row - 1;
	if ( y < 0 ) y = 0;
	if ( y >= w32g_wrd_wnd.col ) y = w32g_wrd_wnd.col - 1;
	w32g_wrd_wnd.curposx = x;
	w32g_wrd_wnd.curposy = y;
}

void WrdWndPaintAll(int lockflag)
{
	RECT rc;
	COLORREF forecolor = w32g_wrd_wnd.pals[w32g_wrd_wnd.curforecolor];
	COLORREF backcolor = w32g_wrd_wnd.pals[w32g_wrd_wnd.curforecolor];
	COLORREF prevforecolor;
	COLORREF prevbackcolor;
	HGDIOBJ hgdiobj;
	int x, y;

	if ( !w32g_wrd_wnd.active ) return;
	if ( lockflag ) gdi_lock();
	prevforecolor = SetTextColor( w32g_wrd_wnd.hmdc, forecolor );
	prevbackcolor = SetBkColor( w32g_wrd_wnd.hmdc, backcolor );
	hgdiobj = SelectObject( w32g_wrd_wnd.hmdc, w32g_wrd_wnd.hFont );
	for( y = 0; y < w32g_wrd_wnd.col; y++ ) {
		for( x = 0; x < w32g_wrd_wnd.row; x++ ) {
			char mbt = _MBC_SINGLE;
			if ( forecolor != w32g_wrd_wnd.pals[w32g_wrd_wnd.forecolorbuf[y][x]] ) {
				forecolor = w32g_wrd_wnd.pals[w32g_wrd_wnd.forecolorbuf[y][x]];
				SetTextColor( w32g_wrd_wnd.hmdc, forecolor );
			}
			if ( forecolor != w32g_wrd_wnd.pals[w32g_wrd_wnd.forecolorbuf[y][x]] ) {
				backcolor = w32g_wrd_wnd.pals[w32g_wrd_wnd.backcolorbuf[y][x]];
				SetBkColor( w32g_wrd_wnd.hmdc, backcolor );
			}
			if ( PlayerLanguage == LANGUAGE_JAPANESE && _mbbtype( w32g_wrd_wnd.textbuf[y][x], _MBC_SINGLE ) == _MBC_LEAD ) {
				ExtTextOut( w32g_wrd_wnd.hmdc, rc.left, rc.top, ETO_OPAQUE, &rc, w32g_wrd_wnd.textbuf[y] + x, 2, NULL);
				x++;
			} else {
				ExtTextOut( w32g_wrd_wnd.hmdc, rc.left, rc.top, ETO_OPAQUE, &rc, w32g_wrd_wnd.textbuf[y] + x, 1, NULL);
			}
		}
	}
	SetTextColor( w32g_wrd_wnd.hmdc, prevforecolor);
	SetBkColor( w32g_wrd_wnd.hmdc, prevbackcolor);
	SelectObject( w32g_wrd_wnd.hmdc, hgdiobj );
	if ( lockflag ) gdi_unlock();
	InvalidateRect( w32g_wrd_wnd.hwnd,NULL, FALSE );
}

void WrdWndPaintDo(int flag)
{
	RECT rc;
	if ( flag ) InvalidateRect( w32g_wrd_wnd.hwnd,NULL, FALSE );
	if ( GetUpdateRect(w32g_wrd_wnd.hwnd, &rc, FALSE) ) {
		PAINTSTRUCT ps;
		gdi_lock(); // gdi_lock
		w32g_wrd_wnd.hdc = BeginPaint(w32g_wrd_wnd.hwnd, &ps);
		BitBlt(w32g_wrd_wnd.hdc,rc.left,rc.top,rc.right,rc.bottom,w32g_wrd_wnd.hmdc,rc.left,rc.top,SRCCOPY);
		EndPaint(w32g_wrd_wnd.hwnd, &ps);
		gdi_unlock(); // gdi_lock
	}
}

BOOL CALLBACK
WrdCanvasWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess)
	{
		case WM_CREATE:
			break;
		case WM_PAINT:
	      	WrdWndPaintDo(FALSE);
	    	return 0;
		case WM_DROPFILES:
			SendMessage(hMainWnd,WM_DROPFILES,wParam,lParam);
			return 0;
		default:
			return DefWindowProc(hwnd,uMess,wParam,lParam) ;
	}
	return 0L;
}

extern void MainWndUpdateWrdButton(void);

BOOL CALLBACK
WrdWndProc(HWND hwnd, UINT uMess, WPARAM wParam, LPARAM lParam)
{
	switch (uMess){
	case WM_INITDIALOG:
		return FALSE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
			ShowWindow(hwnd, SW_HIDE);
			MainWndUpdateWrdButton();
			break;
		default:
			return FALSE;
		}
		case WM_SIZE:
			return FALSE;
		case WM_CLOSE:
			ShowWindow(hWrdWnd, SW_HIDE);
			MainWndUpdateWrdButton();
			break;
		case WM_DROPFILES:
			SendMessage(hMainWnd,WM_DROPFILES,wParam,lParam);
			return 0;
		default:
			return FALSE;
	}
	return FALSE;
}
