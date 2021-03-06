/***********************************************************
 *                      K O U L E S                         *
 *----------------------------------------------------------*
 *  C1995 JAHUSOFT                                          *
 *        Jan Hubicka                                       *
 *        Dukelskych Bojovniku 1944                         *
 *        390 03 Tabor                                      *
 *        Czech Republic                                    *
 *        Phone: 0041-361-32613                             *
 *        eMail: hubicka@limax.paru.cas.cz                  *
 *----------------------------------------------------------*
 *   Copyright(c)1995,1996 by Jan Hubicka.See README for    *
 *                   licence details.                       *
 *----------------------------------------------------------*
 *  init.c jlib depended initialization routines            *
 ***********************************************************/
#define PLAYFORM_VARIABLES_HERE
// DKS #include <vga.h>
// DKS #include <vgagl.h>

#include <sys/time.h>
#include <time.h>
#include "SDL.h"
#include "SFont.h"

// DKS
#include "../koules.h"
#include "interface.h"

// DKS GraphicsContext *physicalscreen, *backscreen, *starbackground, *background;
SDL_Surface *physicalscreen = NULL, *starbackground = NULL, *background = NULL, *bottom_bar = NULL;

/* DKS
void           *fontblack;
void           *fontwhite;
*/

// Added by DKS - custom palette representations (8-bit game converted to palette-less format)
Uint32 colors_RGBA[256]; // Stores full RGBA representation of palette for SDL_gfx usage and 32bpp displays
#ifndef SDL32BPP
Uint32 colors_RGBA[256];  // Stores full RGBA representation of palette for SDL_gfx usage and 32bpp displays
Uint16 colors_16bit[256]; // Stores 16-bit RGB representation for 16bpp displays
#endif

SFont_Font *fontblack = NULL;
SDL_Surface *fontblack_surface = NULL;
SFont_Font *fontwhite = NULL;
SDL_Surface *fontwhite_surface = NULL;
SFont_Font *fontgray = NULL;
SDL_Surface *fontgray_surface = NULL;

#ifndef GCW
SDL_Joystick *joy = NULL;
#else
SDL_Joystick *joy_analog = NULL;
SDL_Joystick *joy_gsensor = NULL;
int analog_enabled = 0;  // This can be toggled on and off through main menu
int gsensor_enabled = 0; // Ditto..
int rumble_enabled = 0;  // Ditto (rumble support waiting on Zear's libhaptic being completed)
// Not implemented yet:
// SDL_Joystick *joy_ext1 = NULL;
// SDL_Joystick *joy_ext2 = NULL;

int analog_deadzone = 8000;  // seems to work well on GCW Zero's analog stick
int gsensor_deadzone = 1250; // seems to also work well on the GCW Zero's g-sensor
int gsensor_centerx = 0;     // the g-sensor needs a re-settable center so the user can play at a normal tilt
int gsensor_centery = 13100; //	13100 is a reasonable backwards-tilt to set as default.
int gsensor_recently_recentered =
    0;                         // This signals code in koules.c to display message confirming recentering of g-sensor
const int gsensor_max = 26200; // seems to be the maximum reading in any direction from the gsensor
#endif

// DKS int             VGAMODE = G640x480x256;

/* DKS
int             GAMEWIDTH = 640;
int             GAMEHEIGHT = 460;
int             MAPWIDTH = 640;
int             MAPHEIGHT = 460;
int             DIV = 1;

int flipping=0,page=0;
*/

int GAMEWIDTH = 640*4;
int GAMEHEIGHT = 440*4;
int MAPWIDTH = 320*4;  // for GP2X/dingoo/wiz/gcw
int MAPHEIGHT = 220*4; // for GP2X/dingoo/wiz/gcw
int DIV = 2;         // A pretty obvious guess - DKS

#ifdef SDL32BPP
// DKS - For SDL initialization on 32bpp platforms
const int SCREEN_WIDTH = 320*4;
const int SCREEN_HEIGHT = 240*4;
const int SCREEN_BPP = 32;
#else
// DKS - For SDL initialization on 16bpp platforms
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;
const int SCREEN_BPP = 16;
#endif

#include <alloca.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/vt.h>
#include <unistd.h>
#include "../client.h"
#include "../framebuffer.h"
#include "../koules.h"
#include "../net.h"
#include "../server.h"

// DKS
//#include <asm/io.h>		/*for waiting for retrace */
//#include <sys/ioctl.h>		/*for waiting for retrace */

extern void fadein1();

// DKS added:
int control_state[CNUMCONTROLS];

int control_pressed(int control) {
    if (control < CNUMCONTROLS)
        return control_state[control];
    else
        printf("Error: invalid parameter to control_pressed(int): %d\n", control);
    return 0;
}

// DKS - this function is used to determine if any button at all is pressed (barring the DPAD directionals)
int any_control_pressed(void) {
    return control_state[CHELP] || control_state[CPAUSE] || control_state[CENTER] || control_state[CESCAPE];
}

// DKS - added this hack function to fix power slider issues on GCW and when enabling/disabling gsensor/analog stick
// Details: slider issues SDLK_PAUSE keypress before slider daemon
// handles volume/lcd brightness adjustment, this somehow can interfere
// with game's directional keystates after it is released.
// Intended to be called from control_update() below
static void control_reset() {
    memset(control_state, 0, sizeof control_state);
}

void control_update(void) {
    SDL_PumpEvents();

    // DKS
#ifdef GCW
    // Hack to get around bug in Dingux firmware when power slider button is pressed. It
    //	interferes with SDL's internal keystate unless worked-around through the only way
    //	it can be detected: events. (Polled state is not possible for detecting when to
    //	apply the hack).
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                exit(0);
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_HOME: // GCW Power slider - apply hack
                        control_reset();
                        return;
                        break;
                    case SDLK_UP:
                        control_state[CUP] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        control_state[CDOWN] = 0; // power slider bug hack
                        break;
                    case SDLK_DOWN:
                        control_state[CDOWN] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        control_state[CUP] = 0; // power slider bug hack
                        break;
                    case SDLK_LEFT:
                        control_state[CLEFT] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        control_state[CRIGHT] = 0; // power slider bug hack
                        break;
                    case SDLK_RIGHT:
                        control_state[CRIGHT] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        control_state[CLEFT] = 0; // power slider bug hack
                        break;
                    case SDLK_RETURN: // GCW Start button
                        control_state[CENTER] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        break;
                    case SDLK_ESCAPE: // GCW Select button
                        control_state[CESCAPE] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        break;
                    case SDLK_SPACE: // GCW Y button
                        control_state[CALTUP] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        control_state[CENTER] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        break;
                    case SDLK_LALT: // GCW B button
                        control_state[CALTDOWN] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        control_state[CENTER] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        break;
                    case SDLK_LSHIFT: // GCW X button
                        control_state[CALTLEFT] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        control_state[CENTER] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        break;
                    case SDLK_LCTRL: // GCW A button
                        control_state[CALTRIGHT] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        control_state[CENTER] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                        break;
                    default:
                        break;
                }
        }
    }

    Uint8 *keystate = SDL_GetKeyState(NULL); // strictly to read L/R Triggers for g-sensor calibration below
    // DKS - on GCW, control can also be done with the analog stick or the g-sensor:
    // First, see if the user is pressing L+R shoulder buttons together, signalling a
    //		desire to zero-out the g-sensor (allows playing in bed, at a slant, etc)
    SDL_JoystickUpdate();

    if (gsensor_enabled && joy_gsensor && keystate[SDLK_BACKSPACE] && keystate[SDLK_TAB]) {
        // user has pressed both shoulder buttons, requesting a centering of the g-sensor
        gsensor_recently_recentered = 1;
        Sint16 xmove, ymove;
        xmove = SDL_JoystickGetAxis(joy_gsensor, 0);
        ymove = SDL_JoystickGetAxis(joy_gsensor, 1);
        gsensor_centerx = xmove;
        gsensor_centery = ymove;
        if ((xmove + gsensor_deadzone) > gsensor_max)
            gsensor_centerx = gsensor_max - gsensor_deadzone;
        else if ((xmove - gsensor_deadzone) < -gsensor_max)
            gsensor_centerx = -gsensor_max + gsensor_deadzone;
        if ((ymove + gsensor_deadzone) > gsensor_max)
            gsensor_centery = gsensor_max - gsensor_deadzone;
        else if ((ymove - gsensor_deadzone) < -gsensor_max)
            gsensor_centery = -gsensor_max + gsensor_deadzone;
        printf("G-sensor zeroed out.. new center-x: %d   new center-y: %d\n", gsensor_centerx, gsensor_centery);
    }

    // ANALOG JOY:
    if (analog_enabled && joy_analog) {
        Sint16 xmove, ymove;
        xmove = SDL_JoystickGetAxis(joy_analog, 0);
        ymove = SDL_JoystickGetAxis(joy_analog, 1);
        control_state[CANALOGLEFT] = (xmove < -analog_deadzone);
        control_state[CANALOGRIGHT] = (xmove > analog_deadzone);
        control_state[CANALOGUP] = (ymove < -analog_deadzone);
        control_state[CANALOGDOWN] = (ymove > analog_deadzone);
    } else {
        control_state[CANALOGUP] = control_state[CANALOGDOWN] = control_state[CANALOGLEFT] =
            control_state[CANALOGRIGHT] = 0;
    }

    // G-SENSOR:
    if (gsensor_enabled && joy_gsensor) { // disabled in 2-player mode
        Sint16 xmove, ymove;
        xmove = SDL_JoystickGetAxis(joy_gsensor, 0);
        ymove = SDL_JoystickGetAxis(joy_gsensor, 1);
        control_state[CGSENSORLEFT] = (xmove < (gsensor_centerx - gsensor_deadzone));
        control_state[CGSENSORRIGHT] = (xmove > (gsensor_centerx + gsensor_deadzone));
        control_state[CGSENSORUP] = (ymove < (gsensor_centery - gsensor_deadzone));
        control_state[CGSENSORDOWN] = (ymove > (gsensor_centery + gsensor_deadzone));
    } else {
        control_state[CGSENSORUP] = control_state[CGSENSORDOWN] = control_state[CGSENSORLEFT] =
            control_state[CGSENSORRIGHT] = 0;
    }
#endif
}

extern void points();
extern void points1();
char hole_data[HOLE_RADIUS * 2][HOLE_RADIUS * 2];
char ehole_data[HOLE_RADIUS * 2][HOLE_RADIUS * 2];
// DKS - fixing for unaligned arch's:
// extern char     rocketcolor[5];
extern unsigned int rocketcolor[5];
extern void setcustompalette(int, float);
extern void starwars();
extern void game();

#define NCOLORS 32

#define HOLE_XCENTER (2 * HOLE_RADIUS - 3 * HOLE_RADIUS / 4)
#define HOLE_YCENTER (2 * HOLE_RADIUS - HOLE_RADIUS / 4)
#define HOLE_MAX_RADIUS (HOLE_RADIUS / DIV + 0.5 * HOLE_RADIUS / DIV)
#define HOLE_SIZE_MAX (radius * radius)

/* DKS
static GraphicsContext *
my_allocatecontext ()
{
  return malloc (sizeof (GraphicsContext));
}
*/

// DKS char *framebuff;

// DKS - modified and replaced
// static void
// initialize ()
//{
//#ifdef SOUND
//  printf ("Initializing sound server...\n");
//  if (sndinit)
//    init_sound ();
//#else
//  printf ("Sound driver not avaiable-recompile koules with SOUND enabled\n");
//#endif
//
//  printf ("Autoprobing hardware\n");
//  printf ("Initializing joystick driver\n");
//#ifdef JOYSTICK
//  joystickdevice[0] = open ("/dev/js0", O_RDONLY);
//  if (joystickdevice[0] < 0)
//    {
//      perror ("Joystick driver");
//      printf ("Joystick 1 not avaiable..\n");
//      joystickplayer[0] = -1;
//    }
//  else
//    printf ("Joystick 1 initialized\n");
//  joystickdevice[1] = open ("/dev/js1", O_RDONLY);
//  if (joystickdevice[1] < 0)
//    {
//      perror ("Joystick driver");
//      printf ("Joystick 2 not avaiable..\n");
//      joystickplayer[1] = -1;
//    }
//  else
//    printf ("Joystick 2 initialized\n");
//
//#else
//  printf ("Joystick driver not avaiable(recompile koules with JOYSTICK enabled )\n");
//#endif
//  printf ("Testing terminal\n");
//
//  printf ("Initializing mouse server\n");
//#ifdef MOUSE
//  if (!nomouse)
//    vga_setmousesupport (1);
//#endif
//  printf ("Initializing graphics server\n");
//  vga_init ();
//  if (!vga_hasmode (VGAMODE))
//    {
//      printf ("graphics mode unavaiable(reconfigure svgalib)\n");
//      if (VGAMODE == G640x480x256)
//	printf ("or use -s option\n");
//      exit (-2);
//    }
//  vga_setmode (VGAMODE);
//
//
//
//  printf ("Initializing video memory\n");
//  setcustompalette (0, 1);
//  gl_setcontextvga (VGAMODE);
//
//
//  physicalscreen = my_allocatecontext ();
//  backscreen = my_allocatecontext ();
//  gl_setclippingwindow (0, 0, MAPWIDTH - 1,
//			MAPHEIGHT + 19);
//  gl_getcontext (physicalscreen);
//  if(flipping) {
//    if(physicalscreen->modetype!=CONTEXT_LINEAR||!gl_enablepageflipping(physicalscreen)) flipping=0; else {
//    backscreen=physicalscreen;
//    flippage();
//    }
//  }
//
//
//  printf ("Initializing graphics font\n");
//  fontblack = malloc (256 * 8 * 8);
//  gl_expandfont (8, 8, back (3), gl_font8x8, fontblack);
//  fontwhite = malloc (256 * 8 * 8);
//  gl_setfont (8, 8, fontwhite);
//  gl_expandfont (8, 8, 255, gl_font8x8, fontwhite);
//
//
//  gl_write (0, 0, "Graphics daemons fired up");
//  gl_write (0, 8, "Checking system consitency....virus not found..");
//  if(!flipping) {
//  gl_setcontextvgavirtual (VGAMODE);
//  gl_setclippingwindow (0, 0, MAPWIDTH - 1,
//			MAPHEIGHT + 19);
//  }
//  gl_getcontext (backscreen);
//  gl_setclippingwindow (0, 0, MAPWIDTH - 1,
//			MAPHEIGHT + 19);
//  gl_setcontext (physicalscreen);
//  gl_write (0, 16, "Calibrating delay loop");
//
//
//  gl_setcontextvgavirtual (VGAMODE);
//  background = my_allocatecontext ();
//  gl_setclippingwindow (0, 0, MAPWIDTH - 1,
//			MAPHEIGHT + 19);
//  gl_getcontext (background);
//  gl_setcontextvgavirtual (VGAMODE);
//  starbackground = my_allocatecontext ();
//  gl_setclippingwindow (0, 0, MAPWIDTH - 1,
//			MAPHEIGHT + 19);
//  gl_getcontext (starbackground);
//  gl_setcontext (physicalscreen);
//  gl_write (0, 24, "Initializing keyboard daemons");
//  if (keyboard_init ())
//    {
//      printf ("Could not initialize keyboard.\n");
//      exit (-1);
//    }
//  /*keyboard_translatekeys(TRANSLATE_CURSORKEYS | TRANSLATE_KEYPADENTER); */
//  keyboard_translatekeys (0);
//  gl_write (0, 32, "1 pc capable keyboard found");
//}

static void initialize(void) {

    // DKS - disabled
    //
    //  printf ("Autoprobing hardware\n");
    //  printf ("Initializing joystick driver\n");
    //#ifdef JOYSTICK
    //  joystickdevice[0] = open ("/dev/js0", O_RDONLY);
    //  if (joystickdevice[0] < 0)
    //    {
    //      perror ("Joystick driver");
    //      printf ("Joystick 1 not avaiable..\n");
    //      joystickplayer[0] = -1;
    //    }
    //  else
    //    printf ("Joystick 1 initialized\n");
    //  joystickdevice[1] = open ("/dev/js1", O_RDONLY);
    //  if (joystickdevice[1] < 0)
    //    {
    //      perror ("Joystick driver");
    //      printf ("Joystick 2 not avaiable..\n");
    //      joystickplayer[1] = -1;
    //    }
    //  else
    //    printf ("Joystick 2 initialized\n");
    //
    //#else
    //  printf ("Joystick driver not avaiable(recompile koules with JOYSTICK enabled )\n");
    //#endif
    //  printf ("Testing terminal\n");
    //
    //  printf ("Initializing mouse server\n");
    //#ifdef MOUSE
    //  if (!nomouse)
    //    vga_setmousesupport (1);
    //#endif
    printf("Initializing SDL subsystems\n");
    // DKS vga_init ();
    // Initialize all SDL subsystems
#ifdef GP2X
    // For Dingoo/GCW, do not init joystick subsystem (not needed on Dingoo, saves power on GCW
    //  as all buttons are mapped to keyboard presses in the kernel and no need to poll analog stick on GCW.)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1)
#else
    // For Wiz/GP2X/GCW etc, init the joystick subsystem
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1)
#endif
    {
        printf("Couldn't initialize SDL systems\n");
        exit(-1);
    }

    // Set up the screen
    physicalscreen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, SDL_HWSURFACE | SDL_DOUBLEBUF);
    SDL_ShowCursor(SDL_DISABLE); // Disable mouse cursor on gp2x/gcw

    // If there was in error in setting up the screen
    if (!physicalscreen) {
        printf("Couldn't set video mode with SDL\n");
        exit(-1);
    }

#ifdef SOUND
    printf("Initializing SDL sound...\n");
    if (sndinit)
        init_sound();
#else
    printf("Sound not avaiable-recompile koules with SOUND enabled\n");
#endif

        // Set the window caption
        // SDL_WM_SetCaption( "SDL Koules", NULL );

        // DKS
#ifdef GCW
    printf("Initializing joystick driver\n");
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        printf("Joystick %u: \"%s\"\n", i, SDL_JoystickName(i));
        if (strcmp(SDL_JoystickName(i), "linkdev device (Analog 2-axis 8-button 2-hat)") == 0) {
            joy_analog = SDL_JoystickOpen(i);
            if (joy_analog)
                printf("Recognized GCW Zero's built-in analog stick..\n");
            else
                printf("ERROR: Failed to recognize GCW Zero's built-in analog stick..\n");
        } else if (strcmp(SDL_JoystickName(i), "mxc6225") == 0) {
            joy_gsensor = SDL_JoystickOpen(i);
            if (joy_gsensor)
                printf("Recognized GCW Zero's built-in g-sensor..\n");
            else
                printf("ERROR: Failed to recognize GCW Zero's built-in g-sensor..\n");
        }
    }
    SDL_JoystickEventState(SDL_IGNORE); // We'll do our own polling
    // DKS - also disable keyboard events:
    SDL_EnableKeyRepeat(0, 0); // No key repeat
                               // DKS - originally, I wanted to ignore keyboard events but found at least on GCW that
    //		it disabled me from detecting the power-slider and getting around a nasty
    //		bug when using it. The bug was that when you press the power slider, it can
    //		lock other keys into being thought pressed. I tried to no avail to even detect
    //		it was pressed (SDLK_HOME) by any means other than keyboard events.
//	SDL_EventState(SDL_KEYDOWN, SDL_IGNORE);
//	SDL_EventState(SDL_KEYUP, SDL_IGNORE);
#else
    joy = NULL;
    SDL_JoystickEventState(SDL_IGNORE); // We'll do our own polling
#endif

    printf("Creating 256-color pseudo 16bpp/32bpp palettes\n");
    setcustompalette(0, 1);

    printf("Initializing control status array\n");
    control_reset();

    printf("Initializing video memory\n");

    background =
        SDL_CreateRGBSurface(SDL_HWSURFACE, physicalscreen->w, physicalscreen->h, physicalscreen->format->BitsPerPixel,
                             physicalscreen->format->Rmask, physicalscreen->format->Gmask,
                             physicalscreen->format->Bmask, physicalscreen->format->Amask);
    starbackground =
        SDL_CreateRGBSurface(SDL_HWSURFACE, physicalscreen->w, physicalscreen->h, physicalscreen->format->BitsPerPixel,
                             physicalscreen->format->Rmask, physicalscreen->format->Gmask,
                             physicalscreen->format->Bmask, physicalscreen->format->Amask);
    starbackground =
        SDL_CreateRGBSurface(SDL_HWSURFACE, physicalscreen->w, physicalscreen->h, physicalscreen->format->BitsPerPixel,
                             physicalscreen->format->Rmask, physicalscreen->format->Gmask,
                             physicalscreen->format->Bmask, physicalscreen->format->Amask);
    bottom_bar = SDL_CreateRGBSurface(SDL_HWSURFACE, 320, 240 - MAPHEIGHT - 1, physicalscreen->format->BitsPerPixel,
                                      physicalscreen->format->Rmask, physicalscreen->format->Gmask,
                                      physicalscreen->format->Bmask, physicalscreen->format->Amask);

    // DKS
    // gl_setcontextvga (VGAMODE);
    //  physicalscreen = my_allocatecontext ();
    //  backscreen = my_allocatecontext ();
    //  gl_setclippingwindow (0, 0, MAPWIDTH - 1,
    //			MAPHEIGHT + 19);
    //  gl_getcontext (physicalscreen);
    //  if(flipping) {
    //    if(physicalscreen->modetype!=CONTEXT_LINEAR||!gl_enablepageflipping(physicalscreen)) flipping=0; else {
    //    backscreen=physicalscreen;
    //    flippage();
    //    }
    //  }

    // DKS
    //  printf ("Initializing graphics font\n");
    //  fontblack = malloc (256 * 8 * 8);
    //  gl_expandfont (8, 8, back (3), gl_font8x8, fontblack);
    //  fontwhite = malloc (256 * 8 * 8);
    //  gl_setfont (8, 8, fontwhite);
    //  gl_expandfont (8, 8, 255, gl_font8x8, fontwhite);

    printf("Initializing graphics font\n");
    SDL_Surface *font_tmp = NULL;
    font_tmp = SDL_LoadBMP("font/font_blackonwhite.bmp");
    // If the image loaded
    if (font_tmp != NULL) {
        SDL_SetColorKey(font_tmp, SDL_SRCCOLORKEY, SDL_MapRGB(font_tmp->format, 0xFF, 0xFF, 0xFF));

        // Create an optimized surface
        fontblack_surface = SDL_DisplayFormat(font_tmp);

        // Free the old surface
        SDL_FreeSurface(font_tmp);

        // If the surface was optimized
        if (fontblack_surface != NULL) {
            // Color key surface
            SDL_SetColorKey(fontblack_surface, SDL_RLEACCEL | SDL_SRCCOLORKEY,
                            SDL_MapRGB(fontblack_surface->format, 0xFF, 0xFF, 0xFF));
        }
        fontblack = SFont_InitFont(fontblack_surface);
    } else {
        printf("Error loading font_blackonwhite.bmp\n");
        exit(-1);
    }

    font_tmp = NULL;
    font_tmp = SDL_LoadBMP("font/font_whiteonblack.bmp");
    // If the image loaded
    if (font_tmp != NULL) {

        SDL_SetColorKey(font_tmp, SDL_SRCCOLORKEY, SDL_MapRGB(font_tmp->format, 0, 0, 0));

        // Create an optimized surface
        fontwhite_surface = SDL_DisplayFormat(font_tmp);

        // Free the old surface
        SDL_FreeSurface(font_tmp);

        // If the surface was optimized
        if (fontwhite_surface != NULL) {
            // Color key surface
            SDL_SetColorKey(fontwhite_surface, SDL_RLEACCEL | SDL_SRCCOLORKEY,
                            SDL_MapRGB(fontwhite_surface->format, 0, 0, 0));
        }
        fontwhite = SFont_InitFont(fontwhite_surface);
    } else {
        printf("Error loading font_whiteonblack.bmp\n");
        exit(-1);
    }

    font_tmp = NULL;
    font_tmp = SDL_LoadBMP("font/font_grayonblack.bmp");
    // If the image loaded
    if (font_tmp != NULL) {

        SDL_SetColorKey(font_tmp, SDL_SRCCOLORKEY, SDL_MapRGB(font_tmp->format, 0, 0, 0));

        // Create an optimized surface
        fontgray_surface = SDL_DisplayFormat(font_tmp);

        // Free the old surface
        SDL_FreeSurface(font_tmp);

        // If the surface was optimized
        if (fontgray_surface != NULL) {
            // Color key surface
            SDL_SetColorKey(fontgray_surface, SDL_RLEACCEL | SDL_SRCCOLORKEY,
                            SDL_MapRGB(fontgray_surface->format, 0, 0, 0));
        }
        fontgray = SFont_InitFont(fontgray_surface);
    } else {
        printf("Error loading font_grayonblack.bmp\n");
        exit(-1);
    }
    //
    //  gl_write (0, 0, "Graphics daemons fired up");
    //  gl_write (0, 8, "Checking system consitency....virus not found..");
    //  if(!flipping) {
    //  gl_setcontextvgavirtual (VGAMODE);
    //  gl_setclippingwindow (0, 0, MAPWIDTH - 1,
    //			MAPHEIGHT + 19);
    //  }
    //  gl_getcontext (backscreen);
    //  gl_setclippingwindow (0, 0, MAPWIDTH - 1,
    //			MAPHEIGHT + 19);
    //  gl_setcontext (physicalscreen);
    //  gl_write (0, 16, "Calibrating delay loop");
    //
    //
    //  gl_setcontextvgavirtual (VGAMODE);
    //  background = my_allocatecontext ();
    //  gl_setclippingwindow (0, 0, MAPWIDTH - 1,
    //			MAPHEIGHT + 19);
    //  gl_getcontext (background);
    //  gl_setcontextvgavirtual (VGAMODE);
    //  starbackground = my_allocatecontext ();
    //  gl_setclippingwindow (0, 0, MAPWIDTH - 1,
    //			MAPHEIGHT + 19);
    //  gl_getcontext (starbackground);
    //  gl_setcontext (physicalscreen);
    //  gl_write (0, 24, "Initializing keyboard daemons");
    //  if (keyboard_init ())
    //    {
    //      printf ("Could not initialize keyboard.\n");
    //      exit (-1);
    //    }
    //  /*keyboard_translatekeys(TRANSLATE_CURSORKEYS | TRANSLATE_KEYPADENTER); */
    //  keyboard_translatekeys (0);
    //  gl_write (0, 32, "1 pc capable keyboard found");
}

void flippage(void) { /*DKS
                      int offset=MAPWIDTH * (MAPHEIGHT+20) * page,offset1=MAPWIDTH * (MAPHEIGHT+20)*(page^1);
                           if(physicalscreen->modeflags&MODEFLAG_FLIPPAGE_BANKALIGNED) {
                                offset=(offset+0xffff) & ~0xffff;
                                offset1=(offset+0xffff) & ~0xffff;
                            }
                            vga_setdisplaystart(offset);
                            gl_setscreenoffset(offset1);
                            page=page^1;
                            */

    // DKS - Update the screen
    if (SDL_Flip(physicalscreen) == -1) {
        printf("SDL page-flipping error\n");
        exit(-1);
    }

    //	//DKS - support screenshot command
    //	if (IsPressedScreenshot())
    //{
    //	take_screenshot();
    //}
}

// DKS to support taking a screenshot, pulled from ../rcfiles.c
static char *mygetenv(char *name) {
    static char name1[200];
    char *var = getenv(name);
    int i = strlen(var);
    if (i > 199)
        i = 199;
    memcpy(name1, var, i + 1);
    name1[199] = 0;
    return name1;
}

// void take_screenshot (void)
//{
//	static int seq = 0;	// start with sequence no. 000
//	//DKS
//	printf("Saving screenshot..\n");
//	char fullname[1024];
//	sprintf (fullname, "%s/KOULES%03d.BMP", mygetenv ("HOME"),seq++);
//	if (SDL_SaveBMP(physicalscreen,fullname))
//		printf("Error saving screenshot to file: %s\n",fullname);
//	else
//		printf("Screenshot saved to file: %s\n",fullname);
//}

void DrawText(SDL_Surface *passed_surface, int x, int y, char *text) {
    SDL_Rect tmp_rect;

    // Get offsets
    tmp_rect.x = x;
    tmp_rect.y = y;
    //  tmp_rect.w = SFont_TextWidth(fontwhite, text);
    //  tmp_rect.h = SFont_TextHeight(fontwhite);
    tmp_rect.w = strlen(text) * 8;
    tmp_rect.h = 8;

#ifdef SDL32BPP
    SDL_FillRect(passed_surface, &tmp_rect, colors_RGBA[0]);
#else
    SDL_FillRect(passed_surface, &tmp_rect, colors_16bit[0]);
#endif
    SFont_Write(passed_surface, fontwhite, x, y, text);
}

void PutBitmap(int x, int y, SDL_Surface *source, SDL_Surface *destination) {
    // Holds offsets
    SDL_Rect offset;

    // Get offsets
    offset.x = x;
    offset.y = y;

    // Blit
    SDL_BlitSurface(source, NULL, destination, &offset);
}

void sdl_fillrect(SDL_Surface *passed_surface, int x, int y, int x1, int y1, int color) {
    SDL_Rect tmp_rect;

    // Get offsets
    tmp_rect.x = x;
    tmp_rect.y = y;
    tmp_rect.w = x1 - x;
    tmp_rect.h = y1 - y;

#ifdef SDL32BPP
    SDL_FillRect(passed_surface, &tmp_rect, colors_RGBA[color]);
#else
    SDL_FillRect(passed_surface, &tmp_rect, colors_16bit[color]);
#endif
}

void sdl_rectangle(int x1, int y1, int x2, int y2, int color) {
    SDL_DrawLine(physicalscreen, x1, y1, x2, y1, colors_RGBA[color]);
    SDL_DrawLine(physicalscreen, x2, y1, x2, y2, colors_RGBA[color]);
    SDL_DrawLine(physicalscreen, x2, y2, x1, y2, colors_RGBA[color]);
    SDL_DrawLine(physicalscreen, x1, y2, x1, y1, colors_RGBA[color]);
}

void EnableClipping() {
    SDL_Rect cliprect;

    cliprect.x = 0;
    cliprect.y = 0;
    cliprect.w = MAPWIDTH;
    cliprect.h = MAPHEIGHT + 20;

    SDL_SetClipRect(physicalscreen, &cliprect);
}

void DisableClipping() {
    SDL_SetClipRect(physicalscreen, NULL);
}

void CopyVSToVS(SDL_Surface *source, SDL_Surface *destination) {
    SDL_Rect offset;
    offset.x = 0;
    offset.y = 0;
    SDL_BlitSurface(source, NULL, destination, &offset);
}

void CopyToScreen(SDL_Surface *source) {
    SDL_Rect offset;
    offset.x = 0;
    offset.y = 0;
    SDL_BlitSurface(source, NULL, physicalscreen, &offset);
}

void uninitialize() {
    static int uninitialized = 0;
    if (uninitialized)
        return;
    uninitialized = 1;
    /*  int             h, i;
       float           p = 0;
       char            bitmap1[MAPWIDTH][MAPHEIGHT + 20];
       char            bitmap2[MAPWIDTH][MAPHEIGHT + 20];
       if (!(physicalscreen->modeflags & (MODEFLAG_PAGEFLIPPING_ENABLED |
       MODEFLAG_TRIPLEBUFFERING_ENABLED))) {

       gl_enableclipping ();
       gl_setcontext (physicalscreen);
       gl_getbox (0, 0, MAPWIDTH, MAPHEIGHT + 20, bitmap1);
       for (h = (MAPHEIGHT + 20) / 2 - (MAPHEIGHT + 18) / 16; h >= 2; h -= (MAPHEIGHT + 18) / 16)
       {
       p += 64.0 / 8;
       gl_scalebox (MAPWIDTH, MAPHEIGHT + 20, bitmap1,
       MAPWIDTH, h * 2, bitmap2);
       gl_putbox (0, (MAPHEIGHT + 20) / 2 - h, MAPWIDTH, h * 2, bitmap2);
       gl_fillbox (0, (MAPHEIGHT + 20) / 2 - h - (MAPHEIGHT + 20) / 16, MAPWIDTH, (MAPHEIGHT + 20) / 16, 0);
       gl_fillbox (0, (MAPHEIGHT + 20) / 2 + h, MAPWIDTH, (MAPHEIGHT + 20) / 16, 0);
       setcustompalette ((int) p, 1);
       }
       gl_fillbox (0, (MAPHEIGHT + 20) / 2 - 50 - 2 / DIV, MAPWIDTH, 50, 0);
       gl_fillbox (0, (MAPHEIGHT + 20) / 2 + 2 / DIV, MAPWIDTH, 50, 0);
       for (i = MAPWIDTH / 2; i >= 5; i -= 5 / DIV)
       gl_fillbox (0, (MAPHEIGHT + 20) / 2 - 2, MAPWIDTH / 2 - i, 20, 0),
       gl_fillbox (MAPWIDTH / 2 + i, (MAPHEIGHT + 20) / 2 - 2, MAPWIDTH / 2 - i, 20, 0),
       usleep (500);
       } */

    /* DKS
    keyboard_close ();
    vga_setmode (TEXT);
    */

    // DKS
#ifdef GCW
    if (joy_gsensor)
        SDL_JoystickClose(joy_gsensor);
    if (joy_analog)
        SDL_JoystickClose(joy_analog);
#else
    if (joy)
        SDL_JoystickClose(joy);
#endif

    // Free the surfaces
    SDL_FreeSurface(background);
    SDL_FreeSurface(starbackground);
    SDL_FreeSurface(bball_bitmap);
    SDL_FreeSurface(apple_bitmap);
    SDL_FreeSurface(inspector_bitmap);
    SDL_FreeSurface(lunatic_bitmap);
    SDL_FreeSurface(bottom_bar);

    int surface_ctr;
    for (surface_ctr = 0; surface_ctr < NLETTERS; surface_ctr++)
        SDL_FreeSurface(lball_bitmap[surface_ctr]);

    SDL_FreeSurface(circle_bitmap);
    SDL_FreeSurface(hole_bitmap);
    SDL_FreeSurface(ball_bitmap);

    for (surface_ctr = 0; surface_ctr < MAXROCKETS; surface_ctr++) {
        SDL_FreeSurface(eye_bitmap[surface_ctr]);
        SDL_FreeSurface(rocket_bitmap[surface_ctr]);
    }

    SDL_FreeSurface(ehole_bitmap);

    SFont_FreeFont(fontblack);
    SFont_FreeFont(fontwhite);

#ifdef SOUND
    if (sndinit)
        kill_sound();
#endif

    printf("Life support systems disconected\n"
           "\n\nHave a nice LINUX!\n");
#ifdef NETSUPPORT
    if (client)
        CQuit("Game uninitialized\n");
#endif

    // Quit SDL
    SDL_Quit();

    // DKS
    //	return 0;  // this line make the compiler happy, but the call to execl() above never returns
    return; // this line make the compiler happy, but the call to execl() above never returns
}

// DKS - don't think we need this (GCW edition)
// static void
// uninitializes (int num)
//{
//  char            s[256];
//  sprintf (s, "Signal %i!!!\n", num);
//  CQuit (s);
//  uninitialize ();
//  exit (1);
//}

int main(int argc, char **argv) {
    // DKS  char            c;
    nrockets = 1;
    printf("\n\n\n\n"
           "                                The  game\n"
           "                               K O U L E S\n"
           "                               For svgalib\n"
           "                               Version:1.4\n"
           "                         SDL Port by Dan Silsby\n"
           "\n\n\n\n"
           "                    Copyright(c) Jan Hubicka 1995, 1996\n\n\n");

    // DKS
    //  while ((c = getopt (argc, argv, "fKWD:P:L:SC:slExMmdh")) != EOF)
    //    {
    //      switch (c)
    //	{
    //#ifdef NETSUPPORT
    //	case 'K':
    //	  server = 1;
    //	  servergameplan = DEATHMATCH;
    //	  break;
    //	case 'W':
    //	  server = 1;
    //	  GAMEHEIGHT = 360;
    //	  break;
    //	case 'D':
    //	  {
    //	    int             p;
    //	    server = 1;
    //	    if (sscanf (optarg, "%i", &p) != 1 || p < 0 || p > 4)
    //	      {
    //		printf ("-D : invalid difficulty\n");
    //		exit (0);
    //	      }
    //	    difficulty = p;
    //	  }
    //	  break;
    //	case 'P':
    //	  {
    //	    int             p;
    //	    if (sscanf (optarg, "%i", &p) != 1 || p < 0)
    //	      {
    //		printf ("-P : invalid port number\n");
    //		exit (0);
    //	      }
    //	    initport = p;
    //	  }
    //	  break;
    //	case 'L':
    //	  {
    //	    int             p;
    //	    server = 1;
    //	    if (sscanf (optarg, "%i", &p) != 1 || p < 1 || p > 100)
    //	      {
    //		printf ("-L : invalid level number\n");
    //		exit (0);
    //	      }
    //	    serverstartlevel = p - 1;
    //	  }
    //	  break;
    //	case 'S':
    //	  server = 1;
    //	  break;
    //	case 'C':
    //	  strcpy (servername, optarg);
    //	  client = 1;
    //	  break;
    //#else
    //	case 'K':
    //	case 'W':
    //	case 'P':
    //	case 'L':
    //	case 'D':
    //	case 'S':
    //	case 'C':
    //	case 'E':
    //	  printf ("Network option but no network support compiled\n");
    //	  break;
    //#endif
    //
    //#ifdef NETSUPPORT
    //	case 'E':
    //	  server = 1;
    //	  GAMEWIDTH = 900;
    //	  GAMEHEIGHT = 600;
    //	  MAPWIDTH = 900;
    //	  MAPHEIGHT = 600;
    //	  DIV = 2;
    //	  break;
    //#endif
    //
    //	//DKS
    ////	case 'f':
    ////	  flipping=1;
    ////	  break;
    //	case 's':
    //	  //VGAMODE = G320x200x256;
    //	  GAMEWIDTH = 640;
    //	  GAMEHEIGHT = 360;
    //	  MAPWIDTH = 320;
    //	  MAPHEIGHT = 180;
    //	  DIV = 2;
    //	  break;
    //	case 'l':
    //	  //VGAMODE = G640x480x256;
    //	  if (GAMEHEIGHT == 360)
    //	    GAMEHEIGHT = MAPHEIGHT = 360;
    //	  else
    //	    MAPHEIGHT = GAMEHEIGHT = 460;
    //	  GAMEWIDTH = 640;
    //	  MAPWIDTH = 640;
    //	  DIV = 1;
    //	  break;
    //	case 'x':
    //	  //VGAMODE = G320x240x256;
    //	  GAMEWIDTH = 640;
    //	  GAMEHEIGHT = 440;
    //	  MAPWIDTH = 320;
    //	  MAPHEIGHT = 220;
    //	  break;
    //#ifdef MOUSE
    //	case 'M':
    //	  nomouse = 1;
    //	  drawpointer = 0;
    //	  break;
    //#endif
    //#ifdef SOUND
    //	case 'd':
    //	  sndinit = 0;
    //	  break;
    //#endif
    //	default:
    //	  printf ("USAGE:\n"
    //		  " -h for help\n"
    //		  " -f enable experimental page flipping mode\n"
    //		  " -s for small display(320x200)\n"
    //		  " -l for large display(640x480)\n"
    //#ifdef SOUND
    //		  " -d Disable sound support\n"
    //#endif
    //#ifdef MOUSE
    //		  " -M disable mouse support\n"
    //#endif
    //#ifdef NETSUPPORT
    //		  " -S run koules as network server\n"
    //		  " -C<host> run koules as network client\n"
    //		  " -P<port> select port. Default is:%i\n"
    //		  " -W run server in width mode-support for 320x200 svgalib and OS/2 clients\n"
    //		  " -L<level> select level for server\n"
    //		  " -D<number> select dificulty for server:\n"
    //		  "     0: nightmare\n"
    //		  "     1: hard\n"
    //		  "     2: medium(default and recomended)\n"
    //		  "     3: easy\n"
    //		  "     4: very easy\n"
    //		  " -K run server in deathmatch mode\n", DEFAULTINITPORT
    //#endif
    //
    //	    );
    //	  exit (2);
    //	}
    //    }
    //  srand (time (NULL));
    //#ifdef NETSUPPORT
    //  if (server)
    //    {
    //      init_server ();
    //      server_loop ();
    //    }
    //  if (client)
    //    {
    //      init_client ();
    //      atexit (uninitialize);
    //      signal (SIGHUP, uninitializes);
    //      signal (SIGINT, uninitializes);
    //      signal (SIGTRAP, uninitializes);
    //      signal (SIGABRT, uninitializes);
    //      signal (SIGSEGV, uninitializes);
    //      signal (SIGQUIT, uninitializes);
    //      signal (SIGFPE, uninitializes);
    //      signal (SIGTERM, uninitializes);
    //      signal (SIGBUS, uninitializes);
    //      signal (SIGIOT, uninitializes);
    //      signal (SIGILL, uninitializes);
    //      MAPWIDTH = GAMEWIDTH / DIV;
    //      MAPHEIGHT = GAMEHEIGHT / DIV;
    //
    //    }
    //#endif
    srand(time(NULL));
    printf("LINUX4GW 1.12.45b professional\n");
    printf("Copyright(c)1991,1992,1993,1994,1995 Jan Hubicka(JAHUSOFT)\n");

    // DKS - had to swap order of these (create_bitmap now demands it)
    initialize();
    create_bitmap();

    // DKS
    // gl_write (0, 40, "Initializing GUI user interface");
#ifdef SOUND
    sound = sndinit;
#endif
    gamemode = MENU;
    // DKS
    //  gl_write (0, 48, "Initializing 4d rotation tables");
    //  gl_write (0, 56, "Initializing refresh daemon ");
    //  gl_write (0, 66, "please wait 12043.21 Bogomipseconds");
    drawstarbackground();
    drawbackground();
    drawbottom_bar();
    // DKS
    // gl_setfont (8, 8, fontblack);

    //  keys[0][0] = SCANCODE_CURSORBLOCKUP;
    //  keys[0][1] = SCANCODE_CURSORBLOCKDOWN;
    //  keys[0][2] = SCANCODE_CURSORBLOCKLEFT;
    //  keys[0][3] = SCANCODE_CURSORBLOCKRIGHT;
    //
    //  keys[1][0] = SCANCODE_CURSORUP;
    //  keys[1][1] = SCANCODE_CURSORDOWN;
    //  keys[1][2] = SCANCODE_CURSORLEFT;
    //  keys[1][3] = SCANCODE_CURSORRIGHT;
    //

    // DKS - disabling this key array handling completely, everything should now be done with new func's
    // control_pressed(), etc
    //  keys[0][0] = GP2X_BUTTON_UP;
    //  keys[0][1] = GP2X_BUTTON_DOWN;
    //  keys[0][2] = GP2X_BUTTON_LEFT;
    //  keys[0][3] = GP2X_BUTTON_RIGHT;
    //
    //  keys[1][0] = GP2X_BUTTON_UP;
    //  keys[1][1] = GP2X_BUTTON_DOWN;
    //  keys[1][2] = GP2X_BUTTON_LEFT;
    //  keys[1][3] = GP2X_BUTTON_RIGHT;

    starwars();

#ifdef NETSUPPORT
    if (client) {
        vga_runinbackground(1);
        client_loop();
    } else
#endif

        game();

    printf("uninitializing\n");
    uninitialize();
    return 0;
}
