#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES

#include <stdio.h>
#include <time.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

int WindowInit(unsigned int const major, unsigned int const minor);
void WindowKill();

void WindowSetFullscreen(int f);

unsigned WindowFrame();
double WindowTime();
double WindowDTime();

unsigned WindowWidth();
unsigned WindowHeight();
unsigned WindowScrWidth();
unsigned WindowScrHeight();

void WindowLoop(void (*)());
