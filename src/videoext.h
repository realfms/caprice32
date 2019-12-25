/* Caprice32 - Amstrad CPC Emulator
   (c) Copyright 1997-2004 Ulrich Doewich

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

#ifndef VIDEOEXT_H
#define VIDEOEXT_H

#include "cap32.h"
#include "types.h"
#include "SDL.h"
#include "video.h"

SDL_Surface *fbpg_init(video_plugin* t,int w,int h, int bpp,bool fs);
void fbpg_setpal(SDL_Color* c);
bool fbpg_lock();
void fbpg_unlock();
void fbpg_flip();
void fbpg_close();

#endif
