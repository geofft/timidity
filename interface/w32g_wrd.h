#ifndef __W32G_WRD_H__
#define __W32G_WRD_H__

#define W32G_WRDWND_ROW 80
#define W32G_WRDWND_COL 25
#define W32G_WRDWND_ATTR_REVERSE	0x01
#define W32G_WRDWND_BLACK	0
#define W32G_WRDWND_RED			1
#define W32G_WRDWND_BLUE		2
#define W32G_WRDWND_PURPLE	3
#define W32G_WRDWND_GREEN	4
#define W32G_WRDWND_YELLOW	5
#define W32G_WRDWND_LIGHTBLUE	6
#define W32G_WRDWND_WHITE	7
#define W32G_WRDWND_GRAPHIC_PLANE_MAX 2
typedef struct w32g_wrd_wnd_t_ {
	HWND hwnd;
	HWND hParentWnd;
	HDC hdc;
	HDC hmdc;
	HGDIOBJ hgdiobj_hmdcprev;
	HBITMAP hbitmap;
	HFONT hFont;
	RECT rc;
#if 0
	BITMAPINFO bi_graphic[W32G_WRDWND_GRAPHIC_PLANE_MAX];
	RGBQUAD rgbq_graphic[W32G_WRDWND_GRAPHIC_PLANE_MAX];
	HBITMAP hbmp_graphic[W32G_WRDWND_GRAPHIC_PLANE_MAX];
	HBITMAP hmdc_graphic[W32G_WRDWND_GRAPHIC_PLANE_MAX];
	int cur_graphic_page;
#endif
	int font_height;
	int font_width;
	int height;
	int width;
	int row;
	int col;
	int curposx;
	int curposy;
	char curforecolor;
	char curbackcolor;
	char curattr;
	char textbuf[W32G_WRDWND_COL][W32G_WRDWND_ROW];
	char forecolorbuf[W32G_WRDWND_COL][W32G_WRDWND_ROW];
	char backcolorbuf[W32G_WRDWND_COL][W32G_WRDWND_ROW];
	char attrbuf[W32G_WRDWND_COL][W32G_WRDWND_ROW];
	COLORREF pals[32];
	int valid;
	int active;
	int updateall;
} w32g_wrd_wnd_t;
extern void WrdWndReset(void);
extern void WrdWndCopyLine(int from, int to, int lockflag);
extern void WrdWndClearLineFromTo(int from, int to, int lockflag);
extern void WrdWndMoveLine(int from, int to, int lockflag);
extern void WrdWndScrollDown(int lockflag);
extern void WrdWndScrollUp(int lockflag);
extern void WrdWndClear(int lockflag);
extern void WrdWndPutString(char *str, int lockflag);
extern void WrdWndPutStringN(char *str, int n, int lockflag);
extern void WrdWndLineClearFrom(int left, int lockflag);
extern void WrdWndSetAttr98(int attr);
extern void WrdWndSetAttrReset(void);
extern void WrdWndGoto(int x, int y);
extern void WrdWndPaintAll(int lockflag);
extern void WrdWndPaintDo(int flag);
extern void WrdWndCurStateSaveAndRestore(int saveflag);
extern w32g_wrd_wnd_t w32g_wrd_wnd;

#endif /* __W32G_WRD_H__ */
