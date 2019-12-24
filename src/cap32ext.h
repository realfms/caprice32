/* Caprice32 - Amstrad CPC Emulator
   (c) Copyright 1997-2005 Ulrich Doewich

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef CAP32EXT_H
#define CAP32EXT_H

#include "cap32.h"
#include "disk.h"
#include "types.h"
#include "keyboard.h"
#include "SDL.h"
#include "video.h"
#include "z80.h"
#include "argparse.h"

#ifdef __cplusplus
// extern "C" is needed so the C++ compiler exports the symbols without name
// manging.
extern "C" {
#endif

typedef void (*videocallback)(struct Cap32Screen *cpcscreen);

bool cap32ext_init(int model, int scr_style, int width, int height, int bpp, bool green, bool fps);
void cap32ext_update();
void cap32ext_finish();
void video_update_callback(videocallback newcallback);

#ifdef __cplusplus
}
#endif

struct Cap32Screen {
	int w, h;
	void *pixels;
};

//Exported cap32.cpp variables
extern char chAppPath[_MAX_PATH + 1];
extern t_CPC CPC;
extern t_CRTC CRTC;
extern t_FDC FDC;
extern t_GateArray GateArray;
extern t_PPI PPI;
extern t_PSG PSG;
extern t_VDU VDU;

extern t_drive driveA;
extern t_drive driveB;

extern std::list<SDL_Event> virtualKeyboardEvents;

extern dword nextVirtualEventFrameCount, dwFrameCountOverall;

extern dword breakPointsToSkipBeforeProceedingWithVirtualEvents;
extern byte keyboard_matrix[16];

extern t_z80regs z80;
extern byte *pbTapeImage;
extern dword dwMF2Flags;

extern dword dwTicks, dwTicksOffset, dwTicksTarget, dwTicksTargetFPS;
extern dword dwFPS, dwFrameCount;
extern dword dwSndBufferCopied;

extern video_plugin* vid_plugin;
extern SDL_Surface *back_surface;
extern dword osd_timing;

extern std::string osd_message;

extern ApplicationWindowState _appWindowState;
extern CapriceArgs args;

extern int joysticks_init();
extern void update_timings();
extern void dumpSnapshot();
extern void set_osd_message(const std::string& message);
extern void cpc_pause();
extern void cpc_resume();
extern void print (byte *pbAddr, const char *pchStr, bool bolColour);
extern void video_display ();
extern void dumpScreen();

#endif
