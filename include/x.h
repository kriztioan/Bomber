
#ifndef BOMBER_X_H
#define BOMBER_X_H

/*
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/cursorfont.h>
#include <X11/Xutil.h>
*/
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "bomber.h"

#define KEYMAX 128


extern Display *dp;
extern GC copygc,andgc,orgc,xorgc;
extern int usedcolors;
extern uchar mymap[];
extern int screen;
extern Window wi;
extern int fontbase,fontysize;
extern XImage *myimage;
extern char *imageloc;
extern XEvent event;
extern XKeyEvent *keyevent;
extern XButtonEvent *buttonevent;
extern XMotionEvent *motionevent;
extern XExposeEvent *exposeevent;

extern Pixmap offscreen,storeage;
extern long map2[];
extern Colormap cmap,mycolormap;
extern uchar fmap[128];
extern int buttonstate,buttondown;
extern int mxpos,mypos;

extern int pressedcodes[KEYMAX],downcodes[KEYMAX],numpressed,numdown;


extern void dumpgfx();
extern void createinout(struct gfxset *);
extern void getcolors();
extern void gfxfetch(struct gfxset *,struct figure *,int);
extern void puttile(int destx,int desty,int source);
extern void store(int x,int y,int which);
extern void restore(int x,int y,int which);
extern void copyup();
extern void copyupxy(int x,int y);
extern void copyupxysize(int x,int y,int xsize,int ysize);
extern void getfigures();
extern unsigned long getcolor(char *name);  /* unsigned long */
extern void openx(int argc, char **argv);
extern void closex();
extern int checkpressed(int code);
extern int checkdown(int code);
extern int checkbutton(int button);
extern int checkbuttondown(int button);
extern int anydown();
extern int firstdown();
extern void scaninput();
extern void fontinit();
extern void writechar(int x,int y,uchar ch);
extern void clear();
extern void xflush(); 
extern void xsync();
extern void drawbox(int x,int y,int size,int color);
extern void drawbox2(int x,int y,int sizex,int sizey,int color);
extern void drawfillrect(int x,int y,int size,int color);
extern void bigpixel(int x,int y,int color);
extern void invert(int x,int y);
extern int getmousex();
extern int getmousey();
extern void drawsquare(int x,int y,uchar *source);
extern void colormapon();
extern void colormapoff();
extern void palette(uchar *pal);
extern void drawfigure(int x,int y,figure *fig);
extern void drawfigureany(int x,int y,figure *fig,figure *dest);
void solidcopy(figure *fig,int destx,int desty,int sizex,int sizey);
void solidcopyany(figure *fig,figure *dest,int destx,int desty,int sizex,int sizey);

#endif // BOMBER_X_H
