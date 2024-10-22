/**
 *  @file   x.c
 *  @brief  X11 Graphics
 *  @author David Ashley (dash@xdr.com)
 *  @date   1999-02-07
 *  @note   GPL licensed
 *
 ***********************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <x.h>
#define MAXCOLORS 256

Display *dp;
GC copygc, andgc, orgc, xorgc;
int usedcolors = 0;
int allocatedcolors = 0;
uchar mymap[3 * MAXCOLORS];
int screen;
Window wi;
XImage *myimage;
char *imageloc;
XEvent event;
XKeyEvent *keyevent;
XButtonEvent *buttonevent;
XMotionEvent *motionevent;
XExposeEvent *exposeevent;

Pixmap offscreen;

#define MAXGFX 20
#define SETSIZEX 512
#define SETSIZEY 512
typedef struct gfxres {
  Pixmap graphics;
  Pixmap mask;
  int curx, cury;
  int maxy;
} gfxres;

gfxres gfxresources[MAXGFX];
int resourcesused = 0;

long map2[MAXCOLORS];
Colormap cmap, mycolormap;
int buttonstate = 0, buttondown = 0;
int mxpos, mypos;

int pressedcodes[KEYMAX], downcodes[KEYMAX], numpressed, numdown;

void xtest(int n) {
  n %= resourcesused;
  XCopyArea(dp, gfxresources[n].graphics, offscreen, copygc, 0, 0, 512, 480, 0,
            0);
  copyup();
}

void dumpgfx() { usedcolors = 0; }
int bestmatch(int red, int green, int blue) {
  unsigned char *p;
  int i;
  int bestcolor, bestdelta;
  int rdelta, gdelta, bdelta;
  int delta;

  p = mymap;
  i = 0;
  bestcolor = -1;
  while (i < usedcolors) {
    rdelta = *p++;
    rdelta -= red;
    rdelta *= rdelta;
    gdelta = *p++;
    gdelta -= green;
    gdelta *= gdelta;
    bdelta = *p++;
    bdelta -= blue;
    bdelta *= bdelta;
    delta = rdelta + gdelta + bdelta;
    if (bestcolor < 0 || delta < bestdelta) {
      bestdelta = delta;
      bestcolor = i;
    }
    i++;
  }
  return bestcolor;
}

void createinout(gfxset *gs) {
  uchar *p;
  int i, j, counts[256];
  uchar red, green, blue;
  int cnt;

  p = gs->gs_pic;
  for (i = 0; i < 256; i++)
    counts[i] = 0;
  i = gs->gs_xsize * gs->gs_ysize;
  while (i--)
    counts[*p++]++;
  cnt = 0;
  gs->gs_inout[0] = 0;
  for (i = 1; i < 256; i++) {
    if (counts[i]) {
      cnt++;
      p = gs->gs_colormap + i + i + i;
      red = *p++;
      green = *p++;
      blue = *p++;
      p = mymap;
      for (j = 0; j < usedcolors; j++, p += 3) {
        if (red == *p && green == p[1] && blue == p[2]) {
          gs->gs_inout[i] = j;
          break;
        }
      }
      if (j == usedcolors) {
        if (usedcolors < MAXCOLORS) {
          *p++ = red;
          *p++ = green;
          *p++ = blue;
          gs->gs_inout[i] = usedcolors;
          ++usedcolors;
        } else
          gs->gs_inout[i] = bestmatch(red, green, blue);
      }
    }
  }
  getcolors();
}

void getcolors() {
  int i;
  XColor ac;
  uchar *p;
  int res;
  p = mymap + allocatedcolors * 3;
  for (i = allocatedcolors; i < usedcolors; i++) {
    ac.red = (*p++) << 8;
    ac.green = (*p++) << 8;
    ac.blue = (*p++) << 8;
    res = XAllocColor(dp, cmap, &ac);
    if (!res)
      printf("Failed on color %d\n", i);
    map2[i] = ac.pixel;
  }
  allocatedcolors = usedcolors;
}

void gfxfetchsingle(gfxset *gs, figure *fig, int sourcex, int sourcey,
                    int sizex, int sizey) {
  uchar *p, *p2;
  int dx, dy;
  uchar ch;
  unsigned long pixel;
  uchar *map1;
  int gswidth;
  int minx, miny, maxx, maxy;
  int tx, ty;
  Pixmap pm;

  map1 = gs->gs_inout;
  gswidth = gs->gs_xsize;
  p = gs->gs_pic + sourcex + gswidth * sourcey;
  minx = miny = maxx = maxy = -1;
  for (dy = 0; dy < sizey; dy++) {
    p2 = p;
    ty = sourcey + dy;
    for (dx = 0; dx < sizex; dx++) {
      if (!*p2++)
        continue;
      if (miny == -1 || ty < miny)
        miny = ty;
      if (maxy == -1 || ty > maxy)
        maxy = ty;
      tx = sourcex + dx;
      if (minx == -1 || tx < minx)
        minx = tx;
      if (maxx == -1 || tx > maxx)
        maxx = tx;
    }
    p += gswidth;
  }

  if (minx == -1) {
    minx = maxx = sourcex;
    miny = maxy = sourcey;
  }

  fig->xdelta = minx - sourcex;
  fig->ydelta = miny - sourcey;

  sourcex = minx;
  sourcey = miny;
  sizex = maxx - minx + 1;
  sizey = maxy - miny + 1;

  fig->xsize = sizex;
  fig->ysize = sizey;

  if (getspace(fig)) {
    printf("Failure to get space for figure\n");
    exit(2);
  }

  p = gs->gs_pic + sourcex + gswidth * sourcey;
  for (dy = 0; dy < sizey; dy++) {
    p2 = p;
    for (dx = 0; dx < sizex; dx++) {
      pixel = *p2++;
      if (pixel)
        pixel = map2[map1[pixel]];
      XPutPixel(myimage, dx, dy, pixel);
    }
    p += gswidth;
  }
  p -= gswidth * sizey;
  XPutImage(dp, fig->graphics, copygc, myimage, 0, 0, fig->xpos, fig->ypos,
            sizex, sizey);
  for (dy = 0; dy < sizey; dy++) {
    p2 = p;
    for (dx = 0; dx < sizex; dx++) {
      ch = *p2++;
      XPutPixel(myimage, dx, dy, ch ? 0L : 0xffffffffL);
    }
    p += gswidth;
  }
  XPutImage(dp, fig->mask, copygc, myimage, 0, 0, fig->xpos, fig->ypos, sizex,
            sizey);
}
//(gfxset *gs,figure *fig,int sourcex,int sourcey,int sizex,int sizey)
void gfxfetch(gfxset *gs, figure *fig, int num) {
  int x, y;
  int xsize, ysize;
  int fxsize, fysize;
  unsigned char *p, *p2;
  int i, j, k;

  xsize = gs->gs_xsize;
  ysize = gs->gs_ysize;
  p2 = p = gs->gs_pic + xsize + 1;
  fxsize = 2;
  while (*p++ == 0)
    ++fxsize;
  fysize = 2;
  while (*p2 == 0)
    ++fysize, p2 += xsize;
  x = fxsize;
  y = 0;
  while (num--) {
    gfxfetchsingle(gs, fig, x, y, fxsize, fysize);
    x += fxsize;
    if (x > xsize - fxsize) {
      x = 0;
      y += fysize;
      if (y > ysize - fysize)
        y = 0;
    }
    fig++;
  }
}

void solidfetch(gfxset *gs, figure *fig) {
  int depth;
  int xsize, ysize;
  int i, j;
  unsigned char *p, *p2, *map;
  int x, y;
  int bitex, bitey;
  long gfx;
  int c;

  memset(fig, 0, sizeof(struct figure));
  depth = DefaultDepth(dp, screen);
  xsize = gs->gs_xsize;
  ysize = gs->gs_ysize;

  gfx = fig->graphics = XCreatePixmap(dp, wi, xsize, ysize, depth);
  if (!gfx)
    return;
  fig->xsize = xsize;
  fig->ysize = ysize;
  map = gs->gs_inout;
  for (y = 0; y < ysize; y += 64) {
    bitey = ysize - y;
    if (bitey > 64)
      bitey = 64;
    for (x = 0; x < xsize; x += 64) {
      bitex = xsize - x;
      if (bitex > 64)
        bitex = 64;
      p = gs->gs_pic + y * xsize + x;
      for (j = 0; j < bitey; ++j) {
        p2 = p;
        for (i = 0; i < bitex; ++i) {
          c = *p2++;
          if (c)
            c = map2[map[c]];
          XPutPixel(myimage, i, j, c);
        }
        p += xsize;
      }
      XPutImage(dp, gfx, copygc, myimage, 0, 0, x, y, bitex, bitey);
    }
  }
}
void solidcopy(figure *fig, int destx, int desty, int sizex, int sizey) {
  solidcopyany(fig, 0, destx, desty, sizex, sizey);
}
void solidcopyany(figure *fig, figure *dest, int destx, int desty, int sizex,
                  int sizey) {
  int xmax, ymax;
  Pixmap destpixmap;

  destpixmap = dest ? dest->graphics : offscreen;
  xmax = fig->xsize;
  ymax = fig->ysize;
  if (destx >= xmax || desty >= ymax || destx + sizex <= 0 ||
      desty + sizey <= 0)
    return;
  if (destx < 0) {
    sizex += destx;
    destx = 0;
  }
  if (desty < 0) {
    sizey += desty;
    desty = 0;
  }
  if (destx + sizex > xmax)
    sizex = xmax - destx;
  if (desty + sizey > ymax)
    sizey = ymax - desty;

  XCopyArea(dp, fig->graphics, destpixmap, copygc, destx, desty, sizex, sizey,
            destx, desty);
}

void drawfigure(int destx, int desty, figure *fig) {
  int sizex, sizey;
  int sourcex, sourcey;

  destx += fig->xdelta;
  desty += fig->ydelta;
  sizex = fig->xsize;
  sizey = fig->ysize;
  sourcex = fig->xpos;
  sourcey = fig->ypos;
  if (destx >= IXSIZE || desty >= IYSIZE || destx + sizex <= 0 ||
      desty + sizey <= 0)
    return;
  if (destx < 0) {
    sizex += destx;
    sourcex -= destx;
    destx = 0;
  }
  if (desty < 0) {
    sizey += desty;
    sourcey -= desty;
    desty = 0;
  }
  if (destx + sizex > IXSIZE)
    sizex = IXSIZE - destx;
  if (desty + sizey > IYSIZE)
    sizey = IYSIZE - desty;

  XCopyArea(dp, fig->mask, offscreen, andgc, sourcex, sourcey, sizex, sizey,
            destx, desty);
  XCopyArea(dp, fig->graphics, offscreen, orgc, sourcex, sourcey, sizex, sizey,
            destx, desty);
}
void drawfigureany(int destx, int desty, figure *fig, figure *dest) {
  int sizex, sizey;
  int sourcex, sourcey;
  Pixmap destpix;
  int maxx, maxy;

  destpix = dest->graphics;
  maxx = dest->xsize;
  maxy = dest->ysize;

  destx += fig->xdelta;
  desty += fig->ydelta;
  sizex = fig->xsize;
  sizey = fig->ysize;
  sourcex = fig->xpos;
  sourcey = fig->ypos;
  if (destx >= maxx || desty >= maxy || destx + sizex <= 0 ||
      desty + sizey <= 0)
    return;
  if (destx < 0) {
    sizex += destx;
    sourcex -= destx;
    destx = 0;
  }
  if (desty < 0) {
    sizey += desty;
    sourcey -= desty;
    desty = 0;
  }
  if (destx + sizex > maxx)
    sizex = maxx - destx;
  if (desty + sizey > maxy)
    sizey = maxy - desty;

  XCopyArea(dp, fig->mask, destpix, andgc, sourcex, sourcey, sizex, sizey,
            destx, desty);
  XCopyArea(dp, fig->graphics, destpix, orgc, sourcex, sourcey, sizex, sizey,
            destx, desty);
}

void copyup() {
  XCopyArea(dp, offscreen, wi, copygc, 0, 0, IXSIZE, IYSIZE, 0, 0);
  needwhole = 0;
}

void copyupxy(int x, int y) {
  XCopyArea(dp, offscreen, wi, copygc, x, y, 24, 24, x, y);
}

void copyupxysize(int x, int y, int xsize, int ysize) {
  XCopyArea(dp, offscreen, wi, copygc, x, y, xsize, ysize, x, y);
}

unsigned long getcolor(char *name) {
  XColor color1, color2;

  if (!XAllocNamedColor(dp, cmap, name, &color1, &color2)) {
    printf("Couldn't allocate color %s\n", name);
    exit(4);
  }
  return color1.pixel;
}

void openx(int argc, char **argv) {
  XGCValues values;
  XSizeHints *sizehints;
  XWMHints *wmhints;
  XClassHint *classhints;
  XTextProperty windowName;
  XTextProperty iconName;
  char *title = "XbomB";
  char *title1 = "XBomb";
  int depth;

  dp = XOpenDisplay(0);
  if (!dp) {
    printf("Cannot open display\n");
    exit(1);
  }
  screen = XDefaultScreen(dp);
  cmap = DefaultColormap(dp, screen);
  depth = DefaultDepth(dp, screen);
  wi = XCreateSimpleWindow(dp, RootWindow(dp, screen), 20, 20, IXSIZE, IYSIZE,
                           0, 0, 0);

  wmhints = XAllocWMHints();
  /* Various window manager settings */
  wmhints->initial_state = NormalState;
  wmhints->input = True;
  wmhints->flags |= StateHint | InputHint;
  wmhints->icon_pixmap = /*create_icon ()*/ 0;
  wmhints->flags = IconPixmapHint;

  /* Set the class for this program */
  classhints = XAllocClassHint();
  classhints->res_name = title1;
  classhints->res_class = title1;
  /* Setup the max and minimum size that the window will be */
  sizehints = XAllocSizeHints();
  sizehints->flags = PSize | PMinSize | PMaxSize;
  sizehints->min_width = IXSIZE;
  sizehints->min_height = IYSIZE;
  sizehints->max_width = IXSIZE;
  sizehints->max_height = IYSIZE;
  /* Create the window/icon name properties */
  if (XStringListToTextProperty(&title, 1, &windowName) == 0) {
    fprintf(stderr, "X: Cannot create window name resource!\n");
    exit(3);
  }
  if (XStringListToTextProperty(&title1, 1, &iconName) == 0) {
    fprintf(stderr, "X: Cannot create window name resource!\n");
    exit(3);
  }
  /* Now set the window manager properties */
  XSetWMProperties(dp, wi, &windowName, &iconName, argv, argc, sizehints,
                   wmhints, classhints);
  XFree((void *)wmhints);
  XFree((void *)classhints);
  XFree((void *)sizehints);

  XMapWindow(dp, wi);
  XSelectInput(dp, wi,
               KeyPressMask | KeyReleaseMask | ButtonPressMask |
                   ButtonReleaseMask | PointerMotionMask | ExposureMask |
                   FocusChangeMask);

  copygc = XCreateGC(dp, wi, 0, NULL);
  values.function = GXor;
  orgc = XCreateGC(dp, wi, GCFunction, &values);
  values.function = GXand;
  andgc = XCreateGC(dp, wi, GCFunction, &values);
  values.function = GXxor;
  xorgc = XCreateGC(dp, wi, GCFunction, &values);
  imageloc = malloc(128 * 128 * 4);
  myimage = XCreateImage(dp, DefaultVisual(dp, screen), depth, ZPixmap, 0,
                         imageloc, 128, 128, 8, 0);
  offscreen = XCreatePixmap(dp, wi, 640, 480, depth);
  keyevent = (XKeyEvent *)&event;
  buttonevent = (XButtonEvent *)&event;
  motionevent = (XMotionEvent *)&event;
  exposeevent = (XExposeEvent *)&event;
  numpressed = numdown = 0;
  if (allocgfxres()) {
    printf("allocgfxres() failure\n");
    exit(1);
  }
}

int allocgfxres() {
  int depth;
  Pixmap graphics, mask;

  if (resourcesused == MAXGFX)
    return 1;
  depth = DefaultDepth(dp, screen);
  graphics = XCreatePixmap(dp, wi, SETSIZEX, SETSIZEY, depth);
  mask = XCreatePixmap(dp, wi, SETSIZEX, SETSIZEY, depth);
  if (!graphics || !mask) {
    if (graphics) {
      XFreePixmap(dp, graphics);
      graphics = 0;
    }
    if (mask) {
      XFreePixmap(dp, mask);
      mask = 0;
    }
    return 1;
  }
  gfxresources[resourcesused].graphics = graphics;
  gfxresources[resourcesused].mask = mask;
  gfxresources[resourcesused].curx = 0;
  gfxresources[resourcesused].cury = 0;
  gfxresources[resourcesused].maxy = 0;
  ++resourcesused;
  return 0;
}

int getspace(figure *fig) {
  int xsize, ysize;
  int i;
  gfxres *gr;
  int used;

  xsize = fig->xsize;
  ysize = fig->ysize;
  gr = gfxresources;
  used = resourcesused;
  for (i = 0; i <= used; ++i, ++gr) {
    if (i == used && allocgfxres())
      return 1;
    if (gr->cury + ysize > SETSIZEY)
      continue;
    if (gr->curx + xsize > SETSIZEX && gr->maxy + ysize > SETSIZEY)
      continue;
    if (gr->curx + xsize > SETSIZEX) {
      gr->curx = 0;
      gr->cury = gr->maxy;
    }
    fig->xpos = gr->curx;
    fig->ypos = gr->cury;
    fig->graphics = gr->graphics;
    fig->mask = gr->mask;
    gr->curx += xsize;
    if (gr->cury + ysize > gr->maxy)
      gr->maxy = gr->cury + ysize;
    return 0;
  }
  return 1;
}

void closex() {
  XFreeGC(dp, copygc);
  XFreeGC(dp, andgc);
  XFreeGC(dp, orgc);
  XFreeGC(dp, xorgc);
  XDestroyWindow(dp, wi);
  XCloseDisplay(dp);
}

int checkpressed(int code) {
  int *p, i;
  i = numpressed;
  p = pressedcodes;
  while (i--)
    if (*p++ == code)
      return 1;
  return 0;
}

int checkdown(int code) {
  int *p, i;
  i = numdown;
  p = downcodes;
  while (i--)
    if (*p++ == code)
      return 1;
  return 0;
}

int checkbutton(int button) { return buttonstate & (1 << button); }

int checkbuttondown(int button) { return buttondown & (1 << button); }

int anydown() { return numdown; }

int takedown() {
  int res = 0;

  if (numdown) {
    res = *downcodes;
    --numdown;
    if (numdown)
      memmove(downcodes, downcodes + 1, numdown * sizeof(int));
  }
  return res;
}
int firstdown() { return *downcodes; }

void scaninput() {
  int i, *ip, code;
  numdown = 0;
  buttondown = 0;
  while (XCheckMaskEvent(dp, ~0, &event))
    switch (event.type) {
    case KeyPress:
      code = XLookupKeysym(keyevent, 0);
      if (numdown < KEYMAX)
        downcodes[numdown++] = code;
      ip = pressedcodes;
      i = numpressed;
      while (i)
        if (*ip++ == code)
          break;
        else
          i--;
      if (!i && numpressed < KEYMAX)
        pressedcodes[numpressed++] = code;
      break;
    case KeyRelease:
      code = XLookupKeysym(keyevent, 0);
      i = numpressed;
      ip = pressedcodes;
      while (i)
        if (*ip++ == code) {
          *--ip = pressedcodes[--numpressed];
          break;
        } else
          i--;
      break;
    case ButtonPress:
      i = 1 << buttonevent->button;
      buttonstate |= i;
      buttondown |= i;
      break;
    case ButtonRelease:
      buttonstate &= ~(1 << buttonevent->button);
      break;
    case MotionNotify:
      mxpos = motionevent->x;
      mypos = motionevent->y;
      break;
    case Expose:
      copyup();
      needwhole = 0;
      break;
    case FocusOut:
      numpressed = 0;
      break;
    }
}

void greyrect(int x, int y, int xsize, int ysize) {
  static int greycolor = -1;
  if (greycolor == -1)
    greycolor = map2[bestmatch(0, 0, 0x70)];
  XSetForeground(dp, copygc, greycolor);
  XFillRectangle(dp, offscreen, copygc, x, y, xsize, ysize);
}

void clear() {
  XSetForeground(dp, copygc, 0);
  XFillRectangle(dp, offscreen, copygc, 0, 0, IXSIZE, IYSIZE);
}
void clearrect(int x, int y, int xsize, int ysize) {
  XSetForeground(dp, copygc, 0);
  XFillRectangle(dp, offscreen, copygc, x, y, xsize, ysize);
}

void xflush() { XFlush(dp); }
void xsync() { XSync(dp, 0); }

void drawbox(int x, int y, int size, int color) {
  XSetForeground(dp, copygc, xcolors[color]);
  XDrawRectangle(dp, offscreen, copygc, x, y, size, size);
}

void drawbox2(int x, int y, int sizex, int sizey, int color) {
  XSetForeground(dp, copygc, xcolors[color]);
  XDrawRectangle(dp, offscreen, copygc, x, y, sizex, sizey);
}

void drawfillrect(int x, int y, int size, int color) {
  XSetForeground(dp, copygc, xcolors[color]);
  XFillRectangle(dp, offscreen, copygc, x, y, size, size);
}

void bigpixel(int x, int y, int color) {
  XSetForeground(dp, copygc, xcolors[color]);
  XFillRectangle(dp, offscreen, copygc, x, y, 8, 8);
}

void invert(int x, int y) {
  XSetForeground(dp, xorgc, 0xffffffff);
  XFillRectangle(dp, offscreen, xorgc, x, y, 11, 11);
}

int getmousex() { return mxpos; }
int getmousey() { return mypos; }
void drawsquare(int x, int y, uchar *source) {
  int i, j;
  for (j = 0; j < 24; j++)
    for (i = 0; i < 24; i++)
      XPutPixel(myimage, i, j, xcolors[*source++]);
  XPutImage(dp, offscreen, copygc, myimage, 0, 0, x, y, 24, 24);
}

void colormapon() {
  int alloc;
  Visual *visual;

  visual = DefaultVisual(dp, screen);
  switch (visual->class) {
  case StaticGray:
  case StaticColor:
  case TrueColor:
    alloc = AllocNone;
    break;
  default:
    alloc = AllocAll;
    break;
  }
  mycolormap = XCreateColormap(dp, wi, DefaultVisual(dp, screen), alloc);
  XSetWindowColormap(dp, wi, mycolormap);
}

void colormapoff() {
  XSetWindowColormap(dp, wi, cmap);
  XFreeColormap(dp, mycolormap);
}
int xcolors[256];

void palette(uchar *pal) {
  int i;
  XColor acolor;
  Visual *visual;
  int res;

  visual = DefaultVisual(dp, screen);
  if ((visual->class == StaticGray) || (visual->class == StaticColor) ||
      (visual->class == TrueColor))
    for (i = 0; i < 256; i++) {
      acolor.red = *pal++ << 8;
      acolor.green = *pal++ << 8;
      acolor.blue = *pal++ << 8;
      res = XAllocColor(dp, mycolormap, &acolor);
      xcolors[i] = acolor.pixel;
    }
  else
    for (i = 0; i < 256; i++) {
      acolor.red = *pal++ << 8;
      acolor.green = *pal++ << 8;
      acolor.blue = *pal++ << 8;
      acolor.pixel = i;
      acolor.flags = DoRed | DoGreen | DoBlue;
      xcolors[i] = i;
      XStoreColor(dp, mycolormap, &acolor);
    }
}
