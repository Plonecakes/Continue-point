/* Continue Point
 * Author: Plonecakes (http://github.com/Plonecakes)
 * Version: 0.1
 * License:
 *  Copyright (C) 2012 Plonecakes
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 *
 *  Though not required, I request that any modifications be kept open source.
 *
 * Usage:
 *  Shift+2 or Right-click -> Continue point -> Toggle to set a continue point
 *  Plugins -> Continue point -> View to view continue points window
 * 
 * Comments:
 *  Sometimes I set on-access breaks to find things, often this will pause in places
 *  that I don't care about. I made this plugin to remedy that.
 *  Some of this code is shameless lifted from the Bookmarks example plugin, and
 *  reformatted. Oleh, you're a cool guy, but your code is the worst to read.
 */

/* TODO:
 *  Figure out why ini stuff isn't working.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "Plugin.h"

#define VERSION "0.1"
#define COPYYEARS "2012"

HINSTANCE     hinst;           // DLL instance
HWND          hwmain;          // Handle of main OllyDbg window
char          windowclass[32]; // Name of window class

typedef struct t_contpoint {
  ulong          addr;
  ulong          size;
  ulong          type;
} t_contpoint;

t_table          contpoint;

void CreateContPointWindow(void);
LRESULT CALLBACK ContPointWindowProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp);
int SortFunction(t_contpoint *b1, t_contpoint *b2, int sort);
void ToggleContPoint(unsigned long address);

BOOL WINAPI DllEntryPoint(HINSTANCE hi, DWORD reason, LPVOID reserved) {
	if(reason == DLL_PROCESS_ATTACH) hinst = hi;
	return 1;
}

extc int _export cdecl ODBG_Plugindata(char shortname[32]) {
	strcpy(shortname, "Continue point");
	return PLUGIN_VERSION;
}

extc int _export cdecl ODBG_Plugininit(
	int ollydbgversion, HWND hw, ulong *features
) {
	// Check that version of OllyDbg is correct.
	if(ollydbgversion < PLUGIN_VERSION) return -1;

	hwmain = hw;

	if(Createsorteddata(&(contpoint.data), "Continue points",
		sizeof(t_contpoint), 30, (SORTFUNC *)SortFunction, NULL) != 0
	) {
		return -1; 
	}

	if(Registerpluginclass(windowclass, NULL, hinst, ContPointWindowProc) < 0) {
		Destroysorteddata(&(contpoint.data));
		return -1;
	};

	Addtolist(0,  0, "Continue point v" VERSION);
	Addtolist(0, -1, "  Copyright (C) " COPYYEARS " Plonecakes");

	if (Plugingetvalue(VAL_RESTOREWINDOWPOS) != 0 &&
		Pluginreadintfromini(hinst, "Restore continue points window", 0) != 0
	) {
		CreateContPointWindow();
	}

	return 0;
}

#define td_item ((t_dump*)item)
extc int _export cdecl ODBG_Pluginmenu(
	int origin, char data[4096], void *item
) {
	switch(origin) {
		case PM_MAIN:
			strcpy(data, "0 &View|11 &About");
			return 1;
		case PM_DISASM:
			if(td_item == NULL || td_item->size == 0) {
				return 0;
			}
			strcpy(data, "Continue point{0 &Toggle\tShift+2}");
			return 1;
		default: break;
	}
	return 0;
}

#define ABOUT_TEXT "Continue point v" VERSION "\n" \
                   "Copyright (C) " COPYYEARS " Plonecakes\n\n" \
                   "Used to skip memory breaks on certain addresses."
extc void _export cdecl ODBG_Pluginaction(
	int origin, int action, void *item
) {
	switch(origin) {
		case PM_MAIN:
			switch(action) {
				case 0:
					// "View"
					CreateContPointWindow();
					break;
				case 11:
					// "About"
					MessageBox(hwmain,
						ABOUT_TEXT, "Continue point",
						MB_OK | MB_ICONINFORMATION
					);
					break;
				default: break;
			}
			break;
		case PM_DISASM:
			switch(action) {
				case 0:
					// "Toggle"
					ToggleContPoint(td_item->sel0);
					break;
				default: break;
			}
			break;
		default: break;
	}
}

extc int _export cdecl ODBG_Pluginshortcut(
	int origin, int ctrl, int alt, int shift, int key, void *item
) {
	if(origin == PM_DISASM && !ctrl && !alt && shift && key == '2') {
		ToggleContPoint(td_item->sel0);
		return 1;
	}
	return 0;
}

extc int _export cdecl ODBG_Pausedex(
	int reason, int extdata, t_reg *reg, DEBUG_EVENT *debugevent
) {
	unsigned long address;

	// Only needs to handle breakpoints
	if(!(reason & (PP_INT3BREAK | PP_MEMBREAK | PP_HWBREAK))) {
		return 0;
	}

	// Is it a continue point?
	address = reg->ip;
	if(Findsorteddata(&(contpoint.data), address) != NULL) {
		// Continue if so
		Addtolist(0,  0, "Continue point at %08X", address);
		Go(0, 0, STEP_SAME, 0, reg->modified);
		return 1;
	}

	// Do not continue if not
	return 0;
}

extc void _export cdecl ODBG_Pluginreset(void) {
	// Currently does not save continue points, so it should probably kill them on a reset, too
	Deletesorteddatarange(&(contpoint.data), 0, 0xFFFFFFFF);
}

extc int _export cdecl ODBG_Pluginclose(void) {
	Pluginwriteinttoini(hinst, "Restore continue points window", contpoint.hw != NULL);
	return 0;
}

#define td_sort ((t_contpoint*)ph)
int WindowGetText(
	char *s, char *mask, int *select, t_sortheader *ph, int column
) {
	ulong cmdsize, decodesize;
	char cmd[MAXCMDSIZE], *pdecode;
	t_memory *pmem;
	t_disasm da;

	if(column == 0) return sprintf(s, "%08X", td_sort->addr);
	else if(column == 1) {
		pmem = Findmemory(td_sort->addr);
		if(pmem == NULL) {
			*select = DRAW_GRAY;
			return sprintf(s, "???");
		};

		cmdsize = pmem->base + pmem->size - td_sort->addr;
		if(cmdsize > MAXCMDSIZE) cmdsize = MAXCMDSIZE;

		if(Readmemory(cmd, td_sort->addr, cmdsize, MM_RESTORE|MM_SILENT) != cmdsize) {
			*select = DRAW_GRAY;
			return sprintf(s, "???");
		};

		pdecode = Finddecode(td_sort->addr, &decodesize);
		if(decodesize < cmdsize) pdecode = NULL;
		Disasm(cmd, cmdsize, td_sort->addr, pdecode, &da, DISASM_CODE, 0);
		return strlen(strcpy(s, da.result));
	}
	else if(column == 2) {
		return Findname(td_sort->addr, NM_COMMENT, s);
	}
	return 0;
}

void CreateContPointWindow(void) {
	if(contpoint.bar.nbar == 0) {
		// Initialize...
		contpoint.bar.name[0]  = "Address";    // Continue point address
		contpoint.bar.defdx[0] = 9;
		contpoint.bar.mode[0]  = 0;
		contpoint.bar.name[1]  = "Disassembly";// Disassembled command
		contpoint.bar.defdx[1] = 32;
		contpoint.bar.mode[1]  = BAR_NOSORT;
		contpoint.bar.name[2]  = "Comment";    // Comment
		contpoint.bar.defdx[2] = 256;
		contpoint.bar.mode[2]  = BAR_NOSORT;
		contpoint.bar.nbar     = 3;

		contpoint.mode = TABLE_COPYMENU | TABLE_SORTMENU | TABLE_APPMENU | TABLE_SAVEPOS | TABLE_ONTOP;
		contpoint.drawfunc = WindowGetText;
	}
	Quicktablewindow(&contpoint, 15, 3, windowclass, "Continue point");
}

LRESULT CALLBACK ContPointWindowProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
	int i, altkey, shiftkey, controlkey;
	HMENU menu;
	t_contpoint *cur_cp;
	switch(msg) {
		case WM_DESTROY:
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		case WM_HSCROLL:
		case WM_VSCROLL:
		case WM_TIMER:
		case WM_SYSKEYDOWN:
			Tablefunction(&contpoint, hw, msg, wp, lp);
			break;
		case WM_USER_SCR:
		case WM_USER_VABS:
		case WM_USER_VREL:
		case WM_USER_VBYTE:
		case WM_USER_STS:
		case WM_USER_CNTS:
		case WM_USER_CHGS:
		case WM_WINDOWPOSCHANGED:
			return Tablefunction(&contpoint, hw, msg, wp, lp);
		case WM_USER_MENU:
			menu = CreatePopupMenu();
			cur_cp = (t_contpoint *)Getsortedbyselection(&(contpoint.data), contpoint.data.selected);
			if(menu != NULL && cur_cp != NULL) {
				AppendMenu(menu, MF_STRING, 1, "&Follow\tEnter");
				AppendMenu(menu, MF_STRING, 2, "&Delete\tDel");
			}

			i = Tablefunction(&contpoint, hw, WM_USER_MENU, 0, (LPARAM)menu);
			if(menu != NULL) DestroyMenu(menu);

			// "Follow"
			if(i == 1) {
				Setcpu(0, cur_cp->addr, 0, 0, CPU_ASMHIST | CPU_ASMCENTER | CPU_ASMFOCUS);
			}
			// "Delete"
			else if(i == 2) {
				Deletesorteddata(&(contpoint.data), cur_cp->addr);
				// Redraw window
				InvalidateRect(hw, NULL, FALSE);
			}
			return 0;
		case WM_KEYDOWN:
			altkey = GetKeyState(VK_MENU) & 0x8000;
			shiftkey = GetKeyState(VK_SHIFT) & 0x8000;
			controlkey = GetKeyState(VK_CONTROL) & 0x8000;

			// "Follow"
			if(wp == VK_RETURN && !altkey && !shiftkey && !controlkey) {
				cur_cp = (t_contpoint *)Getsortedbyselection(&(contpoint.data), contpoint.data.selected);
				if(cur_cp != NULL) {
					Setcpu(0, cur_cp->addr, 0, 0, CPU_ASMHIST | CPU_ASMCENTER | CPU_ASMFOCUS);
				}
			}
			// "Delete"
			else if (wp == VK_DELETE && !altkey && !shiftkey && !controlkey) {
				cur_cp = (t_contpoint *)Getsortedbyselection(&(contpoint.data), contpoint.data.selected);
				if(cur_cp != NULL) {
					Deletesorteddata(&(contpoint.data), cur_cp->addr);
					// Redraw window
					InvalidateRect(hw, NULL, FALSE);
				}
			}
			// Default table key functions
			else Tablefunction(&contpoint, hw, msg, wp, lp);
			break;
		case WM_USER_DBLCLK:
			// "Follow"
			cur_cp = (t_contpoint *)Getsortedbyselection(&(contpoint.data), contpoint.data.selected);
			if(cur_cp != NULL) {
				Setcpu(0, cur_cp->addr, 0, 0, CPU_ASMHIST | CPU_ASMCENTER | CPU_ASMFOCUS);
			}
			return 1;
		case WM_USER_CHALL:
		case WM_USER_CHMEM:
			// Redraw window.
			InvalidateRect(hw, NULL, FALSE);
			return 0;
		case WM_PAINT:
			Painttable(hw, &contpoint, WindowGetText);
			return 0;
		default: break;
	}
	return DefMDIChildProc(hw, msg, wp, lp);
}

int SortFunction(t_contpoint *b1, t_contpoint *b2, int sort) {
	// Just allow address-based sorting for now...
	if(b1->addr > b2->addr) return 1;
	if(b1->addr < b2->addr) return -1;
	return 0;
}

void ToggleContPoint(unsigned long address){
	t_contpoint new_cp, *cp;
	// Check if we need to turn it off
	cp = (t_contpoint *)Findsorteddata(&(contpoint.data), address);
	if(cp != NULL) {
		Deletesorteddata(&(contpoint.data), address);
	}
	// Otherwise we need to turn it on
	else {
		new_cp.size = 1;
		new_cp.type = 0;
		new_cp.addr = address;
		Addsorteddata(&(contpoint.data), &new_cp);
	}

	// Redraw open window
	if(contpoint.hw != NULL) InvalidateRect(contpoint.hw, NULL, FALSE);
}
