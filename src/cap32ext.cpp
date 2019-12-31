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

#include "cap32ext.h"
#include "slotshandler.h"
#include "log.h"
#include "tape.h"
#include "asic.h"
#include <chrono>
#include <thread>

int iExitCondition;
std::string tmpPath;


bool cap32ext_init(bool loadConfig, std::string tmpFolder)
{
    std::vector<std::string> slot_list;

   if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE) < 0) { // initialize SDL
      fprintf(stderr, "SDL_Init() failed: %s\n", SDL_GetError());
      return false;
   }
   tmpPath = tmpFolder;

   #ifndef APP_PATH
   if(getcwd(chAppPath, sizeof(chAppPath)-1) == nullptr) {
      fprintf(stderr, "getcwd failed: %s\n", strerror(errno));
      return false;
   }
   #else
      strncpy(chAppPath,APP_PATH,_MAX_PATH);
   #endif

   if (loadConfig) {
    loadConfiguration(CPC, getConfigurationFilename()); // retrieve the emulator configuration
   }

   if (CPC.printer) {
      if (!printer_start()) { // start capturing printer output, if enabled
         CPC.printer = 0;
      }
   }

   z80_init_tables(); // init Z80 emulation

   if (video_init()) {
      fprintf(stderr, "video_init() failed. Aborting.\n");
      return false;
   }

   if (audio_init()) {
      fprintf(stderr, "audio_init() failed. Disabling sound.\n");
      // TODO(cpitrat): Do not set this to 0 when audio_init fail as this affect
      // configuration when saving from GUI. Rather use some other indicator to
      // know whether snd_bufferptr is usable or not.
      // To test it, set SDL_AUDIODRIVER=dsp or some other unsupported value.
      CPC.snd_enabled = 0; // disable sound emulation
   }

   if (joysticks_init()) {
      fprintf(stderr, "joysticks_init() failed. Joysticks won't work.\n");
   }

#ifdef DEBUG
   pfoDebug = fopen("./debug.txt", "wt");
#endif

   // Extract files to be loaded from the command line args
   fillSlots(slot_list, CPC);

   // Must be done before emulator_init()
   CPC.InputMapper = new InputMapper(&CPC);

   // emulator_init must be called before loading files as they require
   // pbGPBuffer to be initialized.
   if (emulator_init()) {
      fprintf(stderr, "emulator_init() failed. Aborting.\n");
      return false;
   }

   // Really load the various drives, if needed
   loadSlots();

   // Fill the buffer with autocmd if provided
   virtualKeyboardEvents = CPC.InputMapper->StringToEvents(args.autocmd);
   // Give some time to the CPC to start before sending any command
   nextVirtualEventFrameCount = dwFrameCountOverall + CPC.boot_time;

// ----------------------------------------------------------------------------

   update_timings();
   audio_resume();

   iExitCondition = EC_FRAME_COMPLETE;

   return true;
}

bool take_screenshot = false;
videocallback videocb = NULL;
struct Cap32Screen cpcscreen = Cap32Screen{};

void cap32ext_update() {
    if (!CPC.paused) { // run the emulation, as long as the user doesn't pause it
      dwTicks = SDL_GetTicks();
      if (dwTicks >= dwTicksTargetFPS) { // update FPS counter?
         dwFPS = dwFrameCount;
         dwFrameCount = 0;
         dwTicksTargetFPS = dwTicks + 1000; // prep counter for the next run
      }

      if (CPC.limit_speed) { // limit to original CPC speed?
         if (CPC.snd_enabled) {
            if (iExitCondition == EC_SOUND_BUFFER) { // Emulation filled a sound buffer.
               if (!dwSndBufferCopied) {
                  return; // delay emulation until our audio callback copied and played the buffer
               }
               dwSndBufferCopied = 0;
            }
         } else if (iExitCondition == EC_CYCLE_COUNT) {
            dwTicks = SDL_GetTicks();
            if (dwTicks < dwTicksTarget) { // limit speed ?
               if (dwTicksTarget - dwTicks > POLL_INTERVAL_MS) { // No need to burn cycles if next event is far away
                  std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
               }
               return; // delay emulation
            }
            dwTicksTarget = dwTicks + dwTicksOffset; // prep counter for the next run
         }
      }

      if (!vid_plugin->lock()) { // lock the video buffer
         return; // skip the emulation if we can't get a lock
      }
      dword dwOffset = CPC.scr_pos - CPC.scr_base; // offset in current surface row
      if (VDU.scrln > 0) {
         CPC.scr_base = static_cast<byte *>(back_surface->pixels) + (VDU.scrln * CPC.scr_line_offs); // determine current position
      } else {
         CPC.scr_base = static_cast<byte *>(back_surface->pixels); // reset to surface start
      }
      CPC.scr_pos = CPC.scr_base + dwOffset; // update current rendering position

      iExitCondition = z80_execute(); // run the emulation until an exit condition is met
      
      if (iExitCondition == EC_BREAKPOINT) {
         // We have to clear breakpoint to let the z80 emulator move on.
         z80.break_point = 0xffffffff; // clear break point
         z80.trace = 1; // make sure we'll be here to rearm break point at the next z80 instruction.

         if (breakPointsToSkipBeforeProceedingWithVirtualEvents>0) {
            breakPointsToSkipBeforeProceedingWithVirtualEvents--;
            LOG_DEBUG("Decremented breakpoint skip counter to " << breakPointsToSkipBeforeProceedingWithVirtualEvents);
         }
      } else {
         if (z80.break_point == 0xffffffff) { // TODO(cpcitor) clean up 0xffffffff into a value like Z80_BREAKPOINT_NONE
            LOG_DEBUG("Rearming EC_BREAKPOINT.");
            z80.break_point = 0; // set break point for next time
         }
      }

      if (iExitCondition == EC_FRAME_COMPLETE) { // emulation finished rendering a complete frame?
         dwFrameCountOverall++;
         dwFrameCount++;
         if (SDL_GetTicks() < osd_timing) {
            print(static_cast<byte *>(back_surface->pixels) + CPC.scr_line_offs, osd_message.c_str(), true);
         } else if (CPC.scr_fps) {
            char chStr[15];
            sprintf(chStr, "%3dFPS %3d%%", static_cast<int>(dwFPS), static_cast<int>(dwFPS) * 100 / (1000 / static_cast<int>(FRAME_PERIOD_MS)));
            print(static_cast<byte *>(back_surface->pixels) + CPC.scr_line_offs, chStr, true); // display the frames per second counter
         }
         asic_draw_sprites();
         vid_plugin->unlock();
         video_display(); // update PC display
         if (take_screenshot) {
            dumpScreen();
            take_screenshot = false;
         }
         //Callback if any registered
         if (videocb != NULL) {
               cpcscreen.w = back_surface->w;
               cpcscreen.h = back_surface->h;
               cpcscreen.pixels = back_surface->pixels;
               videocb(&cpcscreen);
         }

      } else {
         vid_plugin->unlock();
      }
    }
    else { // We are paused. No need to burn CPU cycles
       std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    }
}

void video_update_callback(videocallback newcallback) {
    videocb = newcallback;
}

void cap32ext_finish() {
   cleanExit(0);
}

bool cap32ext_sendevent(SDL_Event event) {
    if(!virtualKeyboardEvents.empty()
        && (nextVirtualEventFrameCount < dwFrameCountOverall)
        && (breakPointsToSkipBeforeProceedingWithVirtualEvents == 0)) {

        auto nextVirtualEvent = &virtualKeyboardEvents.front();
        SDL_PushEvent(nextVirtualEvent);
        
        auto keysym = nextVirtualEvent->key.keysym;
        LOG_DEBUG("Inserted virtual event keysym=" << int(keysym.sym));
        
        dword cpc_key = CPC.InputMapper->CPCkeyFromKeysym(keysym);
        if (!(cpc_key & MOD_EMU_KEY)) {
        LOG_DEBUG("The virtual event is a keypress (not a command), so introduce a pause.");
        // Setting nextVirtualEventFrameCount below guarantees to
        // immediately break the loop enclosing this code and wait
        // at least one frame.
        nextVirtualEventFrameCount = dwFrameCountOverall
            + ((event.type == SDL_KEYDOWN)?1:0);
        // The extra delay in case of SDL_KEYDOWN is to keep the
        // key pressed long enough.  If we don't do this, the CPC
        // firmware debouncer eats repeated characters.
        }

        virtualKeyboardEvents.pop_front();
    }
    
   switch (event.type) {
      case SDL_KEYDOWN:
         {
               dword cpc_key = CPC.InputMapper->CPCkeyFromKeysym(event.key.keysym);
               if (!(cpc_key & MOD_EMU_KEY)) {
                  applyKeypress(cpc_key, keyboard_matrix, true);
               }
         }
         break;

      case SDL_KEYUP:
         {
               dword cpc_key = CPC.InputMapper->CPCkeyFromKeysym(event.key.keysym);
               if (!(cpc_key & MOD_EMU_KEY)) {
                  applyKeypress(cpc_key, keyboard_matrix, false);
               }
               else { // process emulator specific keys
                  switch (cpc_key) {
                  case CAP32_FULLSCRN:
                     audio_pause();
                     SDL_Delay(20);
                     video_shutdown();
                     CPC.scr_window = CPC.scr_window ? 0 : 1;
                     if (video_init()) {
                           fprintf(stderr, "video_init() failed. Aborting.\n");
                           return false;
                     }
                     audio_resume();
                     break;

                  case CAP32_SCRNSHOT:
                     // Delay taking the screenshot to ensure the frame is complete.
                     take_screenshot = true;
                     break;

                  case CAP32_DELAY:
                     // Reuse boot_time as it is a reasonable wait time for Plus transition between the F1/F2 nag screen and the command line.
                     // TODO: Support an argument to CAP32_DELAY in autocmd instead.
                     LOG_VERBOSE("Take into account CAP32_DELAY");
                     nextVirtualEventFrameCount = dwFrameCountOverall + CPC.boot_time;
                     break;

                  case CAP32_WAITBREAK:
                     breakPointsToSkipBeforeProceedingWithVirtualEvents++;
                     LOG_INFO("Will skip " << breakPointsToSkipBeforeProceedingWithVirtualEvents << " before processing more virtual events.");
                     LOG_DEBUG("Setting z80.break_point=0 (was " << z80.break_point << ").");
                     z80.break_point = 0; // set break point to address 0. FIXME would be interesting to change this via a parameter of CAP32_WAITBREAK on command line.
                     break;

                  case CAP32_SNAPSHOT:
                     dumpSnapshot();
                     break;

                  case CAP32_TAPEPLAY:
                     LOG_DEBUG("Request to play tape");
                     Tape_Rewind();
                     if (pbTapeImage) {
                           if (CPC.tape_play_button) {
                              LOG_DEBUG("Play button released");
                              CPC.tape_play_button = 0;
                           } else {
                              LOG_DEBUG("Play button pushed");
                              CPC.tape_play_button = 0x10;
                           }
                     }
                     set_osd_message(std::string("Play tape: ") + (CPC.tape_play_button ? "on" : "off"));
                     break;

                  case CAP32_MF2STOP:
                     if(CPC.mf2 && !(dwMF2Flags & MF2_ACTIVE)) {
                           reg_pair port;

                           // Set mode to activate ROM_config
                           //port.b.h = 0x40;
                           //z80_OUT_handler(port, 128);

                           // Attempt to load MF2 in lower ROM (can fail if lower ROM is not active)
                           port.b.h = 0xfe;
                           port.b.l = 0xe8;
                           dwMF2Flags &= ~MF2_INVISIBLE;
                           z80_OUT_handler(port, 0);

                           // Stop execution if load succeeded
                           if(dwMF2Flags & MF2_ACTIVE) {
                           z80_mf2stop();
                           }
                     }
                     break;

                  case CAP32_RESET:
                     LOG_DEBUG("User requested emulator reset");
                     emulator_reset(false);
                     break;

                  case CAP32_JOY:
                     CPC.joystick_emulation = CPC.joystick_emulation ? 0 : 1;
                     CPC.InputMapper->set_joystick_emulation();
                     set_osd_message(std::string("Joystick emulation: ") + (CPC.joystick_emulation ? "on" : "off"));
                     break;

                  case CAP32_EXIT:
                     return false;

                  case CAP32_FPS:
                     CPC.scr_fps = CPC.scr_fps ? 0 : 1; // toggle fps display on or off
                     set_osd_message(std::string("Performances info: ") + (CPC.scr_fps ? "on" : "off"));
                     break;

                  case CAP32_SPEED:
                     CPC.limit_speed = CPC.limit_speed ? 0 : 1;
                     set_osd_message(std::string("Limit speed: ") + (CPC.limit_speed ? "on" : "off"));
                     break;

                  case CAP32_DEBUG:
                     log_verbose = !log_verbose;
                     #ifdef DEBUG
                     dwDebugFlag = dwDebugFlag ? 0 : 1;
                     #endif
                     #ifdef DEBUG_CRTC
                     if (dwDebugFlag) {
                           for (int n = 0; n < 14; n++) {
                              fprintf(pfoDebug, "%02x = %02x\r\n", n, CRTC.registers[n]);
                           }
                     }
                     #endif
                     set_osd_message(std::string("Debug mode: ") + (log_verbose ? "on" : "off"));
                     break;
                  }
               }
         }
         break;

      case SDL_JOYBUTTONDOWN:
      {
         dword cpc_key = CPC.InputMapper->CPCkeyFromJoystickButton(event.jbutton);
                           if (cpc_key == 0xff) {
         }
         applyKeypress(cpc_key, keyboard_matrix, true);
      }
      break;

      case SDL_JOYBUTTONUP:
      {
         dword cpc_key = CPC.InputMapper->CPCkeyFromJoystickButton(event.jbutton);
         applyKeypress(cpc_key, keyboard_matrix, false);
      }
      break;

      case SDL_JOYAXISMOTION:
      {
         dword cpc_key[2] = {0xff, 0xff};
         bool release = false;
         CPC.InputMapper->CPCkeyFromJoystickAxis(event.jaxis, cpc_key, release);
         applyKeypress(cpc_key[0], keyboard_matrix, !release);
         if (release && cpc_key[0] != 0xff) {
               applyKeypress(cpc_key[1], keyboard_matrix, !release);
         }
      }
      break;

#ifdef WITH_SDL2
         //TODO: FMS - Check this cover all cases
         case SDL_WINDOWEVENT:
            switch (event.window.event) {
               case SDL_WINDOWEVENT_FOCUS_LOST:
                  _appWindowState = LostFocus;
                  if (CPC.auto_pause)
                     cpc_pause();
                  break;
               case SDL_WINDOWEVENT_FOCUS_GAINED:
                  _appWindowState = GainedFocus;
                  if (CPC.auto_pause)
                     cpc_resume();
                  break;
            }
            break;
#else
         // Code shamelessly copied from http://sdl.beuc.net/sdl.wiki/Event_Examples
         // TODO: What if we were paused because of other reason than losing focus and then only lost focus
         //       the right thing to do here is to restore focus but keep paused... implementing this require
         //       keeping track of pause source, which will be a pain.
         case SDL_ACTIVEEVENT:
            if (event.active.state == (SDL_APPINPUTFOCUS | SDL_APPACTIVE) ) {
               if (event.active.gain == 0) {
                  _appWindowState = Minimized;
                  cpc_pause(); // Always paused when iconified
               } else {
                  if (_appWindowState == LostFocus ) {
                        _appWindowState = GainedFocus;
                        if (CPC.auto_pause)
                           cpc_resume();
                  } else {
                        _appWindowState = Restored;
                        cpc_resume(); // Always unpause when restoring from iconified
                  }
               }
            }
            else if (event.active.state & SDL_APPINPUTFOCUS) {
               if (event.active.gain == 0) {
                     _appWindowState = LostFocus;
                     if (CPC.auto_pause)
                        cpc_pause();
               }
               else {
                     _appWindowState = GainedFocus;
                     if (CPC.auto_pause)
                        cpc_resume();
               }
            }
            break;
#endif

      }
      return true;
}

void cap32ext_checFinalConfig() {
   if (CPC.model > 3) {
      CPC.model = 2;
   }
   if (CPC.ram_size > 576) {
      CPC.ram_size = 576;
   } else if ((CPC.model >= 2) && (CPC.ram_size < 128)) {
      CPC.ram_size = 128; // minimum RAM size for CPC 6128 is 128KB
   }
   if ((CPC.speed < MIN_SPEED_SETTING) || (CPC.speed > MAX_SPEED_SETTING)) {
      CPC.speed = DEF_SPEED_SETTING;
   }
   if (CPC.keyboard > MAX_ROM_MODS) {
      CPC.keyboard = 0;
   }
   CPC.joystick_menu_button -= 1;
   CPC.joystick_vkeyboard_button -= 1;
   if (CPC.scr_style >= nb_video_plugins) {
      CPC.scr_style = DEFAULT_VIDEO_PLUGIN;
      LOG_ERROR("Unsupported video plugin specified - defaulting to plugin " << video_plugin_list[DEFAULT_VIDEO_PLUGIN].name);
   }
   if (CPC.scr_oglscanlines > 100) {
      CPC.scr_oglscanlines = 30;
   }
   if ((CPC.scr_intensity < 5) || (CPC.scr_intensity > 15)) {
      CPC.scr_intensity = 10;
   }
   if (CPC.snd_playback_rate > (MAX_FREQ_ENTRIES-1)) {
      CPC.snd_playback_rate = 2;
   }
   if (CPC.snd_volume > 100) {
      CPC.snd_volume = 80;
   }
}

void cap32ext_loadCPCDefaults() {
   CPC.model = 2; // CPC 6128
   CPC.jumpers = 0x1e; // OEM is Amstrad, video refresh is 50Hz
   CPC.ram_size = 128; // 128KB RAM
   CPC.speed = DEF_SPEED_SETTING; // original CPC speed
   CPC.limit_speed = 1;
   CPC.auto_pause = 1;
   CPC.boot_time = 5;
   CPC.printer = 0;
   CPC.mf2 = 0;
   CPC.keyboard = 0;
   CPC.joystick_emulation = 0;
   CPC.joysticks = 1;
   CPC.joystick_menu_button = 9;
   CPC.joystick_vkeyboard_button = 10;
   CPC.resources_path = "resources";
   CPC.scr_fs_width = 800;
   CPC.scr_fs_height = 600;
   CPC.scr_fs_bpp = 8;
   CPC.scr_style = 0;
   CPC.scr_oglfilter = 1;
   CPC.scr_oglscanlines = 30;

   CPC.scr_led = 1;
   CPC.scr_fps = 0;
   CPC.scr_tube = 0;
   CPC.scr_intensity = 10;
   CPC.scr_remanency = 0;
   CPC.scr_window = 1;

   CPC.snd_enabled = 1;
   CPC.snd_playback_rate = 2;
   CPC.snd_bits = 1;
   CPC.snd_stereo = 1;
   CPC.snd_volume = 80;
   CPC.snd_pp_device = 0;

   CPC.kbd_layout = "keymap_us.map";

   CPC.max_tracksize = 6144-154;
   CPC.current_snap_path =
   CPC.snap_path = "snap";
   CPC.current_cart_path =
   CPC.cart_path = "cart";
   CPC.current_dsk_path =
   CPC.dsk_path = "disk";
   CPC.current_tape_path =
   CPC.tape_path = "tape";

   CPC.printer_file = "printer.dat";
   CPC.sdump_dir = "screenshots";

   CPC.rom_path = "rom";
   for (int iRomNum = 0; iRomNum < 16; iRomNum++) { // loop for ROMs 0-15
      char chRomId[14];
      sprintf(chRomId, "slot%02d", iRomNum); // build ROM ID
      CPC.rom_file[iRomNum] = "";
   }
   CPC.rom_file[7] = "amsdos.rom";  //Insert amsdos in slot 7
   CPC.rom_mf2 = "";
   CPC.cart_file = CPC.rom_path + "/system.cpr"; // Only default path defined. Needed for CPC6128+
}

#ifdef _ANDROID_
FILE *getAndroidTmpFile() {
    FILE * handle = nullptr;

    std::string path = tmpPath + "/cpc-dsk-tmp";

    LOG_INFO("Creating tmp file in:" + path);
    int descriptor = mkstemp(&path[0]);
    if (-1 != descriptor) {
        handle = fdopen(descriptor, "w+b");
        if (nullptr == handle) {
            LOG_ERROR("Error converting file");
            close(descriptor);
        }

        // File already open,
        // can be unbound from the file system
        std::remove(path.c_str());
    }
    return handle;
}
#endif