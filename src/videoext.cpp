#include <iostream>
#include "videoext.h"
#include "crtc.h"
#include "errors.h"
#include "cap32.h"
#include "video.h"
#include "SDL.h"

SDL_Surface* fbvid = nullptr;

/* ------------------------------------------------------------------------------------ */
/* Frambuffer video plugin ------------------------------------------------------------- */
/* ------------------------------------------------------------------------------------ */
SDL_Surface *fbpg_init(video_plugin* t,int w,int h, int bpp,bool fs)
{
    w=CPC_VISIBLE_SCR_WIDTH*2;
	h=CPC_VISIBLE_SCR_HEIGHT*2;

    /* Allocate the surface */
    fbvid = new SDL_Surface{};

    fbvid->format = new SDL_PixelFormat{};
    fbvid->format -> palette = NULL;
    fbvid->format->BitsPerPixel = 32;
    fbvid->format->BytesPerPixel = 4;
    fbvid->format->Rloss = 0;
    fbvid->format->Gloss = 0;
    fbvid->format->Bloss = 0;
    fbvid->format->Aloss = 0;
    fbvid->format->Rshift = 16;
    fbvid->format->Gshift = 8;
    fbvid->format->Bshift = 0;
    fbvid->format->Ashift = 0;
    fbvid->format->Rmask = 0xFF0000;
    fbvid->format->Gmask = 0xFF00;
    fbvid->format->Bmask = 0xFF;
    fbvid->format->Amask = 0x0;
    fbvid->format->colorkey = 0;
    fbvid->format->alpha = 0xFF;
    fbvid->w = w;
    fbvid->h = h;
    fbvid->pitch = fbvid->w * fbvid->format->BytesPerPixel;
    fbvid->clip_rect.x = 0;
    fbvid->clip_rect.y = 0;
    fbvid->clip_rect.w = fbvid->w;
    fbvid->clip_rect.h = fbvid->h;

    if (fbvid->w && fbvid->h) {
        Sint64 size = ((Sint64)fbvid->h * fbvid->pitch);
        fbvid->pixels = new byte[size];
        memset(fbvid->pixels, 0, fbvid->h * fbvid->pitch);
    }

    fbvid->refcount = 1;

	t->x_scale=1.0;
	t->y_scale=1.0;
	t->x_offset=0;
	t->y_offset=0;

	return fbvid;
}

void fbpg_setpal(SDL_Color* c)
{
}

bool fbpg_lock()
{
	return true;
}

void fbpg_unlock()
{
}

void fbpg_flip()
{
}

void fbpg_close()
{
    if (fbvid == NULL)
        return;
    if (--fbvid->refcount > 0) {
        return;
    }
    if (fbvid->format) {
        delete fbvid->format;
        fbvid->format = NULL;
    }
    if (fbvid->pixels) {
        delete fbvid->pixels;
        fbvid->pixels = NULL;
    }
};
