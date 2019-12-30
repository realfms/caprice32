#include "cap32ext.h"

int main(int argc, char **argv)
{
  cap32ext_loadCPCDefaults();

  CPC.model = 0;
  CPC.scr_style = 10;
  CPC.scr_fs_width = 800;
  CPC.scr_fs_height = 600;
  CPC.scr_fs_bpp = 32;
  CPC.scr_tube = false;
  CPC.scr_fps = true;
  CPC.drvA_file = "";
  cap32ext_checFinalConfig();

  //Init emulator
  cap32ext_init(false);

  while (true) {
    cap32ext_update();
  }

  cap32ext_finish();
}
