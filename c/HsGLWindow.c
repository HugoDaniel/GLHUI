#include "HsGLWindow.h"

static Display *display;
static Window window;
static GLXContext context;
static int width, height, scrwidth, scrheight;
static unsigned frame = 0;
static double ftime, dtime;
static int fullscreen = 0;

typedef struct
{
    unsigned long	flags;
    unsigned long	functions;
    unsigned long	decorations;
    long			inputMode;
    unsigned long	status;
} Hints;

// OpenGL 3 specific functions:
// GLXContext glXCreateContextAttribsARB (Display *, GLXFBConfig, GLXContext, Bool direct, const int *);
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
typedef Bool (*glXMakeContextCurrentARBProc)(Display*, GLXDrawable, GLXDrawable, GLXContext);
static glXCreateContextAttribsARBProc createContext = 0;
static glXMakeContextCurrentARBProc makeContextCurrent = 0;

void WindowSetFullscreen(int f)
{
	XWindowChanges changes;
	Hints hints;
	
	Atom property = XInternAtom(display,"_MOTIF_WM_HINTS",True);

	hints.flags = 2;

	if(f) {	
		hints.decorations = False;
	
		changes.x = 0;
		changes.y = 0;
		changes.width = width = scrwidth;
		changes.height = height = scrheight;
		changes.stack_mode = Above;

		fullscreen = 1;
	} else {
		hints.decorations = True;

		changes.x = scrwidth/4;
		changes.y = scrheight/4;
		changes.width = width = scrwidth/2;
		changes.height = height = scrheight/2;
		changes.stack_mode = Above;

		fullscreen = 0;
	}

	XChangeProperty(display, window, property, property, 32, PropModeReplace, (unsigned char *)&hints, 5);
	XConfigureWindow(display, window, CWX | CWY | CWWidth | CWHeight | CWStackMode, &changes);

	//XGrabKeyboard(display, window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	//XGrabPointer(display, window, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, window, None, CurrentTime);
	//XMapRaised(display, window);
}

// GL Attributes
static const int basicAttribs[] = { GLX_RGBA
                                  , GLX_DEPTH_SIZE, 24
                                  , GLX_DOUBLEBUFFER
                                  , None 
                                  };
static const int gl3Attribs[] = { GLX_RENDER_TYPE, GLX_RGBA_BIT
                                , GLX_X_RENDERABLE, True
                                , GLX_DOUBLEBUFFER, 1
                                , GLX_RED_SIZE, 8
                                , GLX_GREEN_SIZE, 8
                                , GLX_BLUE_SIZE, 8
                                , None
                                };


typedef struct {
    unsigned int major, minor, GLSLMajor, GLSLMinor;
} support;

// This function creates a throw away context that is only used to query
// for support of specific features, like what version of opengl is supported

// the minimal context needs a display and a visual;
support _initSupport(Display *dpy) {
    XVisualInfo *vi = glXChooseVisual(dpy, 0, (int*)basicAttribs);
    if(vi == NULL) { 
        fprintf(stderr, "Unable to create the throw away context");
        fprintf(stderr, "No appropriate visual found");
        // TODO: return invalid support
    }

    Window                  root;
    Colormap                cmap;
    XSetWindowAttributes    swa;
    Window                  win;
    GLXContext              glc;

    root = DefaultRootWindow(dpy);
    cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.colormap = cmap;
    win = XCreateWindow(dpy, root, 0, 0, 16, 16, 0, vi->depth, InputOutput, vi->visual, CWColormap, &swa);

    glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    glXMakeCurrent(dpy, win, glc);
    char const * version = glGetString(GL_VERSION);
    unsigned int major = version[0] - '0';
    unsigned int minor = version[2] - '0';
    unsigned int glslMajor = 0;
    unsigned int glslMinor = 0;
    if(major >= 2) {
        char const * glsl = glGetString(GL_SHADING_LANGUAGE_VERSION);
        glslMajor = glsl[0] - '0';
        glslMinor = glsl[2] - '0';
    }
	// printf("TMP CONTEXT: OpenGL:\n\tvendor %s\n\trenderer %s\n\tversion %s\n\tshader language %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
    /* get the required extensions */
    createContext = (glXCreateContextAttribsARBProc)glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB");
    makeContextCurrent = (glXMakeContextCurrentARBProc)glXGetProcAddressARB( (const GLubyte *) "glXMakeContextCurrent");
    if ( !(createContext && makeContextCurrent) && major > 2){
        fprintf(stderr, "missing support for GLX_ARB_create_context\n");
        // TODO: return invalid support
    }

	glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);

    support s = { major, minor, glslMajor, glslMinor };
    return s;
}

int WindowInit(unsigned int const major, unsigned int const minor)
{
    display = XOpenDisplay(NULL);
	if(display == NULL) {
		puts("Cannot connect to the X server.\n");
		return -1;
	}
    support s = _initSupport(display); // check what the system supports
    // printf("Opengl Major: %d, Opengl Minor: %d, GLSL Major: %d, GLSL Minor %d\n", s.openGLMajor, s.openGLMinor, s.GLSLMajor, s.GLSLMinor);
	int nscreen = DefaultScreen(display);
	scrwidth = XDisplayWidth(display, nscreen);
	scrheight = XDisplayHeight(display, nscreen);
	
	width = scrwidth / 2;
	height = scrheight / 2;

	int const * glattr;
    if(s.major > 2) {
        glattr = gl3Attribs;
    } else {
        glattr = basicAttribs;
    }
	XVisualInfo *visual = glXChooseVisual(display, nscreen, (int*)glattr);
	if(visual == NULL) {
		puts("Could not find a visual.\n");
		return -2;
	}
	
    if(major >= 3) {
        int elemc;
        GLXFBConfig *fbcfg = glXChooseFBConfig(display, nscreen, glattr, &elemc);
        if(fbcfg == NULL) {
            puts("Could not find FB config.\n");
            return -3;
        }
        int gl3attr[] = { GLX_CONTEXT_MAJOR_VERSION_ARB, major
                        , GLX_CONTEXT_MINOR_VERSION_ARB, minor
                        , GLX_CONTEXT_FLAGS_ARB
                        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
                        , None
                        };
        context = createContext(display, fbcfg[0], 0, 1, gl3attr);
        XFree(fbcfg);
    } else {
        context = glXCreateContext(display, visual, NULL, GL_TRUE);
    }

	Window root = RootWindow(display, visual->screen);

	XSetWindowAttributes xattr;
	xattr.event_mask = KeyPressMask | ButtonPressMask | StructureNotifyMask;
	xattr.colormap = XCreateColormap(display, root, visual->visual, AllocNone);

	window = XCreateWindow(display, root, 0, 0, width, height, 0, visual->depth, InputOutput, visual->visual, CWColormap | CWEventMask, &xattr);
	XSetStandardProperties(display, window, "United Colors Of Funk", 0, None, 0, 0, 0);

	XGrabKeyboard(display, window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	XGrabPointer(display, window, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, window, None, CurrentTime);
	XMapRaised(display, window);

	glXMakeCurrent(display, window, context);
	glViewport(0, 0, width, height);

	XFree(visual);

	printf("OpenGL:\n\tvendor %s\n\trenderer %s\n\tversion %s\n\tshader language %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

	return 1;
}


void WindowLoop(void (*Loop)())
{
	struct timespec ts[3];	// even, odd, initial
	int done = 0;
	XEvent event;

	clock_gettime(CLOCK_MONOTONIC, ts+2);
	ts[0] = ts[1] = ts[2];

	while(!done)
	{
		while(XPending(display) > 0)
		{
			XNextEvent(display, &event);
			switch(event.type)
			{
			case ConfigureNotify:
				if((event.xconfigure.width != width) || (event.xconfigure.height != height))
				{
					width = event.xconfigure.width;
					height = event.xconfigure.height;
					glViewport(0, 0, width, height);
				}
				break;

			case ButtonPress:
				done = 1;
				break;

			case KeyPress:
				if(XLookupKeysym(&event.xkey, 0) == XK_Escape)
					done = 1;
				else if(XLookupKeysym(&event.xkey, 0) == XK_F1)
					WindowSetFullscreen(!fullscreen);
				break;
			}
		}

		Loop();
		glXSwapBuffers(display, window);
		frame++;
		clock_gettime(CLOCK_MONOTONIC, ts+(frame%2));
		dtime = 1e-9*(ts[frame%2].tv_nsec - ts[!(frame%2)].tv_nsec) + (ts[frame%2].tv_sec - ts[!(frame%2)].tv_sec);
		ftime = 1e-9*(ts[frame%2].tv_nsec - ts[2].tv_nsec) + (ts[frame%2].tv_sec - ts[2].tv_sec);
	}

	//WindowKill(0);
}


void WindowKill()
{
    puts("Bye.");
	if(context) {
        XDestroyWindow(display, window);
		glXMakeCurrent(display, 0, 0);
		glXDestroyContext(display, context);
		context = 0;
	}

	XCloseDisplay(display);
}


unsigned WindowFrame(){ return frame; }
double WindowTime(){ return ftime; }
double WindowDTime(){ return dtime; }
unsigned WindowWidth(){ return width; }
unsigned WindowHeight(){ return height; }
unsigned WindowScrWidth(){ return scrwidth; }
unsigned WindowScrHeight(){ return scrheight; }
