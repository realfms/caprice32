#include "cap32ext.h"

int main(int argc, char **argv)
{
  //Init emulator
  cap32ext_init(0, 1, 800, 600, 32, false, true, "");

  while (true) {
    cap32ext_update();
  }

  cap32ext_finish();
}
