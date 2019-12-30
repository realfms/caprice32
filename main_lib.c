#include "cap32ext.h"

int main(int argc, char **argv)
{
  SDL_Event event;
  bool doWork = true;
  
  cap32ext_loadCPCDefaults();

  CPC.model = 0;
  CPC.scr_style = 1;
  CPC.scr_fs_width = 800;
  CPC.scr_fs_height = 600;
  CPC.scr_fs_bpp = 32;
  CPC.scr_tube = false;
  CPC.scr_fps = false;
  CPC.drvA_file = "disk/BatmanForever.zip";
  CPC.kbd_layout = "keymap_es_linux.map";
  CPC.keyboard = 2;
  cap32ext_checFinalConfig();

  //Init emulator
  cap32ext_init(false);

  while (doWork) {
    if (SDL_PollEvent(&event)) {
      fprintf(stderr, "Received event %X\n", event.type);
      switch (event.type)
      {
      case SDL_QUIT:
         cleanExit(0);

      default:
        doWork = cap32ext_sendevent(event);

        break;
      }
    }
    cap32ext_update();
  }

  cap32ext_finish();
}
