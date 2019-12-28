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

/*
   This file includes video filters from the SMS Plus/SDL 
   sega master system emulator :
   (c) Copyright Gregory Montoir
   http://membres.lycos.fr/cyxdown/smssdl/
*/

/*
   This file includes video filters from MAME
   (Multiple Arcade Machine Emulator) :
   (c) Copyright The MAME Team
   http://www.mame.net/
*/
#ifdef WITH_SDL2

#include "video.h"
#include "cap32.h"
#include <math.h>
#include <iostream>

// internal SDL 2 structures
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Texture *texture = nullptr;
SDL_Palette *palette = nullptr;

// the real video surface
SDL_Surface* vid = nullptr;
// the video surface shown by the plugin to the application
static SDL_Surface* pub = nullptr;

extern t_CPC CPC;

#ifndef min
#define min(a,b) ((a)<(b) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) ((a)>(b) ? (a) : (b))
#endif

#define CAP32_SDL2_UPDATETEXTURE(surface) \
  if (surface->format->BitsPerPixel == 8) {\
	SDL_Surface *vidSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0); \
	SDL_UpdateTexture(texture, NULL, vidSurface->pixels, vidSurface->pitch); \
	SDL_FreeSurface(vidSurface); \
  } else { \
	SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch); \
  }

#define CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, w, h) \
  Uint32 rmask, gmask, bmask, amask; \
  int newBpp; \
\
  if (!SDL_PixelFormatEnumToMasks(get_pixel_format(bpp), &newBpp, &rmask, &gmask, &bmask, &amask)) \
		return nullptr; \
  if (bpp == 8) {\
	vid = SDL_CreateRGBSurface(0, w,  h, newBpp, rmask, gmask, bmask, amask); \
	if (!vid) \
		return nullptr; \
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h); \
	if (texture == NULL) \
		return nullptr; \
	palette = SDL_AllocPalette(32); \
	if (palette == NULL) \
		return nullptr; \
	SDL_SetSurfacePalette(vid, palette); \
  } else { \
	vid = SDL_CreateRGBSurface(0, w, h, newBpp, rmask, gmask, bmask, amask); \
	if (!vid) \
		return nullptr; \
	texture = SDL_CreateTexture(renderer, get_pixel_format(bpp), SDL_TEXTUREACCESS_STREAMING, w, h); \
	if (texture == NULL) \
		return nullptr; \
  }

  #define CAP32_SDL2_FREEALL() \
	if (palette != nullptr) { \
		SDL_FreePalette(palette); \
		palette = nullptr; \
	} \
	SDL_DestroyTexture(texture); \
	texture = nullptr; \
	SDL_FreeSurface(vid); \
	vid = nullptr; \
	if (pub != nullptr) { \
		SDL_FreeSurface(pub); \
		pub = nullptr; \
	} \
	SDL_DestroyRenderer(renderer); \
	renderer = nullptr; \
	SDL_DestroyWindow(window); \
	window = nullptr;


Uint32  get_pixel_format(int bpp) {
	switch (bpp) {
		case 8:
			return SDL_PIXELFORMAT_INDEX8;
			break;
		case 16:
			return SDL_PIXELFORMAT_RGB565;
			break;
		case 32:
			return SDL_PIXELFORMAT_ARGB8888;
			break;
		default:
			return SDL_PIXELFORMAT_UNKNOWN;
			break;
	}

}

// computes the clipping of two rectangles and changes src and dst accordingly
// dst is the screen
// src is the internal window
static void compute_rects(SDL_Rect* src, SDL_Rect* dst)
{
	/* initialise the source rect to full source */
	src->x=0;
	src->y=0;
	src->w=pub->w;
	src->h=pub->h;
	
	dst->x=(vid->w-CPC_VISIBLE_SCR_WIDTH*2)/2,
	dst->y=(vid->h-CPC_VISIBLE_SCR_HEIGHT*2)/2;
	dst->w=vid->w;
	dst->h=vid->h;
	
	int dw=src->w*2-dst->w;
	/* the src width is too big */
	if (dw>0)
	{
		src->w-=dw/2;
		src->x+=dw/4;

		dst->x=0;
		dst->w=vid->w;
	}
	else
	{
		dst->w=CPC_VISIBLE_SCR_WIDTH*2;
	}
	int dh=src->h*2-dst->h;
	/* the src height is too big */
	if (dh>0)
	{
		src->h-=dh/2;
		src->y+=dh/4;
		
		dst->y=0;
		dst->h=vid->h;
	}
	else
	{
		src->h-=2*2;
		dst->h=CPC_VISIBLE_SCR_HEIGHT*2;
	}
}

/* ------------------------------------------------------------------------------------ */
/* Half size video plugin ------------------------------------------------------------- */
/* ------------------------------------------------------------------------------------ */
SDL_Surface* half_init(video_plugin* t,int w,int h, int bpp,bool fs)
{
	if (!fs)
	{
		w=CPC_VISIBLE_SCR_WIDTH;
		h=CPC_VISIBLE_SCR_HEIGHT;
	}
	window = SDL_CreateWindow("Caprice32 " VERSION_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, (fs?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
	if (window == NULL)
		return nullptr;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL)
		return nullptr;
	CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, CPC_VISIBLE_SCR_WIDTH, CPC_VISIBLE_SCR_HEIGHT);
	if (fs)
	{
		t->x_scale=1.0;
		t->y_scale=1.0;
		t->x_offset=static_cast<int>((w-CPC_VISIBLE_SCR_WIDTH/t->x_scale)/2);
		t->y_offset=static_cast<int>((h-CPC_VISIBLE_SCR_HEIGHT/t->y_scale)/2);
	}
	else
	{
		t->x_scale=1.0;
		t->y_scale=1.0;
		t->x_offset=0;
		t->y_offset=0;
	}
	SDL_FillRect(vid,nullptr,SDL_MapRGB(vid->format,0,0,0));
	pub=SDL_CreateRGBSurface(SDL_SWSURFACE,CPC_VISIBLE_SCR_WIDTH,CPC_VISIBLE_SCR_HEIGHT,bpp,0,0,0,0);
	return pub;
}

void half_setpal(SDL_Color* c)
{
	SDL_SetPaletteColors(palette, c, 0, 32);
}

bool half_lock()
{
	return true;
}

void half_unlock()
{
}

void half_flip()
{
	SDL_Rect dr;
	dr.x=(vid->w-CPC_VISIBLE_SCR_WIDTH)/2;
	dr.y=(vid->h-CPC_VISIBLE_SCR_HEIGHT)/2;
	dr.w=CPC_VISIBLE_SCR_WIDTH;
	dr.h=CPC_VISIBLE_SCR_HEIGHT;
	SDL_BlitSurface(pub,nullptr,vid,&dr);
	SDL_UpdateTexture(texture, NULL, vid->pixels, vid->pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);}

void half_close()
{
	CAP32_SDL2_FREEALL();
}

/* ------------------------------------------------------------------------------------ */
/* Half size with hardware flip video plugin ------------------------------------------ */
/* ------------------------------------------------------------------------------------ */
SDL_Surface* halfhw_init(video_plugin* t, int w __attribute__((unused)), int h __attribute__((unused)), int bpp, bool fs)
{
	window = SDL_CreateWindow("Caprice32 " VERSION_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CPC_VISIBLE_SCR_WIDTH, CPC_VISIBLE_SCR_HEIGHT, (fs?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
	if (window == NULL)
		return nullptr;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL)
		return nullptr;
	CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, CPC_VISIBLE_SCR_WIDTH, CPC_VISIBLE_SCR_HEIGHT);

	t->x_scale=1.0;
	t->y_scale=1.0;
	t->x_offset=0;
	t->y_offset=0;
	SDL_FillRect(vid,nullptr,SDL_MapRGB(vid->format,0,0,0));
	return vid;
}

void halfhw_setpal(SDL_Color* c)
{
	SDL_SetPaletteColors(palette, c, 0, 32);
}

bool halfhw_lock()
{
	if (SDL_MUSTLOCK(vid))
		return (SDL_LockSurface(vid)==0);
	return true;
}

void halfhw_unlock()
{
	if (SDL_MUSTLOCK(vid))
		SDL_UnlockSurface(vid);
}

void halfhw_flip()
{
  CAP32_SDL2_UPDATETEXTURE(vid);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

void halfhw_close()
{
	CAP32_SDL2_FREEALL();
}
/* ------------------------------------------------------------------------------------ */
/* Double size video plugin ----------------------------------------------------------- */
/* ------------------------------------------------------------------------------------ */
SDL_Surface* double_init(video_plugin* t,int w,int h, int bpp, bool fs)
{
	if (!fs)
	{
		w=CPC_VISIBLE_SCR_WIDTH*2;
		h=CPC_VISIBLE_SCR_HEIGHT*2;
	}
	window = SDL_CreateWindow("Caprice32 " VERSION_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, (fs?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
	if (window == NULL)
		return nullptr;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL)
		return nullptr;
	CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, CPC_VISIBLE_SCR_WIDTH*2, CPC_VISIBLE_SCR_HEIGHT*2);
		
	if (fs)
	{
		t->x_scale=1.0;
		t->y_scale=1.0;
		t->x_offset=static_cast<int>((w-CPC_VISIBLE_SCR_WIDTH*2/t->x_scale)/2);
		t->y_offset=static_cast<int>((h-CPC_VISIBLE_SCR_HEIGHT*2/t->y_scale)/2);
	}
	else
	{
		t->x_scale=1.0;
		t->y_scale=1.0;
		t->x_offset=0;
		t->y_offset=0;
	}
	SDL_FillRect(vid,nullptr,SDL_MapRGB(vid->format,0,0,0));
	pub=SDL_CreateRGBSurface(SDL_SWSURFACE,CPC_VISIBLE_SCR_WIDTH*2,CPC_VISIBLE_SCR_HEIGHT*2,bpp,0,0,0,0);
	return pub;
}

void double_setpal(SDL_Color* c)
{
	SDL_SetPaletteColors(palette, c, 0, 32);
}

bool double_lock()
{
	return true;
}

void double_unlock()
{
}

void double_flip()
{
	SDL_Rect dr;
	dr.x=(vid->w-CPC_VISIBLE_SCR_WIDTH*2)/2;
	dr.y=(vid->h-CPC_VISIBLE_SCR_HEIGHT*2)/2;
	dr.w=CPC_VISIBLE_SCR_WIDTH*2;
	dr.h=CPC_VISIBLE_SCR_HEIGHT*2;
	SDL_BlitSurface(pub,nullptr,vid,&dr);
	SDL_UpdateTexture(texture, NULL, vid->pixels, vid->pitch);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void double_close()
{
	CAP32_SDL2_FREEALL();
}


/* ------------------------------------------------------------------------------------ */
/* Double size with hardware flip video plugin ---------------------------------------- */
/* ------------------------------------------------------------------------------------ */
SDL_Surface* doublehw_init(video_plugin* t, int w __attribute__((unused)), int h __attribute__((unused)), int bpp, bool fs)
{
	window = SDL_CreateWindow("Caprice32 " VERSION_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CPC_VISIBLE_SCR_WIDTH*2, CPC_VISIBLE_SCR_HEIGHT*2, (fs?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
	if (window == NULL)
		return nullptr;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL)
		return nullptr;
	CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, CPC_VISIBLE_SCR_WIDTH*2, CPC_VISIBLE_SCR_HEIGHT*2);

	t->x_scale=1.0;
	t->y_scale=1.0;
	t->x_offset=0;
	t->y_offset=0;
	SDL_FillRect(vid,nullptr,SDL_MapRGB(vid->format,0,0,0));

	return vid;
}

void doublehw_setpal(SDL_Color* c)
{
	SDL_SetPaletteColors(palette, c, 0, 32);
}

bool doublehw_lock()
{
	if (SDL_MUSTLOCK(vid))
		return (SDL_LockSurface(vid)==0);
	return true;
}

void doublehw_unlock()
{
	if (SDL_MUSTLOCK(vid))
		SDL_UnlockSurface(vid);
}

void doublehw_flip()
{
  CAP32_SDL2_UPDATETEXTURE(vid);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

void doublehw_close()
{
	CAP32_SDL2_FREEALL();
}

/* ------------------------------------------------------------------------------------ */
/* Super eagle video plugin ----------------------------------------------------------- */
/* ------------------------------------------------------------------------------------ */

/* 2X SAI Filter */
static Uint32 colorMask = 0xF7DEF7DE;
static Uint32 lowPixelMask = 0x08210821;
static Uint32 qcolorMask = 0xE79CE79C;
static Uint32 qlowpixelMask = 0x18631863;
static Uint32 redblueMask = 0xF81F;
static Uint32 greenMask = 0x7E0;

__inline__ int GetResult1 (Uint32 A, Uint32 B, Uint32 C, Uint32 D)
{
	int x = 0;
	int y = 0;
	int r = 0;

	if (A == C)
		x += 1;
	else if (B == C)
		y += 1;
	if (A == D)
		x += 1;
	else if (B == D)
		y += 1;
	if (x <= 1)
		r += 1;
	if (y <= 1)
		r -= 1;
	return r;
}

__inline__ int GetResult2 (Uint32 A, Uint32 B, Uint32 C, Uint32 D)
{
	int x = 0;
	int y = 0;
	int r = 0;

	if (A == C)
		x += 1;
	else if (B == C)
		y += 1;
	if (A == D)
		x += 1;
	else if (B == D)
		y += 1;
	if (x <= 1)
		r -= 1;
	if (y <= 1)
		r += 1;
	return r;
}

__inline__ int GetResult (Uint32 A, Uint32 B, Uint32 C, Uint32 D)
{
	int x = 0;
	int y = 0;
	int r = 0;

	if (A == C)
		x += 1;
	else if (B == C)
		y += 1;
	if (A == D)
		x += 1;
	else if (B == D)
		y += 1;
	if (x <= 1)
		r += 1;
	if (y <= 1)
		r -= 1;
	return r;
}


__inline__ Uint32 INTERPOLATE (Uint32 A, Uint32 B)
{
	if (A != B)
	{
		return (((A & colorMask) >> 1) + ((B & colorMask) >> 1) +
				(A & B & lowPixelMask));
	}
  return A;
}

__inline__ Uint32 Q_INTERPOLATE (Uint32 A, Uint32 B, Uint32 C, Uint32 D)
{
	Uint32 x = ((A & qcolorMask) >> 2) +
		((B & qcolorMask) >> 2) +
		((C & qcolorMask) >> 2) + ((D & qcolorMask) >> 2);
	Uint32 y = (A & qlowpixelMask) +
		(B & qlowpixelMask) + (C & qlowpixelMask) + (D & qlowpixelMask);
	y = (y >> 2) & qlowpixelMask;
	return x + y;
}

void filter_supereagle(Uint8 *srcPtr, Uint32 srcPitch, /* Uint8 *deltaPtr,  */
		 Uint8 *dstPtr, Uint32 dstPitch, int width, int height)
{
    Uint8  *dP;
    Uint16 *bP;
    Uint32 inc_bP;



	Uint32 finish;
	Uint32 Nextline = srcPitch >> 1;

	inc_bP = 1;

	for (; height ; height--)
	{
	    bP = reinterpret_cast<Uint16 *>(srcPtr);
	    dP = dstPtr;
	    for (finish = width; finish; finish -= inc_bP)
	    {
		Uint32 color4, color5, color6;
		Uint32 color1, color2, color3;
		Uint32 colorA1, colorA2, colorB1, colorB2, colorS1, colorS2;
		Uint32 product1a, product1b, product2a, product2b;
		colorB1 = *(bP - Nextline);
		colorB2 = *(bP - Nextline + 1);

		color4 = *(bP - 1);
		color5 = *(bP);
		color6 = *(bP + 1);
		colorS2 = *(bP + 2);

		color1 = *(bP + Nextline - 1);
		color2 = *(bP + Nextline);
		color3 = *(bP + Nextline + 1);
		colorS1 = *(bP + Nextline + 2);

		colorA1 = *(bP + Nextline + Nextline);
		colorA2 = *(bP + Nextline + Nextline + 1);
		// --------------------------------------
		if (color2 == color6 && color5 != color3)
		{
		    product1b = product2a = color2;
		    if ((color1 == color2) || (color6 == colorB2))
		    {
			product1a = INTERPOLATE (color2, color5);
			product1a = INTERPOLATE (color2, product1a);
//                       product1a = color2;
		    }
		    else
		    {
			product1a = INTERPOLATE (color5, color6);
		    }

		    if ((color6 == colorS2) || (color2 == colorA1))
		    {
			product2b = INTERPOLATE (color2, color3);
			product2b = INTERPOLATE (color2, product2b);
//                       product2b = color2;
		    }
		    else
		    {
			product2b = INTERPOLATE (color2, color3);
		    }
		}
		else if (color5 == color3 && color2 != color6)
		{
		    product2b = product1a = color5;

		    if ((colorB1 == color5) || (color3 == colorS1))
		    {
			product1b = INTERPOLATE (color5, color6);
			product1b = INTERPOLATE (color5, product1b);
//                       product1b = color5;
		    }
		    else
		    {
			product1b = INTERPOLATE (color5, color6);
		    }

		    if ((color3 == colorA2) || (color4 == color5))
		    {
			product2a = INTERPOLATE (color5, color2);
			product2a = INTERPOLATE (color5, product2a);
//                       product2a = color5;
		    }
		    else
		    {
			product2a = INTERPOLATE (color2, color3);
		    }

		}
		else if (color5 == color3 && color2 == color6)
		{
		    int r = 0;

		    r += GetResult (color6, color5, color1, colorA1);
		    r += GetResult (color6, color5, color4, colorB1);
		    r += GetResult (color6, color5, colorA2, colorS1);
		    r += GetResult (color6, color5, colorB2, colorS2);

		    if (r > 0)
		    {
			product1b = product2a = color2;
			product1a = product2b = INTERPOLATE (color5, color6);
		    }
		    else if (r < 0)
		    {
			product2b = product1a = color5;
			product1b = product2a = INTERPOLATE (color5, color6);
		    }
		    else
		    {
			product2b = product1a = color5;
			product1b = product2a = color2;
		    }
		}
		else
		{
		    product2b = product1a = INTERPOLATE (color2, color6);
		    product2b =
			Q_INTERPOLATE (color3, color3, color3, product2b);
		    product1a =
			Q_INTERPOLATE (color5, color5, color5, product1a);

		    product2a = product1b = INTERPOLATE (color5, color3);
		    product2a =
			Q_INTERPOLATE (color2, color2, color2, product2a);
		    product1b =
			Q_INTERPOLATE (color6, color6, color6, product1b);

//                    product1a = color5;
//                    product1b = color6;
//                    product2a = color2;
//                    product2b = color3;
		}
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		product1a = product1a | (product1b << 16);
		product2a = product2a | (product2b << 16);
#else
    product1a = (product1a << 16) | product1b;
    product2a = (product2a << 16) | product2b;
#endif

		*(reinterpret_cast<Uint32 *>(dP)) = product1a;
		*(reinterpret_cast<Uint32 *>(dP + dstPitch)) = product2a;

		bP += inc_bP;
		dP += sizeof (Uint32);
	    }			// end of for ( finish= width etc..)
	    srcPtr += srcPitch;
	    dstPtr += dstPitch * 2;
	}			// endof: for (height; height; height--)
}

SDL_Surface* seagle_init(video_plugin* t,int w,int h, int bpp, bool fs)
{
	if (bpp!=16)
  {
    std::cerr << t->name << " only works in 16 bits color mode - forcing 16 bpp" << std::endl;
    bpp = 16;
  }
	if (!fs)
	{
		w=CPC_VISIBLE_SCR_WIDTH*2;
		h=CPC_VISIBLE_SCR_HEIGHT*2;
	}
	window = SDL_CreateWindow("Caprice32 " VERSION_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, (fs?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
	if (window == NULL)
		return nullptr;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL)
		return nullptr;
	CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, w, h);

	if (vid->format->BitsPerPixel!=16)
  {
    std::cerr << t->name << ": SDL didn't return a 16 bpp surface but a " << static_cast<int>(vid->format->BitsPerPixel) << " bpp one." << std::endl;
		return nullptr;
  }
	if (fs)
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=static_cast<int>(w-CPC_VISIBLE_SCR_WIDTH/t->x_scale)/2;
		t->y_offset=static_cast<int>(h-CPC_VISIBLE_SCR_HEIGHT/t->y_scale)/2;
	}
	else
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=0;
		t->y_offset=0;
	}
	SDL_FillRect(vid,nullptr,SDL_MapRGB(vid->format,0,0,0));
	pub=SDL_CreateRGBSurface(SDL_SWSURFACE,CPC_VISIBLE_SCR_WIDTH,CPC_VISIBLE_SCR_HEIGHT,bpp,0,0,0,0);
	return pub;
}

void seagle_setpal(SDL_Color* c)
{
	SDL_SetPaletteColors(palette, c, 0, 32);
}

bool seagle_lock()
{
	return true;
}

void seagle_unlock()
{
}

void seagle_flip()
{
	if (SDL_MUSTLOCK(vid))
		SDL_LockSurface(vid);
	SDL_Rect src;
	SDL_Rect dst;
	compute_rects(&src,&dst);
	filter_supereagle(static_cast<Uint8*>(pub->pixels) + (2*src.x+src.y*pub->pitch) + (pub->pitch), pub->pitch,
		 static_cast<Uint8*>(vid->pixels) + (2*dst.x+dst.y*vid->pitch), vid->pitch, src.w, src.h);
	SDL_UpdateTexture(texture, NULL, vid->pixels, vid->pitch);
	if (SDL_MUSTLOCK(vid))
		SDL_UnlockSurface(vid);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);}

void seagle_close()
{
	CAP32_SDL2_FREEALL();
}

/* ------------------------------------------------------------------------------------ */
/* Scale2x video plugin --------------------------------------------------------------- */
/* ------------------------------------------------------------------------------------ */
void filter_scale2x(Uint8 *srcPtr, Uint32 srcPitch, 
                      Uint8 *dstPtr, Uint32 dstPitch,
		      int width, int height)
{
	unsigned int nextlineSrc = srcPitch / sizeof(short);
	short *p = reinterpret_cast<short *>(srcPtr);

	unsigned int nextlineDst = dstPitch / sizeof(short);
	short *q = reinterpret_cast<short *>(dstPtr);

	while(height--) {
		int i = 0, j = 0;
		for(i = 0; i < width; ++i, j += 2) {
			short B = *(p + i - nextlineSrc);
			short D = *(p + i - 1);
			short E = *(p + i);
			short F = *(p + i + 1);
			short H = *(p + i + nextlineSrc);

			*(q + j) = D == B && B != F && D != H ? D : E;
			*(q + j + 1) = B == F && B != D && F != H ? F : E;
			*(q + j + nextlineDst) = D == H && D != B && H != F ? D : E;
			*(q + j + nextlineDst + 1) = H == F && D != H && B != F ? F : E;
		}
		p += nextlineSrc;
		q += nextlineDst << 1;
	}
}

SDL_Surface* scale2x_init(video_plugin* t,int w,int h, int bpp, bool fs)
{
	if (bpp!=16)
  {
    std::cerr << t->name << " only works in 16 bits color mode - forcing 16 bpp" << std::endl;
    bpp = 16;
  }
	if (!fs)
	{
		w=CPC_VISIBLE_SCR_WIDTH*2;
		h=CPC_VISIBLE_SCR_HEIGHT*2;
	}

	window = SDL_CreateWindow("Caprice32 " VERSION_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, (fs?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
	if (window == NULL)
		return nullptr;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL)
		return nullptr;
	CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, w, h);

	if (vid->format->BitsPerPixel!=16)
  {
    std::cerr << t->name << ": SDL didn't return a 16 bpp surface but a " << static_cast<int>(vid->format->BitsPerPixel) << " bpp one." << std::endl;
		return nullptr;
  }
	if (fs)
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=static_cast<int>(w-CPC_VISIBLE_SCR_WIDTH/t->x_scale)/2;
		t->y_offset=static_cast<int>(h-CPC_VISIBLE_SCR_HEIGHT/t->y_scale)/2;
	}
	else
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=0;
		t->y_offset=0;
	}
	SDL_FillRect(vid,nullptr,SDL_MapRGB(vid->format,0,0,0));
	pub=SDL_CreateRGBSurface(SDL_SWSURFACE,CPC_VISIBLE_SCR_WIDTH,CPC_VISIBLE_SCR_HEIGHT,bpp,0,0,0,0);
	return pub;
}

void scale2x_setpal(SDL_Color* c)
{
	SDL_SetPaletteColors(palette, c, 0, 32);
}

bool scale2x_lock()
{
	return true;
}

void scale2x_unlock()
{
}

void scale2x_flip()
{
	if (SDL_MUSTLOCK(vid))
		SDL_LockSurface(vid);
	SDL_Rect src;
	SDL_Rect dst;
	compute_rects(&src,&dst);
	filter_scale2x(static_cast<Uint8*>(pub->pixels) + (2*src.x+src.y*pub->pitch) + (pub->pitch), pub->pitch,
		 static_cast<Uint8*>(vid->pixels) + (2*dst.x+dst.y*vid->pitch), vid->pitch, src.w, src.h);
	SDL_UpdateTexture(texture, NULL, vid->pixels, vid->pitch);
	if (SDL_MUSTLOCK(vid))
		SDL_UnlockSurface(vid);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void scale2x_close()
{
	CAP32_SDL2_FREEALL();
}

/* ------------------------------------------------------------------------------------ */
/* ascale2x video plugin --------------------------------------------------------------- */
/* ------------------------------------------------------------------------------------ */
void filter_ascale2x (Uint8 *srcPtr, Uint32 srcPitch,
	     Uint8 *dstPtr, Uint32 dstPitch, int width, int height)
{
    Uint8  *dP;
    Uint16 *bP;
    Uint32 inc_bP;


	Uint32 finish;
	Uint32 Nextline = srcPitch >> 1;
	inc_bP = 1;


	for (; height; height--)
	{
	    bP = reinterpret_cast<Uint16 *>(srcPtr);
	    dP = dstPtr;

	    for (finish = width; finish; finish -= inc_bP)
	    {

		Uint32 colorA, colorB;
		Uint32 colorC, colorD,
		    colorE, colorF, colorG, colorH,
		    colorI, colorJ, colorK, colorL,

		    colorM, colorN, colorO;
		Uint32 product, product1, product2;

//---------------------------------------
// Map of the pixels:                    I|E F|J
//                                       G|A B|K
//                                       H|C D|L
//                                       M|N O|P
		colorI = *(bP - Nextline - 1);
		colorE = *(bP - Nextline);
		colorF = *(bP - Nextline + 1);
		colorJ = *(bP - Nextline + 2);

		colorG = *(bP - 1);
		colorA = *(bP);
		colorB = *(bP + 1);
		colorK = *(bP + 2);

		colorH = *(bP + Nextline - 1);
		colorC = *(bP + Nextline);
		colorD = *(bP + Nextline + 1);
		colorL = *(bP + Nextline + 2);

		colorM = *(bP + Nextline + Nextline - 1);
		colorN = *(bP + Nextline + Nextline);
		colorO = *(bP + Nextline + Nextline + 1);

		if ((colorA == colorD) && (colorB != colorC))
		{
		    if (((colorA == colorE) && (colorB == colorL)) ||
			    ((colorA == colorC) && (colorA == colorF)
			     && (colorB != colorE) && (colorB == colorJ)))
		    {
			product = colorA;
		    }
		    else
		    {
			product = INTERPOLATE (colorA, colorB);
		    }

		    if (((colorA == colorG) && (colorC == colorO)) ||
			    ((colorA == colorB) && (colorA == colorH)
			     && (colorG != colorC) && (colorC == colorM)))
		    {
			product1 = colorA;
		    }
		    else
		    {
			product1 = INTERPOLATE (colorA, colorC);
		    }
		    product2 = colorA;
		}
		else if ((colorB == colorC) && (colorA != colorD))
		{
		    if (((colorB == colorF) && (colorA == colorH)) ||
			    ((colorB == colorE) && (colorB == colorD)
			     && (colorA != colorF) && (colorA == colorI)))
		    {
			product = colorB;
		    }
		    else
		    {
			product = INTERPOLATE (colorA, colorB);
		    }

		    if (((colorC == colorH) && (colorA == colorF)) ||
			    ((colorC == colorG) && (colorC == colorD)
			     && (colorA != colorH) && (colorA == colorI)))
		    {
			product1 = colorC;
		    }
		    else
		    {
			product1 = INTERPOLATE (colorA, colorC);
		    }
		    product2 = colorB;
		}
		else if ((colorA == colorD) && (colorB == colorC))
		{
		    if (colorA == colorB)
		    {
			product = colorA;
			product1 = colorA;
			product2 = colorA;
		    }
		    else
		    {
			int r = 0;

			product1 = INTERPOLATE (colorA, colorC);
			product = INTERPOLATE (colorA, colorB);

			r += GetResult1 (colorA, colorB, colorG, colorE);
			r += GetResult2 (colorB, colorA, colorK, colorF);
			r += GetResult2 (colorB, colorA, colorH, colorN);
			r += GetResult1 (colorA, colorB, colorL, colorO);

			if (r > 0)
			    product2 = colorA;
			else if (r < 0)
			    product2 = colorB;
			else
			{
			    product2 =
				Q_INTERPOLATE (colorA, colorB, colorC,
					       colorD);
			}
		    }
		}
		else
		{
		    product2 = Q_INTERPOLATE (colorA, colorB, colorC, colorD);

		    if ((colorA == colorC) && (colorA == colorF)
			    && (colorB != colorE) && (colorB == colorJ))
		    {
			product = colorA;
		    }
		    else
			if ((colorB == colorE) && (colorB == colorD)
			    && (colorA != colorF) && (colorA == colorI))
		    {
			product = colorB;
		    }
		    else
		    {
			product = INTERPOLATE (colorA, colorB);
		    }

		    if ((colorA == colorB) && (colorA == colorH)
			    && (colorG != colorC) && (colorC == colorM))
		    {
			product1 = colorA;
		    }
		    else
			if ((colorC == colorG) && (colorC == colorD)
			    && (colorA != colorH) && (colorA == colorI))
		    {
			product1 = colorC;
		    }
		    else
		    {
			product1 = INTERPOLATE (colorA, colorC);
		    }
		}
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		product = colorA | (product << 16);
		product1 = product1 | (product2 << 16);
#else
    product = (colorA << 16) | product;
    product1 = (product1 << 16) | product2;
#endif
		*(reinterpret_cast<Uint32 *>(dP)) = product;
		*(reinterpret_cast<Uint32 *>(dP + dstPitch)) = product1;

		bP += inc_bP;
		dP += sizeof (Uint32);
	    }			// end of for ( finish= width etc..)

	    srcPtr += srcPitch;
	    dstPtr += dstPitch * 2;
	}			// endof: for (height; height; height--)
}



SDL_Surface* ascale2x_init(video_plugin* t,int w,int h, int bpp, bool fs)
{
	if (bpp!=16)
  {
    std::cerr << t->name << " only works in 16 bits color mode - forcing 16 bpp" << std::endl;
    bpp = 16;
  }
	if (!fs)
	{
		w=CPC_VISIBLE_SCR_WIDTH*2;
		h=CPC_VISIBLE_SCR_HEIGHT*2;
	}
	window = SDL_CreateWindow("Caprice32 " VERSION_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, (fs?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
	if (window == NULL)
		return nullptr;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL)
		return nullptr;
	CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, w, h);

	if (vid->format->BitsPerPixel!=16)
  {
    std::cerr << t->name << ": SDL didn't return a 16 bpp surface but a " << static_cast<int>(vid->format->BitsPerPixel) << " bpp one." << std::endl;
		return nullptr;
  }
	if (fs)
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=static_cast<int>(w-CPC_VISIBLE_SCR_WIDTH/t->x_scale)/2;
		t->y_offset=static_cast<int>(h-CPC_VISIBLE_SCR_HEIGHT/t->y_scale)/2;
	}
	else
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=0;
		t->y_offset=0;
	}
	SDL_FillRect(vid,nullptr,SDL_MapRGB(vid->format,0,0,0));
	pub=SDL_CreateRGBSurface(SDL_SWSURFACE,CPC_VISIBLE_SCR_WIDTH,CPC_VISIBLE_SCR_HEIGHT,bpp,0,0,0,0);
	return pub;
}

void ascale2x_setpal(SDL_Color* c)
{
	SDL_SetPaletteColors(palette, c, 0, 32);
}

bool ascale2x_lock()
{
	return true;
}

void ascale2x_unlock()
{
}

void ascale2x_flip()
{
	if (SDL_MUSTLOCK(vid))
		SDL_LockSurface(vid);
	SDL_Rect src;
	SDL_Rect dst;
	compute_rects(&src,&dst);
	filter_ascale2x(static_cast<Uint8*>(pub->pixels) + (2*src.x+src.y*pub->pitch) + (pub->pitch), pub->pitch,
		 static_cast<Uint8*>(vid->pixels) + (2*dst.x+dst.y*vid->pitch), vid->pitch, src.w, src.h);
	SDL_UpdateTexture(texture, NULL, vid->pixels, vid->pitch);
	if (SDL_MUSTLOCK(vid))
		SDL_UnlockSurface(vid);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void ascale2x_close()
{
	CAP32_SDL2_FREEALL();
}


/* ------------------------------------------------------------------------------------ */
/* tv2x video plugin ------------------------------------------------------------------ */
/* ------------------------------------------------------------------------------------ */
void filter_tv2x(Uint8 *srcPtr, Uint32 srcPitch, 
		Uint8 *dstPtr, Uint32 dstPitch, 
		int width, int height)
{
	unsigned int nextlineSrc = srcPitch / sizeof(Uint16);
	Uint16 *p = reinterpret_cast<Uint16 *>(srcPtr);

	unsigned int nextlineDst = dstPitch / sizeof(Uint16);
	Uint16 *q = reinterpret_cast<Uint16 *>(dstPtr);

	while(height--) {
		int i = 0, j = 0;
		for(; i < width; ++i, j += 2) {
			Uint16 p1 = *(p + i);
			Uint32 pi;

			pi = (((p1 & redblueMask) * 7) >> 3) & redblueMask;
			pi |= (((p1 & greenMask) * 7) >> 3) & greenMask;

			*(q + j) = p1;
			*(q + j + 1) = p1;
			*(q + j + nextlineDst) = pi;
			*(q + j + nextlineDst + 1) = pi;
		}
		p += nextlineSrc;
		q += nextlineDst << 1;
	}
}

SDL_Surface* tv2x_init(video_plugin* t,int w,int h, int bpp, bool fs)
{
	if (bpp!=16)
  {
    std::cerr << t->name << " only works in 16 bits color mode - forcing 16 bpp" << std::endl;
    bpp = 16;
  }
	if (!fs)
	{
		w=CPC_VISIBLE_SCR_WIDTH*2;
		h=CPC_VISIBLE_SCR_HEIGHT*2;
	}
	window = SDL_CreateWindow("Caprice32 " VERSION_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, (fs?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
	if (window == NULL)
		return nullptr;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL)
		return nullptr;
	CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, w, h);
	if (vid->format->BitsPerPixel!=16)
  {
    std::cerr << t->name << ": SDL didn't return a 16 bpp surface but a " << static_cast<int>(vid->format->BitsPerPixel) << " bpp one." << std::endl;
		return nullptr;
  }
	if (fs)
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=static_cast<int>(w-CPC_VISIBLE_SCR_WIDTH/t->x_scale)/2;
		t->y_offset=static_cast<int>(h-CPC_VISIBLE_SCR_HEIGHT/t->y_scale)/2;
	}
	else
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=0;
		t->y_offset=0;
	}
	SDL_FillRect(vid,nullptr,SDL_MapRGB(vid->format,0,0,0));
	pub=SDL_CreateRGBSurface(SDL_SWSURFACE,CPC_VISIBLE_SCR_WIDTH,CPC_VISIBLE_SCR_HEIGHT,bpp,0,0,0,0);
	return pub;
}

void tv2x_setpal(SDL_Color* c)
{
	SDL_SetPaletteColors(palette, c, 0, 32);
}

bool tv2x_lock()
{
	return true;
}

void tv2x_unlock()
{
}

void tv2x_flip()
{
	if (SDL_MUSTLOCK(vid))
		SDL_LockSurface(vid);
	SDL_Rect src;
	SDL_Rect dst;
	compute_rects(&src,&dst);
	filter_tv2x(static_cast<Uint8*>(pub->pixels) + (2*src.x+src.y*pub->pitch) + (pub->pitch), pub->pitch,
		 static_cast<Uint8*>(vid->pixels) + (2*dst.x+dst.y*vid->pitch), vid->pitch, src.w, src.h);
	SDL_UpdateTexture(texture, NULL, vid->pixels, vid->pitch);
	if (SDL_MUSTLOCK(vid))
		SDL_UnlockSurface(vid);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void tv2x_close()
{
	CAP32_SDL2_FREEALL();
}

/* ------------------------------------------------------------------------------------ */
/* Software bilinear video plugin ----------------------------------------------------- */
/* ------------------------------------------------------------------------------------ */
void filter_bilinear(Uint8 *srcPtr, Uint32 srcPitch, 
		Uint8 *dstPtr, Uint32 dstPitch, 
		int width, int height)
{
	unsigned int nextlineSrc = srcPitch / sizeof(Uint16);
	Uint16 *p = reinterpret_cast<Uint16 *>(srcPtr);
	unsigned int nextlineDst = dstPitch / sizeof(Uint16);
	Uint16 *q = reinterpret_cast<Uint16 *>(dstPtr);

	while(height--) {
		int i, ii;
		for(i = 0, ii = 0; i < width; ++i, ii += 2) {
			Uint16 A = *(p + i);
			Uint16 B = *(p + i + 1);
			Uint16 C = *(p + i + nextlineSrc);
			Uint16 D = *(p + i + nextlineSrc + 1);
			*(q + ii) = A;
			*(q + ii + 1) = INTERPOLATE(A, B);
			*(q + ii + nextlineDst) = INTERPOLATE(A, C);
			*(q + ii + nextlineDst + 1) = Q_INTERPOLATE(A, B, C, D);
		}
		p += nextlineSrc;
		q += nextlineDst << 1;
	}
}

SDL_Surface* swbilin_init(video_plugin* t,int w,int h, int bpp, bool fs)
{
	if (bpp!=16)
  {
    std::cerr << t->name << " only works in 16 bits color mode - forcing 16 bpp" << std::endl;
    bpp = 16;
  }
	if (!fs)
	{
		w=CPC_VISIBLE_SCR_WIDTH*2;
		h=CPC_VISIBLE_SCR_HEIGHT*2;
	}
	window = SDL_CreateWindow("Caprice32 " VERSION_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, (fs?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
	if (window == NULL)
		return nullptr;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL)
		return nullptr;
	CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, w, h);
	if (vid->format->BitsPerPixel!=16)
  {
    std::cerr << t->name << ": SDL didn't return a 16 bpp surface but a " << static_cast<int>(vid->format->BitsPerPixel) << " bpp one." << std::endl;
		return nullptr;
  }
	if (fs)
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=static_cast<int>(w-CPC_VISIBLE_SCR_WIDTH/t->x_scale)/2;
		t->y_offset=static_cast<int>(h-CPC_VISIBLE_SCR_HEIGHT/t->y_scale)/2;
	}
	else
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=0;
		t->y_offset=0;
	}
	SDL_FillRect(vid,nullptr,SDL_MapRGB(vid->format,0,0,0));
	pub=SDL_CreateRGBSurface(SDL_SWSURFACE,CPC_VISIBLE_SCR_WIDTH,CPC_VISIBLE_SCR_HEIGHT,bpp,0,0,0,0);
	return pub;
}

void swbilin_setpal(SDL_Color* c)
{
	SDL_SetPaletteColors(palette, c, 0, 32);
}

bool swbilin_lock()
{
	return true;
}

void swbilin_unlock()
{
}

void swbilin_flip()
{
	if (SDL_MUSTLOCK(vid))
		SDL_LockSurface(vid);
	SDL_Rect src;
	SDL_Rect dst;
	compute_rects(&src,&dst);
	filter_bilinear(static_cast<Uint8*>(pub->pixels) + (2*src.x+src.y*pub->pitch) + (pub->pitch), pub->pitch,
		 static_cast<Uint8*>(vid->pixels) + (2*dst.x+dst.y*vid->pitch), vid->pitch, src.w, src.h);
	SDL_UpdateTexture(texture, NULL, vid->pixels, vid->pitch);
	if (SDL_MUSTLOCK(vid))
		SDL_UnlockSurface(vid);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void swbilin_close()
{
	CAP32_SDL2_FREEALL();
}

/* ------------------------------------------------------------------------------------ */
/* Software bicubic video plugin ------------------------------------------------------ */
/* ------------------------------------------------------------------------------------ */
#define BLUE_MASK565 0x001F001F
#define RED_MASK565 0xF800F800
#define GREEN_MASK565 0x07E007E0

#define BLUE_MASK555 0x001F001F
#define RED_MASK555 0x7C007C00
#define GREEN_MASK555 0x03E003E0

__inline__ static void MULT(Uint16 c, float* r, float* g, float* b, float alpha) {
  *r += alpha * ((c & RED_MASK565  ) >> 11);
  *g += alpha * ((c & GREEN_MASK565) >>  5);
  *b += alpha * ((c & BLUE_MASK565 ) >>  0);
}

__inline__ static Uint16 MAKE_RGB565(float r, float g, float b) {
  return 
  (((static_cast<Uint8>(r)) << 11) & RED_MASK565  ) |
  (((static_cast<Uint8>(g)) <<  5) & GREEN_MASK565) |
  (((static_cast<Uint8>(b)) <<  0) & BLUE_MASK565 );
}

__inline__ float CUBIC_WEIGHT(float x) {
  // P(x) = { x, x>0 | 0, x<=0 }
  // P(x + 2) ^ 3 - 4 * P(x + 1) ^ 3 + 6 * P(x) ^ 3 - 4 * P(x - 1) ^ 3
  double r = 0.;
  if(x + 2 > 0) r +=      pow(x + 2, 3);
  if(x + 1 > 0) r += -4 * pow(x + 1, 3);
  if(x     > 0) r +=  6 * pow(x    , 3);
  if(x - 1 > 0) r += -4 * pow(x - 1, 3);
  return static_cast<float>(r) / 6;
}

void filter_bicubic(Uint8 *srcPtr, Uint32 srcPitch, 
                    Uint8 *dstPtr, Uint32 dstPitch, 
                    int width, int height)
{
  unsigned int nextlineSrc = srcPitch / sizeof(Uint16);
  Uint16 *p = reinterpret_cast<Uint16 *>(srcPtr);
  unsigned int nextlineDst = dstPitch / sizeof(Uint16);
  Uint16 *q = reinterpret_cast<Uint16 *>(dstPtr);
  int dx = width << 1, dy = height << 1;
  float fsx = static_cast<float>(width) / dx;
	float fsy = static_cast<float>(height) / dy;
	float v = 0.0f;
	int j = 0;
	for(; j < dy; ++j) {
	  float u = 0.0f;
	  int iv = static_cast<int>(v);
    float decy = v - iv;
    int i = 0;
	  for(; i < dx; ++i) {
		  int iu = static_cast<int>(u);
		  float decx = u - iu;
      float r, g, b;
      int m;
      r = g = b = 0.;
      for(m = -1; m <= 2; ++m) {
        float r1 = CUBIC_WEIGHT(decy - m);
        int n;
        for(n = -1; n <= 2; ++n) {
          float r2 = CUBIC_WEIGHT(n - decx);
          Uint16* pIn = p + (iu  + n) + (iv + m) * nextlineSrc;
          MULT(*pIn, &r, &g, &b, r1 * r2);
        }
      }
      *(q + i) = MAKE_RGB565(r, g, b);
      u += fsx;
	  }
    q += nextlineDst;
	  v += fsy;
  }
}

SDL_Surface* swbicub_init(video_plugin* t,int w,int h, int bpp, bool fs)
{
	if (bpp!=16)
  {
    std::cerr << t->name << " only works in 16 bits color mode - forcing 16 bpp" << std::endl;
    bpp = 16;
  }
	if (!fs)
	{
		w=CPC_VISIBLE_SCR_WIDTH*2;
		h=CPC_VISIBLE_SCR_HEIGHT*2;
	}
	window = SDL_CreateWindow("Caprice32 " VERSION_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, (fs?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
	if (window == NULL)
		return nullptr;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL)
		return nullptr;
	CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, w, h);
	if (vid->format->BitsPerPixel!=16)
  {
    std::cerr << t->name << ": SDL didn't return a 16 bpp surface but a " << static_cast<int>(vid->format->BitsPerPixel) << " bpp one." << std::endl;
		return nullptr;
  }
	if (fs)
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=static_cast<int>(w-CPC_VISIBLE_SCR_WIDTH/t->x_scale)/2;
		t->y_offset=static_cast<int>(h-CPC_VISIBLE_SCR_HEIGHT/t->y_scale)/2;
	}
	else
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=0;
		t->y_offset=0;
	}
	SDL_FillRect(vid,nullptr,SDL_MapRGB(vid->format,0,0,0));
	pub=SDL_CreateRGBSurface(SDL_SWSURFACE,CPC_VISIBLE_SCR_WIDTH,CPC_VISIBLE_SCR_HEIGHT,bpp,0,0,0,0);
	return pub;
}

void swbicub_setpal(SDL_Color* c)
{
	SDL_SetPaletteColors(palette, c, 0, 32);
}

bool swbicub_lock()
{
	return true;
}

void swbicub_unlock()
{
}

void swbicub_flip()
{
	if (SDL_MUSTLOCK(vid))
		SDL_LockSurface(vid);
	SDL_Rect src;
	SDL_Rect dst;
	compute_rects(&src,&dst);
	filter_bicubic(static_cast<Uint8*>(pub->pixels) + (2*src.x+src.y*pub->pitch) + (pub->pitch), pub->pitch,
		 static_cast<Uint8*>(vid->pixels) + (2*dst.x+dst.y*vid->pitch), vid->pitch, src.w, src.h);
	SDL_UpdateTexture(texture, NULL, vid->pixels, vid->pitch);
	if (SDL_MUSTLOCK(vid))
		SDL_UnlockSurface(vid);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void swbicub_close()
{
	CAP32_SDL2_FREEALL();
}


/* ------------------------------------------------------------------------------------ */
/* Dot matrix video plugin ------------------------------------------------------------ */
/* ------------------------------------------------------------------------------------ */
static Uint16 DOT_16(Uint16 c, int j, int i) {
  static const Uint16 dotmatrix[16] = {
	  0x01E0, 0x0007, 0x3800, 0x0000,
	  0x39E7, 0x0000, 0x39E7, 0x0000,
	  0x3800, 0x0000, 0x01E0, 0x0007,
	  0x39E7, 0x0000, 0x39E7, 0x0000
  };
  return c - ((c >> 2) & *(dotmatrix + ((j & 3) << 2) + (i & 3)));
}

void filter_dotmatrix(Uint8 *srcPtr, Uint32 srcPitch, 
		Uint8 *dstPtr, Uint32 dstPitch,
		int width, int height)
{
	unsigned int nextlineSrc = srcPitch / sizeof(Uint16);
	Uint16 *p = reinterpret_cast<Uint16 *>(srcPtr);

	unsigned int nextlineDst = dstPitch / sizeof(Uint16);
	Uint16 *q = reinterpret_cast<Uint16 *>(dstPtr);

	int i, ii, j, jj;
	for(j = 0, jj = 0; j < height; ++j, jj += 2) {
		for(i = 0, ii = 0; i < width; ++i, ii += 2) {
			Uint16 c = *(p + i);
			*(q + ii) = DOT_16(c, jj, ii);
			*(q + ii + 1) = DOT_16(c, jj, ii + 1);
			*(q + ii + nextlineDst) = DOT_16(c, jj + 1, ii);
			*(q + ii + nextlineDst + 1) = DOT_16(c, jj + 1, ii + 1);
		}
		p += nextlineSrc;
		q += nextlineDst << 1;
	}
}

SDL_Surface* dotmat_init(video_plugin* t,int w,int h, int bpp, bool fs)
{
	if (bpp!=16)
  {
    std::cerr << t->name << " only works in 16 bits color mode - forcing 16 bpp" << std::endl;
    bpp = 16;
  }
	if (!fs)
	{
		w=CPC_VISIBLE_SCR_WIDTH*2;
		h=CPC_VISIBLE_SCR_HEIGHT*2;
	}
	window = SDL_CreateWindow("Caprice32 " VERSION_STRING, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, (fs?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
	if (window == NULL)
		return nullptr;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (renderer == NULL)
		return nullptr;
	CAP32_SDL2_PREPARE_SURFACE_TEXTURE(bpp, w, h);
	if (vid->format->BitsPerPixel!=16)
  {
    std::cerr << t->name << ": SDL didn't return a 16 bpp surface but a " << static_cast<int>(vid->format->BitsPerPixel) << " bpp one." << std::endl;
		return nullptr;
  }
	if (fs)
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=static_cast<int>(w-CPC_VISIBLE_SCR_WIDTH/t->x_scale)/2;
		t->y_offset=static_cast<int>(h-CPC_VISIBLE_SCR_HEIGHT/t->y_scale)/2;
	}
	else
	{
		t->x_scale=0.5;
		t->y_scale=0.5;
		t->x_offset=0;
		t->y_offset=0;
	}
	SDL_FillRect(vid,nullptr,SDL_MapRGB(vid->format,0,0,0));
	pub=SDL_CreateRGBSurface(SDL_SWSURFACE,CPC_VISIBLE_SCR_WIDTH,CPC_VISIBLE_SCR_HEIGHT,bpp,0,0,0,0);
	return pub;
}

void dotmat_setpal(SDL_Color* c)
{
	SDL_SetPaletteColors(palette, c, 0, 32);
}

bool dotmat_lock()
{
	return true;
}

void dotmat_unlock()
{
}

void dotmat_flip()
{
	if (SDL_MUSTLOCK(vid))
		SDL_LockSurface(vid);
	SDL_Rect src;
	SDL_Rect dst;
	compute_rects(&src,&dst);
	filter_dotmatrix(static_cast<Uint8*>(pub->pixels) + (2*src.x+src.y*pub->pitch) + (pub->pitch), pub->pitch,
		 static_cast<Uint8*>(vid->pixels) + (2*dst.x+dst.y*vid->pitch), vid->pitch, src.w, src.h);
	SDL_UpdateTexture(texture, NULL, vid->pixels, vid->pitch);
	if (SDL_MUSTLOCK(vid))
		SDL_UnlockSurface(vid);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void dotmat_close()
{
	CAP32_SDL2_FREEALL();
}


/* ------------------------------------------------------------------------------------ */
/* End of video plugins --------------------------------------------------------------- */
/* ------------------------------------------------------------------------------------ */

video_plugin video_plugin_list[]=
{
/* Name                            Init func      Palette func     Lock func      Unlock func      Flip func      Close func      Pixel formats  Half size  X, Y offsets   X, Y scale  */
{"Half size with hardware flip",   halfhw_init,   halfhw_setpal,   halfhw_lock,   halfhw_unlock,   halfhw_flip,   halfhw_close,   ALL,           1,         0, 0,          0, 0   },
{"Double size with hardware flip", doublehw_init, doublehw_setpal, doublehw_lock, doublehw_unlock, doublehw_flip, doublehw_close, ALL,           0,         0, 0,          0, 0   },
{"Half size",                      half_init,     half_setpal,     half_lock,     half_unlock,     half_flip,     half_close,     ALL,           1,         0, 0,          0, 0   },
{"Double size",                    double_init,   double_setpal,   double_lock,   double_unlock,   double_flip,   double_close,   ALL,           0,         0, 0,          0, 0   },
{"Super eagle",                    seagle_init,   seagle_setpal,   seagle_lock,   seagle_unlock,   seagle_flip,   seagle_close,   F16_BPP,       1,         0, 0,          0, 0   },
{"Scale2x",                        scale2x_init,  scale2x_setpal,  scale2x_lock,  scale2x_unlock,  scale2x_flip,  scale2x_close,  F16_BPP,       1,         0, 0,          0, 0   },
{"Advanced Scale2x",               ascale2x_init, ascale2x_setpal, ascale2x_lock, ascale2x_unlock, ascale2x_flip, ascale2x_close, F16_BPP,       1,         0, 0,          0, 0   },
{"TV 2x",                          tv2x_init,     tv2x_setpal,     tv2x_lock,     tv2x_unlock,     tv2x_flip,     tv2x_close,     F16_BPP,       1,         0, 0,          0, 0   },
{"Software bilinear",              swbilin_init,  swbilin_setpal,  swbilin_lock,  swbilin_unlock,  swbilin_flip,  swbilin_close,  F16_BPP,       1,         0, 0,          0, 0   },
{"Software bicubic",               swbicub_init,  swbicub_setpal,  swbicub_lock,  swbicub_unlock,  swbicub_flip,  swbicub_close,  F16_BPP,       1,         0, 0,          0, 0   },
{"Dot matrix",                     dotmat_init,   dotmat_setpal,   dotmat_lock,   dotmat_unlock,   dotmat_flip,   dotmat_close,   F16_BPP,       1,         0, 0,          0, 0   },
{nullptr,                          nullptr,       nullptr,         nullptr,       nullptr,         nullptr,       nullptr,        0,             0,         0, 0,          0, 0   }
};

unsigned int nb_video_plugins = sizeof(video_plugin_list)/sizeof(video_plugin_list[0])-1;

#endif