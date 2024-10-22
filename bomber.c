/**
 *  @file   bomber.c
 *  @brief  Bomberman
 *  @author David Ashley (dash@xdr.com)
 *  @date   1999-02-07
 *  @note   GPL licensed
 *
 ***********************************************/

#include <arpa/inet.h>
#include <bomber.h>
#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <x.h>

#define FRACTION 10
#define MAXMSG 4096
#define PORT 5521
#define SPEEDDELTA (3 << FRACTION - 1)
#define SPEEDMAX (30 << FRACTION)

#define TEMPNODES 2

/* Animation specific #defines */
#define NUMBOMBFRAMES 10
#define NUMWALKFRAMES 60
#define NUMFLAMEFRAMES 80
#define NUMDEATHFRAMES 42
#define NUMCHARACTERS 50

#define HOST 1
#define CLIENT 2

unsigned char mesg[MAXMSG] = "";
uchar needwhole = 0;
int gameframe;
int netframe;
int myslot;
int network;
char HOSThostname[256];

char *remapstring = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.:!?\177/\\*-,>< =";

char *mname;

gfxset gfxsets[NUMGFX];
char walkingname[256];
char colorsetname[256];
char backgroundname[256];
char blocksname[256];
char blocksxname[256];
char bombs1name[256];
char bombs2name[256];
char flamesname[256];
char tilesname[256];
char deathname[256];
char fontname[256];
char bigfontname[256];

#define REGISTERLEN (1 + 4 + 4 + 4 + 16 + 1)

char asciiremap[256];

figure walking[MAXSETS][NUMWALKFRAMES];
figure background, backgroundoriginal;
figure blocks[3];
figure blocksx[9];
figure bombs1[MAXSETS][NUMBOMBFRAMES];
figure bombs2[MAXSETS][NUMBOMBFRAMES];
figure flamefigs[MAXSETS][NUMFLAMEFRAMES];
figure tiles[15];
figure death[NUMDEATHFRAMES];
figure font[NUMCHARACTERS];
figure bigfont[NUMCHARACTERS];

int fontxsize, fontysize;
int textx, texty;
int bigfontxsize, bigfontysize, bigfontyspace;

/* On screen array variables */
int arraynumx = 15;
int arraynumy = 11;
int arraystartx = 20;
int arraystarty = 70;
int arrayspacex = 40;
int arrayspacey = 36;

/* The playfield array, contains FIELD_* equates */
unsigned char field[32][32];
void *info[32][32];

void *things = 0;
int thingsize, thingnum;

list activebombs;
list activedecays;
list activebonus;
list activegeneric;

bomb *detonated[MAXBOMBSDETONATED];
int detonateput = 0;
int detonatetake = 0;

list activeflames;
list activeplayers;

damage damages[MAXDAMAGES];
int damageused = 0;

int dopcx(char *name, gfxset *gs);

sprite sprites[MAXSPRITES];
int spritesused = 0;

int hc = 0;
int gamemode;
char exitflag = 0;
int framecount = 0;

unsigned char regpacket[64];
char gameversion[4] = {0xda, 0x01, 0x00, 0x01};
char *playername;

struct itimerval itval;
char havepulse = 0;

struct netnode {
  struct sockaddr_in netname;
  char name[16];
  char used;
  unsigned char unique[4];
} netnodes[64];

#define MAXNETNODES 6

int numnetnodes = 0;
int myaction;
unsigned char actions[MAXNETNODES];

int udpsocket = -1, myport;
struct sockaddr_in myname = {0}, HOSTname = {0};
int senderlength;
struct sockaddr_in sender = {0};
struct sockaddr_in matchername = {0};
unsigned char matcheropened = 0;

long longtime(void) {
  struct timeb tb;
  ftime(&tb);
  return tb.time;
}

long gtime() {
  struct timeval tv;

  gettimeofday(&tv, 0);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

#define TAP1 250
#define TAP2 103
/*
#define TAP1 55
#define TAP2 31
*/

unsigned char myrandblock[TAP1];
int myrandtake;

/*

 Startings Christiaan Boersma

*/

int numplayers;
int scores[MAXNETNODES];

int singleHOST = 0;

void reverse(char s[]);
void itoa(int n, char s[]);
void initscores();
void drawscore();
void update();
int arraytoscreenx(int x);
int arraytoscreeny(int y);
int tovideox(int x);
int tovideoy(int y);
int dopcxreal(char *name, gfxset *gs);
int centerychange(player *pl);
int centerxchange(player *pl);
int scaninvite(int size);
int iterate();
extern void soundinit(const char *dev);
extern void playsound(int sound);
extern void endsound();

void reverse(char s[]) {
  int c, i, j;
  for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}

void itoa(int n, char s[]) {
  int i;
  i = 0;
  do {
    s[i++] = n % 10 + '0';
  } while ((n /= 10) > 0);
  s[i] = '\0';
  reverse(s);
}

void initscores() { bzero(scores, MAXNETNODES); }

void drawscore() {
  player *pl;
  int idx = 0, i, deltaX = 83;
  char score[3], name[4];

  pl = (player *)activeplayers.next;
  while (pl) {

    if (network)
      strncpy(name, netnodes[pl->controller].name, 3);
    else
      strncpy(name, playername, 3);
    name[3] = '\0';

    drawstring(35 + (idx * deltaX), 20, name);
    itoa(scores[pl->controller], score);
    drawstring(70 + (idx * deltaX), 20, "   ");
    drawstring(70 + (idx * deltaX), 20, score);

    ++idx;
    pl = pl->next;
  }
}

void getsocket() {
  int status;
  udpsocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (udpsocket == -1) {
    perror("socket()");
    exit(1);
  }
  bzero(&myname, sizeof(myname));
  myname.sin_family = AF_INET;
  myname.sin_addr.s_addr = htonl(INADDR_ANY);
  myname.sin_port = htons(0);
  status = bind(udpsocket, (struct sockaddr *)&myname, sizeof(myname));
  if (status == -1) {
    perror("bind()");
    exit(1);
  }
}

int myrand1() {
  int i;
  int val;

  i = myrandtake - TAP2;
  if (i < 0)
    i += TAP1;
  val = myrandblock[myrandtake++] ^= myrandblock[i];
  if (myrandtake == TAP1)
    myrandtake = 0;
  return val;
}

int myrand() {
  int v;
  v = myrand1();
  return (v << 8) | myrand1();
}

void initmyrand() {
  int i, j;
  char *p;
  int msb, msk;

  myrandtake = 0;
  p = myrandblock;
  j = 12345;
  i = TAP1;
  while (i--) {
    j = (j * 1277) & 0xffff;
    *p++ = j >> 8;
  }
  p = myrandblock + 14;
  msk = 0xff;
  msb = 0x80;
  do {
    *p &= msk;
    *p |= msb;
    p += 11;
    msk >>= 1;
    msb >>= 1;
  } while (msk);
  i = 500;
  while (i--)
    myrand();
}

int putmsg(struct sockaddr_in *toname, unsigned char *msg, int len) {
  int status;
  status = sendto(udpsocket, msg, len, 0, (struct sockaddr *)toname,
                  sizeof(struct sockaddr_in));
  return status;
}

int getmsg(int msec) {
  int size;

  bzero(&sender, sizeof(sender));
  senderlength = sizeof(sender);
  if (msec) {
    struct timeval timeout;
    fd_set readfds;
    int res;

    bzero(&timeout, sizeof(timeout));
    timeout.tv_sec = msec / 1000;
    timeout.tv_usec = (msec % 1000) * 1000;
    FD_ZERO(&readfds);
    FD_SET(udpsocket, &readfds);
    res = select(udpsocket + 1, &readfds, 0, 0, &timeout);
    if (res <= 0)
      return -1;
  }
  size = recvfrom(udpsocket, mesg, MAXMSG, 0, (struct sockaddr *)&sender,
                  &senderlength);
  return size;
}
int isvalidmsg() {
  int i;
  void *host;
  void *port;

  if (memcmp(mesg + 1, regpacket + 1, 4))
    return -1;
  host = &sender.sin_addr.s_addr;
  port = &sender.sin_port;
  for (i = 1; i < MAXNETNODES; ++i)
    if (netnodes[i].used &&
        !memcmp(&netnodes[i].netname.sin_addr.s_addr, host, 4) &&
        !memcmp(&netnodes[i].netname.sin_port, port, 2))
      return i;
  return -1;
}

long longind(unsigned char *p) {
  return (p[0] << 24L) | (p[1] << 16L) | (p[2] << 8) | p[3];
}
short shortind(unsigned char *p) { return (p[0] << 8L) | p[1]; }

void sendactions(int which, int frame) {
  char msg[64];

  *msg = PKT_STEP;
  memmove(msg + 1, regpacket + 1, 4);
  msg[5] = frame >> 24L;
  msg[6] = frame >> 16L;
  msg[7] = frame >> 8L;
  msg[8] = frame;
  memmove(msg + 9, actions, MAXNETNODES);
  putmsg(&netnodes[which].netname, msg, MAXNETNODES + 9);
}

void sendmine(int frame) {
  char msg[64];
  *msg = PKT_MYDATA;
  memmove(msg + 1, regpacket + 1, 4);
  msg[5] = frame >> 24L;
  msg[6] = frame >> 16L;
  msg[7] = frame >> 8L;
  msg[8] = frame;
  msg[9] = myaction;
  putmsg(&HOSTname, msg, 10);
}
int informsize;
int buildinform(unsigned char type) {
  unsigned char *put;
  int i;
  int size;

  put = mesg;
  *put++ = type;
  memmove(put, regpacket + 1, 4);
  put += 4;
  ++put; // slot specific for each CLIENT
  if (type == PKT_OPTIONS) {
    memcpy(put, singleoptions, 10);
    put += 10;
  } else {
    for (i = 0; i < MAXNETNODES; ++i) {
      if (!netnodes[i].used)
        continue;
      *put++ = i;
      memmove(put, netnodes[i].name, 16);
      put += 16;
    }
  }
  *put++ = 0xff;
  informsize = put - mesg;
}
void inform1(int which) {
  mesg[5] = which;
  putmsg(&netnodes[which].netname, mesg, informsize);
}
void inform(unsigned char type) {
  int i;
  buildinform(type);
  for (i = 1; i < MAXNETNODES; ++i)
    if (netnodes[i].used)
      inform1(i);
}

void networktraffic() {
  int i, j, k;
  int length;
  int frame;
  int whosent;
  unsigned char newactions[MAXNETNODES];
  int actionsneeded;
  long now;

  if (!network)
    return;
  if (network == HOST) {
    int totalknown;
    actionsneeded = 0;
    for (i = 1; i < MAXNETNODES; ++i) {
      newactions[i] = ACT_INVALID;
      if (netnodes[i].used)
        ++actionsneeded;
    }
    totalknown = 0;
    newactions[0] = myaction;
    now = longtime();
    while (totalknown < actionsneeded) {
      if (longtime() - now >= 2 && !netframe) {
        now = longtime();
        for (i = 1; i < MAXNETNODES; ++i)
          if (netnodes[i].used && newactions[i] == ACT_INVALID) {
            buildinform(PKT_BEGIN);
            inform1(i);
          }
      }
      length = getmsg(100);
      if (length > 0 && *mesg != PKT_MYDATA)
        printf("Strange packet %d\n", *mesg);
      // check for unexpected old packets...
      // for example JOIN on frame 0, respond with BEGIN if player already in
      // game respond with uninvite INVITE on JOIN from others
      if (length < 10)
        continue;
      whosent = isvalidmsg();
      if (whosent < 0)
        continue;
      frame = longind(mesg + 5);
      if (frame == netframe) {
        if (newactions[whosent] == ACT_INVALID) {
          newactions[whosent] = mesg[9];
          ++totalknown;
        }
      } else if (frame == netframe - 1) {
        sendactions(whosent, netframe - 1);
      }
    }
    if (myaction == ACT_QUIT) {
      for (i = 1; i < MAXNETNODES; ++i)
        if (netnodes[i].used)
          newactions[i] = ACT_QUIT;
    }

    memmove(actions, newactions, sizeof(actions));
    for (i = 1; i < MAXNETNODES; ++i)
      if (netnodes[i].used)
        sendactions(i, netframe);
    for (i = 1; i < MAXNETNODES; ++i)
      if (netnodes[i].used && actions[i] == ACT_QUIT)
        netnodes[i].used = 0;
  } else {
    for (;;) {
      sendmine(netframe);
      length = getmsg(100);
      if (length == MAXNETNODES + 9 &&
          sender.sin_addr.s_addr == HOSTname.sin_addr.s_addr &&
          sender.sin_port == HOSTname.sin_port &&
          longind(mesg + 5) == netframe) {
        memmove(actions, mesg + 9, MAXNETNODES);
        break;
      }
    }
  }
  ++netframe;
}

int startmatcher() {

  int fd;
  char pid_file[128], cmd[128];
  pid_t pid;
  strncpy(pid_file, "/tmp/", 128);
  strncat(pid_file, MATCHER_COM, 128);
  strncat(pid_file, ".pid", 128);
  fd = open(pid_file, O_RDONLY);
  if (fd > 0) {
    if (read(fd, &pid, sizeof(pid_t))) {
      close(fd);
      if (kill(pid, 0) == 0)
        return 0;
    }
  }
  strncpy(cmd, "./", 128);
  strncat(cmd, MATCHER_COM, 128);
  strncat(cmd, " &", 128);
  if (system(cmd) < 0) {
    return -1;
  }
  return 0;
}

int openmatcher() {
  struct hostent *hostptr;
  int status;
  if (matcheropened)
    return 1;
  hostptr = gethostbyname(mname);
  if (!hostptr) {
    hostptr = gethostbyaddr(mname, strlen(mname), AF_INET);
    if (!hostptr)
      return 0;
  }
  bzero(&matchername, sizeof(matchername));
  matchername.sin_family = AF_INET;
  matchername.sin_port = htons(PORT);
  memcpy(&matchername.sin_addr, hostptr->h_addr, hostptr->h_length);
  matcheropened = 1;
  return 1;
}

void thandler(int val) {
  signal(SIGALRM, thandler);
  hc++;
}

void freegfxset(gfxset *gs) {
  if (gs->gs_pic)
    free(gs->gs_pic);
  gs->gs_pic = 0;
}

void nomem(char *str) {
  printf("No memory!!![%s]\n", str);
  exit(1);
}

void getgroup(char *name, gfxset *colorgs, figure *fig, int count) {
  int err;
  int i;
  gfxset gs;

  err = dopcx(name, &gs);
  if (err)
    exit(1000 + err);
  createinout(&gs);
  for (i = 0; i < MAXSETS; ++i, fig += count, ++colorgs) {
    if (!colorgs->gs_pic)
      continue;
    memmove(gs.gs_colormap, colorgs->gs_colormap, sizeof(gs.gs_colormap));
    createinout(&gs);
    gfxfetch(&gs, fig, count);
  }
  freegfxset(&gs);
}

void getsingle(char *name, figure *fig, int count) {
  gfxset gs;
  int err;
  err = dopcx(name, &gs);
  if (err)
    exit(1000 + err);
  createinout(&gs);
  gfxfetch(&gs, fig, count);
  freegfxset(&gs);
}
void loadfonts(void) {
  int i, j;
  char *p;

  getsingle(fontname, font, NUMCHARACTERS);
  getsingle(bigfontname, bigfont, NUMCHARACTERS);
  fontxsize = 8;
  fontysize = 12;
  bigfontxsize = 16;
  bigfontysize = 24;
  bigfontyspace = 32;
  p = remapstring;
  j = 0;
  while (*p && *p != ' ')
    ++p, ++j;
  memset(asciiremap, j, sizeof(asciiremap));
  p = remapstring;
  i = 0;
  while (*p && i < 40)
    asciiremap[*p++] = i++;
}

void loadgfx() {
  gfxset *gs;
  gfxset *colorgs;
  int err;
  int i, j, k;
  char name[256], *p;

  strcpy(walkingname, "walk");
  strcpy(colorsetname, "pal");
  strcpy(backgroundname, "field0");
  strcpy(blocksname, "blocks3");
  strcpy(blocksxname, "blocks3x");
  strcpy(bombs1name, "bomb1");
  strcpy(bombs2name, "bomb2");
  strcpy(flamesname, "flames");
  strcpy(tilesname, "tiles");
  strcpy(deathname, "death1");
  strcpy(fontname, "font");
  strcpy(bigfontname, "bigfont");

  gs = malloc((MAXSETS + 1) * sizeof(gfxset));
  if (!gs)
    nomem("loadgfx");
  colorgs = gs + 1;

  for (i = 0; i < MAXSETS; ++i) {
    sprintf(name, "%s%d", colorsetname, i);
    err = dopcx(name, colorgs + i);
    if (err)
      continue;
  }
  clear();
  loadfonts();
  texthome();
  bigscrprintf("XBOMBER ");
  bigscrprintf("Version 1.8\n\n");
  bigscrprintf("Loaded Fonts\n");
  err = dopcx(backgroundname, gs);
  if (err)
    exit(1000 + err);
  createinout(gs);
  solidfetch(gs, &background);
  solidfetch(gs, &backgroundoriginal);
  freegfxset(gs);
  bigscrprintf("Loaded Background\n");
  getsingle(blocksname, blocks, 3);
  getsingle(blocksxname, blocksx, 9);
  bigscrprintf("Loaded Blocks\n");
  getgroup(walkingname, colorgs, walking[0], NUMWALKFRAMES);
  bigscrprintf("Loaded Walking Sequence\n");
  getgroup(bombs1name, colorgs, bombs1[0], NUMBOMBFRAMES);
  getgroup(bombs2name, colorgs, bombs2[0], NUMBOMBFRAMES);
  bigscrprintf("Loaded Bombs\n");
  getgroup(flamesname, colorgs, flamefigs[0], NUMFLAMEFRAMES);
  getsingle(flamesname, flamefigs[0], NUMFLAMEFRAMES);
  bigscrprintf("Loaded FLames\n");
  getsingle(tilesname, tiles, 15);
  bigscrprintf("Loaded Bonuses\n");
  getsingle(deathname, death, NUMDEATHFRAMES);
  bigscrprintf("Loaded Death Secquence\n");
  if (mname) {
    bigscrprintf("Server Set For ");
    bigscrprintf(mname);
  }
  bigscrprintf("\n\n!!!BLOW UP YOUR MIND!!!");

  for (i = 0; i < MAXSETS; ++i)
    freegfxset(colorgs + i);
  free(gs);
  sleep(1);
}

void texthome(void) { textx = texty = 10; }
void scrprintf(char *str, ...) {
  char output[256], *p, *p2;

  vsprintf(output, str, &str + 1);
  p = output;
  for (;;) {
    p2 = p;
    while (*p2 && *p2 != '\n')
      ++p2;
    if (*p2) {
      *p2 = 0;
      drawstring(textx, texty, p);
      texty += fontysize;
      textx = 10;
      p = p2 + 1;
    } else {
      drawstring(textx, texty, p);
      textx += fontxsize * (p2 - p);
      break;
    }
  }
  update();
  xsync();
}
void bigscrprintf(char *str, ...) {
  char output[256], *p, *p2;

  vsprintf(output, str, &str + 1);
  p = output;
  for (;;) {
    p2 = p;
    while (*p2 && *p2 != '\n')
      ++p2;
    if (*p2) {
      *p2 = 0;
      drawbigstring(textx, texty, p);
      texty += bigfontysize;
      textx = 10;
      p = p2 + 1;
    } else {
      drawbigstring(textx, texty, p);
      textx += bigfontxsize * (p2 - p);
      break;
    }
  }
  update();
  xsync();
}
void centerbig(int y, char *str) {
  int w;

  w = strlen(str) * bigfontxsize;
  drawbigstring(IXSIZE - w >> 1, y, str);
}
void addsprite(int x, int y, figure *fig) {
  sprite *sp;
  if (spritesused == MAXSPRITES)
    return;
  sp = sprites + spritesused++;
  sp->flags = 0;
  sp->xpos = x;
  sp->ypos = y;
  sp->fig = fig;
}
void adddamage(int xpos, int ypos, int xsize, int ysize) {
  damage *dm;
  if (damageused == MAXDAMAGES)
    return;
  dm = damages + damageused++;
  dm->xpos = xpos;
  dm->ypos = ypos;
  dm->xsize = xsize;
  dm->ysize = ysize;
}

void plotsprites() {
  int i;
  sprite *sp;
  figure *fig;

  sp = sprites;
  for (i = 0; i < spritesused; ++i) {
    fig = sp->fig;
    drawfigure(sp->xpos, sp->ypos, fig);
    adddamage(sp->xpos + fig->xdelta, sp->ypos + fig->ydelta, fig->xsize,
              fig->ysize);
    ++sp;
  }
}
void erasesprites() {
  int i;
  sprite *sp;
  figure *fig;

  sp = sprites;
  for (i = 0; i < spritesused; ++i) {
    fig = sp->fig;

    solidcopy(&background, sp->xpos + fig->xdelta, sp->ypos + fig->ydelta,
              fig->xsize, fig->ysize);
    adddamage(sp->xpos + fig->xdelta, sp->ypos + fig->ydelta, fig->xsize,
              fig->ysize);

    ++sp;
  }
}
void clearsprites() {
  int i;
  sprite *sp;
  figure *fig;

  sp = sprites;
  for (i = 0; i < spritesused; ++i) {
    fig = sp->fig;

    clearrect(sp->xpos + fig->xdelta, sp->ypos + fig->ydelta, fig->xsize,
              fig->ysize);
    adddamage(sp->xpos + fig->xdelta, sp->ypos + fig->ydelta, fig->xsize,
              fig->ysize);
    ++sp;
  }
}
void clearspritelist() { spritesused = 0; }

void update() {
  int i;
  damage *dm;
  dm = damages;
  for (i = 0; i < damageused; ++i) {
    copyupxysize(dm->xpos, dm->ypos, dm->xsize, dm->ysize);
    ++dm;
  }
  damageused = 0;
}

void addflame(player *owner, int px, int py) {
  flame *fl, *fl2;

  fl = allocentry();
  if (!fl)
    return;
  addtail(&activeflames, fl);
  field[py][px] = FIELD_FLAME;
  info[py][px] = fl;
  fl->px = px;
  fl->py = py;
  fl->xpos = arraytoscreenx(px);
  fl->ypos = arraytoscreeny(py);
  fl->owner = owner;
  if (px && field[py][px - 1] == FIELD_FLAME) {
    fl2 = info[py][px - 1];
    fl->lurd |= FL_LEFT;
    fl2->lurd |= FL_RIGHT;
  }
  if (py && field[py - 1][px] == FIELD_FLAME) {
    fl2 = info[py - 1][px];
    fl->lurd |= FL_UP;
    fl2->lurd |= FL_DOWN;
  }
  if (px < arraynumx - 1 && field[py][px + 1] == FIELD_FLAME) {
    fl2 = info[py][px + 1];
    fl->lurd |= FL_RIGHT;
    fl2->lurd |= FL_LEFT;
  }
  if (py < arraynumy - 1 && field[py + 1][px] == FIELD_FLAME) {
    fl2 = info[py + 1][px];
    fl->lurd |= FL_DOWN;
    fl2->lurd |= FL_UP;
  }
}

void dropbomb(player *pl, int px, int py, int type) {
  int bx, by;
  bomb *bmb;

  if (field[py][px] != FIELD_EMPTY)
    return;
  bmb = allocentry();
  if (!bmb)
    return;
  playsound(3);
  addtail(&activebombs, bmb);

  --(pl->bombsavailable);
  field[py][px] = FIELD_BOMB;
  info[py][px] = bmb;
  bmb->type = type;
  bmb->px = px;
  bmb->py = py;
  bmb->xpos = arraytoscreenx(px);
  bmb->ypos = arraytoscreeny(py);
  bmb->power = pl->flamelength;
  bmb->owner = pl;
}

void processbombs() {
  int i;
  bomb *bmb, *next;
  bmb = activebombs.next;
  while (bmb) {
    switch (bmb->type) {
    case BOMB_NORMAL:
      ++bmb->timer;
      if (bmb->timer == BOMBLIFE)
        adddetonate(bmb);
      ++(bmb->figcount);
      break;
    case BOMB_CONTROLLED:
      ++(bmb->figcount);
      break;
    }
    bmb = bmb->next;
  }
}
void adddetonate(bomb *bmb) {
  if (bmb->type == BOMB_OFF)
    return;
  bmb->type = BOMB_OFF;
  field[bmb->py][bmb->px] = FIELD_EXPLODING;
  info[bmb->py][bmb->px] = 0;
  detonated[detonateput++] = bmb;
  detonateput %= MAXBOMBSDETONATED;
}
void flameshaft(player *owner, int px, int py, int dx, int dy, int power) {
  int there;
  bomb *bmb;
  while (power--) {
    px += dx;
    py += dy;
    if (px < 0 || py < 0 || px >= arraynumx || py >= arraynumy)
      break;
    there = field[py][px];
    switch (there) {
    case FIELD_BOMB:
      bmb = info[py][px];
      adddetonate(bmb);
      break;
    case FIELD_EMPTY:
      addflame(owner, px, py);
      break;
    case FIELD_BRICK:
      adddecay(px, py);
      power = 0;
      break;
    case FIELD_BONUS:
      deletebonus(info[py][px]);
      power = 0;
      break;
    case FIELD_BORDER:
    case FIELD_EXPLODING:
    default:
      power = 0;
    case FIELD_FLAME:
      break;
    }
  }
}
void detonatebomb(bomb *bmb) {
  int px, py;
  int power;
  int i, j;
  player *owner;

  ++(bmb->owner->bombsavailable);
  px = bmb->px;
  py = bmb->py;
  power = bmb->power;
  owner = bmb->owner;
  delink(&activebombs, bmb);
  addflame(owner, px, py);
  flameshaft(owner, px, py, -1, 0, power);
  flameshaft(owner, px, py, 0, -1, power);
  flameshaft(owner, px, py, 1, 0, power);
  flameshaft(owner, px, py, 0, 1, power);
}
void dodetonations() {
  int i = 0;
  while (detonatetake != detonateput) {
    ++i;
    detonatebomb(detonated[detonatetake]);
    detonatetake = (detonatetake + 1) % MAXBOMBSDETONATED;
  }
  if (i)
    playsound((myrand() & 1) ? 0 : 4);
}

void drawbombs() {
  int i, j;
  bomb *bmb;
  struct figure *figtab;
  int color;
  int xpos, ypos;

  bmb = activebombs.next;
  while (bmb) {
    color = bmb->owner->color;
    figtab = (bmb->type == BOMB_NORMAL) ? bombs1[color] : bombs2[color];
    j = bmb->figcount % (NUMBOMBFRAMES << 1);
    if (j >= NUMBOMBFRAMES)
      j = (NUMBOMBFRAMES << 1) - j - 1;
    xpos = tovideox(bmb->xpos);
    ypos = tovideoy(bmb->ypos) - 3;

    addsprite(xpos, ypos, figtab + j);
    bmb = bmb->next;
  }
}
void processflames() {
  flame *fl, *fl2, *flend;
  int i;
  fl = activeflames.next;
  while (fl) {
    ++(fl->timer);
    fl = fl->next;
  }
  fl = activeflames.next;
  while (fl) {
    if (fl->timer == FLAMELIFE) {
      field[fl->py][fl->px] = FIELD_EMPTY;
      info[fl->py][fl->px] = 0;
      fl2 = fl;
      fl = fl->next;
      delink(&activeflames, fl2);
    } else
      fl = fl->next;
  }
}
void drawflames() {
  int i;
  flame *fl;
  int xpos, ypos;
  int color;
  int fig;
  int lurd;

  fl = activeflames.next;
  while (fl) {
    color = fl->owner->color;
    xpos = tovideox(fl->xpos);
    ypos = tovideoy(fl->ypos);
    fig = (fl->timer * 10) / FLAMELIFE;
    if (fig >= 5)
      fig = 9 - fig;
    fig += 5 * fl->lurd;
    addsprite(xpos, ypos, flamefigs[5 /* color */] + fig);
    fl = fl->next;
  }
}
void adddecay(int px, int py) {
  brickdecay *bd;
  int xpos, ypos;
  bd = allocentry();
  if (!bd)
    return;
  field[py][px] = FIELD_EXPLODING;
  bd->xpos = arraytoscreenx(px);
  bd->ypos = arraytoscreeny(py);
  bd->px = px;
  bd->py = py;
  addtail(&activedecays, bd);
  xpos = tovideox(bd->xpos);
  ypos = tovideoy(bd->ypos);
  solidcopyany(&backgroundoriginal, &background, xpos, ypos, arrayspacex,
               arrayspacey);
  solidcopy(&background, xpos, ypos, arrayspacex, arrayspacey);
}
void processdecays() {
  brickdecay *bd, *bd2;
  int xpos, ypos;

  bd = activedecays.next;
  while (bd) {
    ++(bd->timer);
    if (bd->timer == DECAYLIFE) {
      field[bd->py][bd->px] = FIELD_EMPTY;
      trybonus(bd->px, bd->py);
      bd2 = bd;
      bd = bd->next;
      delink(&activedecays, bd2);
    } else
      bd = bd->next;
  }
}

void drawdecays() {
  brickdecay *bd;

  bd = activedecays.next;
  while (bd) {
    addsprite(tovideox(bd->xpos), tovideoy(bd->ypos),
              blocksx + (bd->timer * 9) / DECAYLIFE);
    bd = bd->next;
  }
}
int bonustotal;
int bonuschances[] = {TILE_BOMB,      10, TILE_FLAME,  15, TILE_CONTROL, 20,
                      TILE_GOLDFLAME, 10, TILE_SKATES, 20, TILE_TURTLE,  8,
                      TILE_POISON,    8,  TILE_RANDOM, 15, TILE_DEAD,    6,
                      TILE_PI,        10, TILE_NONE,   40};

void trybonus(int px, int py) {
  int i, *p, r;

  if (field[py][px] != FIELD_EMPTY)
    return;
  p = bonuschances;
  r = myrand() % bonustotal;
  while (r >= 0) {
    i = *p++;
    r -= *p++;
  }
  if (i == TILE_NONE)
    return;
  addbonus(px, py, i);
}
void deletebonus(bonustile *bonus) {
  int px, py;
  px = bonus->px;
  py = bonus->py;
  field[py][px] = 0;
  info[py][px] = 0;
  delink(&activebonus, bonus);
}
void addbonus(int px, int py, int type) {
  bonustile *bonus;
  bonus = allocentry();
  if (!bonus)
    return;
  addtail(&activebonus, bonus);
  bonus->px = px;
  bonus->py = py;
  bonus->xpos = arraytoscreenx(px);
  bonus->ypos = arraytoscreeny(py);
  bonus->type = type;
  field[py][px] = FIELD_BONUS;
  info[py][px] = bonus;
}
void drawbonus() {
  bonustile *bonus;
  bonus = activebonus.next;
  while (bonus) {
    addsprite(tovideox(bonus->xpos), tovideoy(bonus->ypos),
              tiles + bonus->type);
    bonus = bonus->next;
  }
}

void drawplayers() {
  int i;
  player *pl;
  int xpos, ypos;
  pl = activeplayers.next;

  while (pl) {
    if (!(pl->flags & FLG_DEAD)) {
      if (!pl->figure)
        pl->figure = walking[pl->color] + 30;
      xpos = tovideox(pl->xpos) + pl->fixx;
      ypos = tovideoy(pl->ypos) + pl->fixy;
      addsprite(xpos, ypos, pl->figure);
    }
    pl = pl->next;
  }
}
void detonatecontrolled(player *pl) {
  bomb *bmb;
  bmb = activebombs.next;
  while (bmb) {
    if (bmb->owner == pl && bmb->type == BOMB_CONTROLLED)
      adddetonate(bmb);
    bmb = bmb->next;
  }
}
void killplayer(player *pl) {
  pl->flags |= FLG_DEAD;

  numplayers--;

  playsound(2);
  adddeath(pl);
  detonatecontrolled(pl);
}
void adddeath(player *pl) {
  int xpos, ypos;
  xpos = tovideox(pl->xpos) + pl->fixx - 10;
  ypos = tovideoy(pl->ypos) + pl->fixy;
  queuesequence(xpos, ypos, death, NUMDEATHFRAMES);
}
void drawstring(int xpos, int ypos, char *str) {
  char ch;

  adddamage(xpos, ypos, fontxsize * strlen(str), fontysize);
  while (ch = *str++) {
    drawfigure(xpos, ypos, font + asciiremap[toupper(ch)]);
    xpos += fontxsize;
  }
}
void drawbigstring(int xpos, int ypos, char *str) {
  char ch;

  adddamage(xpos, ypos, bigfontxsize * strlen(str), bigfontysize);
  while (ch = *str++) {
    drawfigure(xpos, ypos, bigfont + asciiremap[toupper(ch)]);
    xpos += bigfontxsize;
  }
}

int dopcx(char *name, gfxset *gs) {
  int err;
  err = dopcxreal(name, gs);
  if (err)
    printf("Error loading \"%s\":code %d\n", name, err);
  return err;
}

#define IBUFFLEN 1024
int ileft = 0, ihand = 0, byteswide;
unsigned char ibuff[IBUFFLEN], *itake;

int myci() {
  if (!ileft) {
    ileft = read(ihand, ibuff, IBUFFLEN);

    if (!ileft)
      return -1;
    itake = ibuff;
  }
  ileft--;
  return *itake++;
}

int dopcxreal(char *name, gfxset *gs) {
  int xs, ys;
  int i, j, k;
  int totalsize;
  int width, height;
  unsigned char *bm, *lp;
  char tname[256];

  bzero(gs, sizeof(gfxset));
  ileft = 0;
  sprintf(tname, "data/%s", name);
  ihand = open(tname, O_RDONLY);
  if (ihand < 0) {
    char tname2[256];
    sprintf(tname2, "%s.pcx", tname);
    ihand = open(tname2, O_RDONLY);
    if (ihand < 0)
      return 1;
  }
  if (myci() != 10) {
    close(ihand);
    return 2;
  } // 10=zsoft .pcx
  if (myci() != 5) {
    close(ihand);
    return 3;
  } // version 3.0
  if (myci() != 1) {
    close(ihand);
    return 4;
  } // encoding method
  if (myci() != 8) {
    close(ihand);
    return 5;
  } // bpp
  xs = myci();
  xs |= myci() << 8;
  ys = myci();
  ys |= myci() << 8;
  width = myci();
  width |= myci() << 8;
  height = myci();
  height |= myci() << 8;
  width = width + 1 - xs;
  height = height + 1 - ys;
  for (i = 0; i < 48 + 4; ++i)
    myci();
  myci();
  if (myci() != 1) {
    close(ihand);
    return 6;
  } // # of planes
  byteswide = myci();
  byteswide |= myci() << 8;
  i = myci();
  i |= myci() << 8;
  //	if(i!=1) {close(ihand);return 7;} // 1=color/bw,2=grey
  for (i = 0; i < 58; ++i)
    myci();
  totalsize = height * byteswide;
  bm = malloc(totalsize + 1);
  if (!bm) {
    close(ihand);
    return 8;
  } // no memory
  gs->gs_pic = bm;
  gs->gs_xsize = width;
  gs->gs_ysize = height;
  while (height--) {
    lp = bm;
    i = byteswide;
    while (i > 0) {
      j = myci();
      if (j < 0xc0) {
        *lp++ = j;
        --i;
      } else {
        j &= 0x3f;
        k = myci();
        while (j-- && i) {
          *lp++ = k;
          --i;
        }
      }
    }
    bm += width;
  }
  lseek(ihand, -0x300, SEEK_END);
  read(ihand, gs->gs_colormap, 0x300);
  close(ihand);
  return 0;
}

void queuesequence(int xpos, int ypos, figure *fig, int count) {
  generic *gen;
  gen = allocentry();
  if (!gen)
    return;
  gen->xpos = xpos;
  gen->ypos = ypos;
  gen->data1 = count;
  gen->process = playonce;
  gen->draw = drawgeneric;
  gen->ptr1 = fig;
  addtail(&activegeneric, gen);
}
void playonce(generic *gen) {
  if (gen->timer == gen->data1)
    delink(&activegeneric, gen);
}
void drawgeneric(generic *gen) {
  addsprite(gen->xpos, gen->ypos, ((figure *)(gen->ptr1)) + gen->timer);
}

void processgenerics() {
  generic *gen, *gen2;
  gen = activegeneric.next;
  while (gen) {
    gen2 = gen;
    gen = gen->next;
    ++(gen2->timer);
    gen2->process(gen2);
  }
}
void drawgenerics() {
  generic *gen;
  gen = activegeneric.next;
  while (gen) {
    gen->draw(gen);
    gen = gen->next;
  }
}

#define SGN(x) ((x) == 0 ? 0 : ((x) < 0 ? -1 : 1))

int screentoarrayx(int x) {
  x += arrayspacex << FRACTION + 2;
  return ((x >> FRACTION) + (arrayspacex >> 1)) / arrayspacex - 4;
}
int screentoarrayy(int y) {
  y += arrayspacey << FRACTION + 2;
  return ((y >> FRACTION) + (arrayspacey >> 1)) / arrayspacey - 4;
}
int arraytoscreenx(int x) { return arrayspacex * x << FRACTION; }
int arraytoscreeny(int y) { return arrayspacey * y << FRACTION; }
int tovideox(int x) { return (x >> FRACTION) + arraystartx; }
int tovideoy(int y) { return (y >> FRACTION) + arraystarty; }

void trymove(player *pl, int dx, int dy) {
  int wx, wy;
  int sx, sy;
  int there;
  int px, py;
  static int depth = 0;
  int tx, ty;

  ++depth;
  sx = dx * (arrayspacex + 1) << FRACTION - 1;
  sy = dy * (arrayspacey + 1) << FRACTION - 1;

  wx = screentoarrayx(pl->xpos + sx);
  wy = screentoarrayy(pl->ypos + sy);
  px = screentoarrayx(pl->xpos);
  py = screentoarrayy(pl->ypos);

  if (wx < 0 || wx >= arraynumx || wy < 0 || wy >= arraynumy) {
    --depth;
    return;
  }
  there = field[wy][wx];
  if ((px != wx || py != wy) &&
      (there == FIELD_BRICK || there == FIELD_BOMB || there == FIELD_BORDER)) {
    if (dx && !dy) {
      ty = centerychange(pl);
      if (ty && depth == 1)
        trymove(pl, 0, -SGN(ty));

    } else if (dy && !dx) {
      tx = centerxchange(pl);
      if (tx && depth == 1)
        trymove(pl, -SGN(tx), 0);
    }

  } else {
    pl->xpos += dx * pl->speed;
    pl->ypos += dy * pl->speed;
    if (dx && !dy)
      centery(pl);
    if (dy && !dx)
      centerx(pl);
  }
  --depth;
}
int centerxchange(player *pl) {
  int speed;
  int val;
  int line;
  int max;

  max = arrayspacex << FRACTION;
  speed = pl->speed;
  val = pl->xpos + (max << 2);
  val %= max;
  line = max >> 1;
  if (val < line) {
    if (val - speed < 0)
      return -val;
    else
      return -speed;
  } else if (val >= line) {
    if (val + speed > max)
      return max - val;
    else
      return speed;
  }
  return 0;
}
void centerx(player *pl) { pl->xpos += centerxchange(pl); }
int centerychange(player *pl) {
  int speed;
  int val;
  int line;
  int max;

  max = arrayspacey << FRACTION;
  speed = pl->speed;
  val = pl->ypos + (max << 2);
  val %= max;
  line = max >> 1;
  if (val < line) {
    if (val - speed < 0)
      return -val;
    else
      return -speed;
  } else if (val >= line) {
    if (val + speed > max)
      return max - val;
    else
      return speed;
  }
  return 0;
}
void centery(player *pl) { pl->ypos += centerychange(pl); }

void applybonus(player *pl, bonustile *bonus) {
  int type;
  int maxflame;

  maxflame = arraynumx > arraynumy ? arraynumx : arraynumy;
  type = bonus->type;
  deletebonus(bonus);

  if (type == TILE_RANDOM) {
    switch (rand() % 19) {
    case 0:
    case 1:
      type = TILE_BOMB;
      break;
    case 2:
    case 3:
    case 4:
      type = TILE_FLAME;
      break;
    case 5:
    case 6:
      type = TILE_GOLDFLAME;
      break;
    case 7:
    case 8:
      type = TILE_CONTROL;
      break;
    case 9:
    case 10:
    case 11:
      type = TILE_SKATES;
      break;
    case 12:
    case 13:
      type = TILE_TURTLE;
      break;
    case 14:
    case 15:
      type = TILE_POISON;
      break;
    case 16:
      type = TILE_DEAD;
      break;
    case 17:
    case 18:
      type = TILE_PI;
    }
  }

  switch (type) {
  case TILE_BOMB:
    ++(pl->bombsavailable);
    break;
  case TILE_FLAME:
    if (pl->flamelength < maxflame)
      ++(pl->flamelength);
    break;
  case TILE_GOLDFLAME:
    pl->flamelength = maxflame;
    break;
  case TILE_CONTROL:
    if (pl->flags == FLG_CONTROL)
      ++(pl->bombsavailable);
    pl->flags |= FLG_CONTROL;
    break;
  case TILE_SKATES:
    pl->speed += SPEEDDELTA;
    if (pl->speed > SPEEDMAX)
      pl->speed = SPEEDMAX;
    break;
  case TILE_TURTLE:
    pl->turtle = TURTLE;
    pl->speed0 = pl->speed;
    pl->speed = 1 << FRACTION;
    break;
  case TILE_POISON:
    pl->poison = POISONED;
    break;
  case TILE_DEAD:
    killplayer(pl);
    break;
  case TILE_PI:
    scores[pl->controller] = 0;
    drawscore();
    break;
  }
}

void doplayer(player *pl) {
  int last;
  int color;
  int speed;
  int xpos, ypos;
  int px, py;
  int there;
  int flags;
  int what;

  if (pl->controller == -1)
    what = myaction;
  else
    what = actions[pl->controller];

  flags = pl->flags;
  if (flags & FLG_DEAD)
    return;
  color = pl->color;
  last = pl->doing;
  pl->doing = what;
  speed = pl->speed;
  px = screentoarrayx(pl->xpos);
  py = screentoarrayy(pl->ypos);
  there = field[py][px];

  if (what == ACT_QUIT) {
    killplayer(pl);
    return;
  }

  if (there == FIELD_BONUS) {
    playsound((myrand() & 1) ? 1 : 5);
    applybonus(pl, info[py][px]);
  } else if (there == FIELD_FLAME) {
    killplayer(pl);
    return;
  }

  //	if(what&ACT_TURBO) speed<<=2;
  if (what & ACT_PRIMARY) {
    if (there == FIELD_EMPTY && pl->bombsavailable)
      dropbomb(pl, px, py,
               (flags & FLG_CONTROL) ? BOMB_CONTROLLED : BOMB_NORMAL);
  }
  if (what & ACT_SECONDARY && (flags & FLG_CONTROL))
    detonatecontrolled(pl);

  if (pl->poison & POISONED) {
    ++pl->timer;
    if (pl->timer == POISONLIFE) {
      pl->poison = NOPOISON;
      pl->timer = 0;
    }
    switch (what & ACT_MASK) {
    case ACT_DOWN:
      trymove(pl, 0, -1);
      pl->figcount = (pl->figcount + 1) % 15;
      pl->figure = walking[color] + pl->figcount + 15;
      break;
    case ACT_UP:
      trymove(pl, 0, 1);
      pl->figcount = (pl->figcount + 1) % 15;
      pl->figure = walking[color] + pl->figcount + 30;
      break;
    case ACT_RIGHT:
      trymove(pl, -1, 0);
      pl->figcount = (pl->figcount + 1) % 15;
      pl->figure = walking[color] + pl->figcount + 45;
      break;
    case ACT_LEFT:
      trymove(pl, 1, 0);
      pl->figcount = (pl->figcount + 1) % 15;
      pl->figure = walking[color] + pl->figcount;
      break;
    case ACT_NONE:
      break;
    }
  } else {
    switch (what & ACT_MASK) {
    case ACT_UP:
      trymove(pl, 0, -1);
      pl->figcount = (pl->figcount + 1) % 15;
      pl->figure = walking[color] + pl->figcount + 15;
      break;
    case ACT_DOWN:
      trymove(pl, 0, 1);
      pl->figcount = (pl->figcount + 1) % 15;
      pl->figure = walking[color] + pl->figcount + 30;
      break;
    case ACT_LEFT:
      trymove(pl, -1, 0);
      pl->figcount = (pl->figcount + 1) % 15;
      pl->figure = walking[color] + pl->figcount + 45;
      break;
    case ACT_RIGHT:
      trymove(pl, 1, 0);
      pl->figcount = (pl->figcount + 1) % 15;
      pl->figure = walking[color] + pl->figcount;
      break;
    case ACT_NONE:
      break;
    }
  }

  if (pl->turtle == TURTLE) {
    ++pl->timer0;
    if (pl->timer0 == TURTLELIFE) {
      pl->turtle = NOTURTLE;
      pl->timer0 = 0;
      pl->speed = pl->speed0;
    }
  }
}

void initplayer(int color, int x, int y, int controller) {
  player *pl;
  pl = allocentry();
  if (!pl)
    nomem("Couldn't get player structure (allocentry())");
  addtail(&activeplayers, pl);
  bzero(pl, sizeof(player));
  pl->xpos = arraytoscreenx(x);
  pl->ypos = arraytoscreeny(y);
  pl->color = color;
  pl->speed = 6 << FRACTION;
  pl->flags = 0;
  pl->fixx = -4;
  pl->fixy = -40;
  pl->flamelength = gameoptions[GO_FLAMES] + 1;
  pl->bombsavailable = gameoptions[GO_BOMBS] + 1;
  pl->controller = controller;
  pl->timer = 0;
  pl->timer0 = 0;
  pl->poison = NOPOISON;
  pl->turtle = NOTURTLE;
  field[y][x] = FIELD_EMPTY;
  if (x)
    field[y][x - 1] = FIELD_EMPTY;
  if (y)
    field[y - 1][x] = FIELD_EMPTY;
  if (x < arraynumx - 1)
    field[y][x + 1] = FIELD_EMPTY;
  if (y < arraynumy - 1)
    field[y + 1][x] = FIELD_EMPTY;
}

unsigned char playerpositions[] = {
    2, 0, 0, 3, 14, 10, 4, 14, 0, 5, 0,  10,
    6, 6, 0, 7, 8,  10, 8, 0,  6, 1, 14, 4,
};

void initplayers() {
  int i;
  unsigned char *p;
  int c, x, y;

  if (!network) {
    initplayer(2, 0, 0, -1);
    return;
  }
  p = playerpositions;
  for (i = 0; i < MAXNETNODES; ++i) {
    if (!netnodes[i].used)
      continue;
    c = *p++;
    x = *p++;
    y = *p++;
    initplayer(c, x, y, i);
  }
}
void initlist(void *first, int size, int num) {
  unsigned char *p;
  list *usable;
  int i;

  bzero(first, size * num);
  for (i = 0; i < num - 1; ++i) {
    p = first;
    p += size;
    ((list *)first)->next = (void *)p;
    first = p;
  }
  ((list *)first)->next = 0;
}
void *allocentry() {
  list *entry;
  if (!(entry = ((list *)things)->next))
    return 0;
  ((list *)things)->next = entry->next;
  bzero(entry, thingsize);
  return entry;
}
void freeentry(void *entry) {
  ((list *)entry)->next = ((list *)things)->next;
  ((list *)things)->next = entry;
}
void addtail(void *header, void *entry) {
  while (((list *)header)->next)
    header = ((list *)header)->next;
  ((list *)header)->next = entry;
  ((list *)entry)->next = 0;
}
void delink(void *header, void *entry) {
  while (((list *)header)->next != entry)
    header = ((list *)header)->next;
  ((list *)header)->next = ((list *)entry)->next;
  ((list *)entry)->next = 0;
  freeentry(entry);
}
void allocthings() {
  if (!things) {
    thingsize = sizeof(bomb);
    if (sizeof(flame) > thingsize)
      thingsize = sizeof(flame);
    if (sizeof(brickdecay) > thingsize)
      thingsize = sizeof(brickdecay);
    if (sizeof(player) > thingsize)
      thingsize = sizeof(player);
    thingnum = MAXTHINGS;
    things = malloc(thingsize * thingnum);
    if (!things)
      nomem("Trying to allocate thing memory");
  }
  initlist(things, thingsize, thingnum);
}
void initheader(void *p) { bzero(p, sizeof(list)); }

void initgame() {
  int i, j;
  int x, y;
  int bl;
  int *p;
  int comp;

  int idx = 0, deltaX = 83;
  player *pl;
  struct figure *figtab;

  netframe = 0;
  memmove(gameoptions, singleoptions, sizeof(gameoptions));
  gameframe = 0;
  allocthings();
  initheader(&activebombs);
  initheader(&activeflames);
  initheader(&activedecays);
  initheader(&activebonus);
  initheader(&activeplayers);
  initheader(&activegeneric);

  detonateput = detonatetake = 0;

  p = bonuschances;
  bonustotal = 0;
  for (;;) {
    i = *p++;
    if (i == TILE_NONE)
      break;
    bonustotal += *p++;
  }
  bonustotal += *p = 64 * (3 - gameoptions[GO_GENEROSITY]);
  memset(field, FIELD_EMPTY, sizeof(field));
  comp = gameoptions[GO_DENSITY];
  for (j = 0; j < arraynumy; ++j) {
    for (i = 0; i < arraynumx; ++i) {
      if ((i & j) & 1) {
        field[j][i] = FIELD_BORDER;
      } else {
        field[j][i] = (myrand() & 3) >= comp ? FIELD_BRICK : FIELD_EMPTY;
      }
    }
  }
  solidcopyany(&backgroundoriginal, &background, 0, 0, IXSIZE, IYSIZE);

  initplayers();

  for (j = 0; j < arraynumy; ++j) {
    y = arraystarty + j * arrayspacey;
    for (i = 0; i < arraynumx; ++i) {
      x = arraystartx + i * arrayspacex;
      bl = field[j][i];
      if (bl == FIELD_BORDER)
        bl = 2;
      else if (bl == FIELD_BRICK)
        bl = 1;
      else
        continue;
      drawfigureany(x, y, blocks + bl, &background);
    }
  }

  pl = activeplayers.next;
  numplayers = 0;
  idx = 0;
  while (pl) {
    figtab = bombs1[(idx + 2)];
    drawfigureany(idx * deltaX, 0, figtab + 9, &background);

    numplayers++;
    idx++;

    pl = pl->next;
  }

  if (numplayers == 1 && network)
    singleHOST = 1;

  solidcopy(&background, 0, 0, IXSIZE, IYSIZE);
  copyup();
  drawscore();
}

void pulseon() {
  if (havepulse)
    return;
  havepulse = 1;
  itval.it_interval.tv_sec = itval.it_value.tv_sec = 0;
  itval.it_interval.tv_usec = itval.it_value.tv_usec = 33334;
  thandler(0);
  setitimer(ITIMER_REAL, &itval, NULL);
}

void pulseoff() {
  if (!havepulse)
    return;
  havepulse = 0;
  bzero(&itval, sizeof(itval));
  setitimer(ITIMER_REAL, &itval, NULL);
}

int menuhistory[32] = {0};
char menustring[1024];
char *menuput, *menuitems[40], *menutitle, *menuputtext, *menutext[40];
int menunum, textnum;
int menudelta;

void menustart() {
  menunum = -1;
  menuput = menustring;
  *menuput = 0;

  textnum = 0;
  menuputtext = menustring;
}

void additem(char *format, ...) {
  char output[256];
  va_list aptr;
  va_start(aptr, format);
  vsnprintf(output, 255, format, aptr);
  va_end(aptr);
  if (menunum < 0)
    menutitle = menuput;
  else
    menuitems[menunum] = menuput;
  ++menunum;
  strcpy(menuput, output);
  menuput += strlen(output) + 1;
}

void addtext(char *item, ...) {
  char output[256];

  vsprintf(output, item, &item + 1);
  menutext[textnum] = menuput;
  ++textnum;
  strcpy(menuput, output);
  menuput += strlen(output) + 1;
}

void drawmenu(int selected) {
  int i, j;
  int tx, ty;

  clear();
  ty = (IYSIZE - (bigfontysize * (menunum)) >> 1) - (IYSIZE >> 2);

  for (i = 0; i < textnum; ++i) {
    j = strlen(menutext[i]) * bigfontxsize;
    drawbigstring(20, ty, menutext[i]);
    ty += bigfontyspace;
  }
  ty += bigfontyspace;

  j = strlen(menutitle) * bigfontxsize;
  drawbigstring(IXSIZE - j >> 1, 20, menutitle);
  for (i = 0; i < menunum; ++i) {
    j = strlen(menuitems[i]) * bigfontxsize;
    tx = IXSIZE - j >> 1;
    if (i == selected) {
      greyrect(0, ty - 1, tx - 5, bigfontysize);
      greyrect(tx + j + 3, ty - 1, IXSIZE - (tx + j + 3), bigfontysize);
    }
    drawbigstring(tx, ty, menuitems[i]);
    ty += bigfontyspace;
  }
  adddamage(0, 0, IXSIZE, IYSIZE);
}

long longhash(char *str) {
  long val = 0;
  char ch;
  while (ch = *str++) {
    val = (val << 4) | ((val >> 28) & 0x0f);
    val ^= ch;
  }
}
void makereg() {
  long val;
  val = gtime();
  regpacket[0] = PKT_REGISTER;
  memmove(regpacket + 1, &val, 4);
  memset(regpacket + 5, 0, 4);
  memmove(regpacket + 9, gameversion, 4);
  memmove(regpacket + 13, playername, 16);
  regpacket[29] = 1;
}
void clearreg() { regpacket[29] = 0; }
void drawjoinscreen() {
  int i, j, k;
  char name[17];
  char temp[64];

#define JX (IXSIZE / 3)
#define JY (IYSIZE / 4)

  clear();
  centerbig(20, "JOIN NETWORK GAME");
  drawbigstring(JX, JY, "SLOT    NAME");
  for (i = 0; i < MAXNETNODES; ++i) {
    if (!netnodes[i].used)
      continue;
    memmove(name, netnodes[i].name, 16);
    name[16] = 0;
    sprintf(temp, "  %d     %s", i + 1, name);
    drawbigstring(JX, JY + (i + 1) * bigfontyspace, temp);
  }
}
int tryjoin(int which) {
  long now;
  int size;
  char amin;
  int res;

  pulseoff();
  memmove(&HOSTname, &netnodes[which].netname, sizeof(HOSTname));
  *regpacket = PKT_JOIN;
  memmove(regpacket + 1, netnodes[which].unique, 4);
  memmove(regpacket + 5, playername, 16);
  now = longtime();
  amin = 0;
  while (longtime() - now < 10 && !amin) {
    putmsg(&HOSTname, regpacket, 1 + 4 + 16);
    size = getmsg(1000);
    res = scaninvite(size);
    if (!size)
      continue;
    if (size == 1)
      return 0;
    amin = 1;
  }
  if (!amin)
    return 0;
  scaninvite(size);
  drawjoinscreen();
  copyup();
  if (*mesg == PKT_BEGIN)
    amin = 2;
  while (amin < 2) {
    scaninput();
    while (anydown())
      switch (takedown()) {
      case XK_Escape:
        *regpacket = PKT_QUIT;
        now = longtime();
        while (longtime() - now < 10) {
          putmsg(&HOSTname, regpacket, 5);
          size = getmsg(1000);
          if (size < 6)
            continue;
          if (*mesg != PKT_ACK)
            continue;
          if (!memcmp(mesg + 1, regpacket, 5))
            break;
        }
        return 0;
      }
    size = getmsg(200);
    if (res = scaninvite(size)) {
      drawjoinscreen();
      copyup();
      if (res == 3)
        return 1;
      if (res == 1)
        return 0;
    }
  }
}

// returns 0=ignore packet,1=we're rejected,2=INVITE,3=BEGIN

int scaninvite(int size) {
  int i;
  unsigned char *take;
  if (size < 7)
    return 0;
  if (*mesg != PKT_INVITE && *mesg != PKT_BEGIN && *mesg != PKT_OPTIONS)
    return 0;
  if (memcmp(mesg + 1, regpacket + 1, 4))
    return 0;
  if (mesg[6] == 0xff)
    return 1;
  if (*mesg == PKT_OPTIONS) {
    memcpy(singleoptions, mesg + 6, 10);
    return 0;
  }
  myslot = mesg[5];
  bzero(netnodes, sizeof(netnodes));
  size -= 6;
  take = mesg + 6;
  while (*take != 0xff && size >= 17) {
    if ((i = *take) < MAXNETNODES) {
      netnodes[i].used = 1;
      memmove(netnodes[i].name, take + 1, 16);
    }
    take += 17;
    size -= 17;
  }
  if (*mesg == PKT_INVITE)
    return 2;
  else
    return 3;
}

int domenu(int whichmenu, int whichanimation) {
  char redraw;
  int selected;
  int mcount = 0;
  figure *fig;

  if (whichmenu >= 0)
    selected = menuhistory[whichmenu];
  else
    selected = 0;

  redraw = 1;
  clearspritelist();
  pulseon();
  for (;;) {
    pause();
    if (redraw) {
      drawmenu(selected);
      redraw = 0;
    }
    scaninput();
    ++mcount;

    clearsprites();
    clearspritelist();

    if (whichanimation) {
      addsprite(IXSIZE / 2 - 50 - 20, IYSIZE - 80,
                walking[2] + (30 + mcount % 15));
      addsprite(IXSIZE / 2 + 50 - 20, IYSIZE - 80,
                walking[3] + (30 + (mcount + 7) % 15));
    } else {
      addsprite(IXSIZE / 2 - 50 - 20, IYSIZE - 80,
                bombs1[4] + (1 + mcount % 9));
      addsprite(IXSIZE / 2 + 50 - 20, IYSIZE - 80,
                bombs1[6] + (1 + mcount % 9));
    }
    plotsprites();
    update();

    if (anydown())
      playsound(3);
    while (anydown())
      switch (takedown()) {
      case XK_Left:
        menudelta = -1;
        return selected;
      case XK_Right:
      case XK_space:
      case XK_Return:
        menudelta = 1;
        return selected;
      case XK_k:
      case XK_Up:
        if (selected)
          --selected;
        else
          selected = menunum - 1;
        if (whichmenu >= 0)
          menuhistory[whichmenu] = selected;
        redraw = 1;
        break;
      case XK_j:
      case XK_Down:
        ++selected;
        if (selected == menunum)
          selected = 0;
        if (whichmenu >= 0)
          menuhistory[whichmenu] = selected;
        redraw = 1;
        break;
      case XK_Escape:
        if (!whichmenu && selected) {
          selected = 0;
          redraw = 1;
          break;
        }
        menudelta = 1;
        return 0;
      }
  }
}

int registergame() {
  long now;
  int size;
  if (!openmatcher())
    return 0;
  pulseoff();
  now = longtime();
  while (longtime() - now < 10) {
    putmsg(&matchername, regpacket, REGISTERLEN);
    size = getmsg(1000);
    if (size < REGISTERLEN + 1)
      continue;
    if (mesg[0] != PKT_ACK)
      continue;
    if (memcmp(regpacket, mesg + 1, REGISTERLEN))
      continue;
    return 1;
  }
  return 0;
}
int unregistergame() {
  long now;
  int size;

  if (!openmatcher())
    return 0;
  pulseoff();
  now = longtime();
  clearreg();
  while (longtime() - now < 10) {
    putmsg(&matchername, regpacket, REGISTERLEN);
    size = getmsg(1000);
    if (size < REGISTERLEN + 1)
      continue;
    if (mesg[0] != PKT_ACK)
      continue;
    if (memcmp(regpacket, mesg + 1, REGISTERLEN))
      continue;
    return 1;
  }
  return 0;
}

void failure(const char *str) {
  printf("%s\n", str);
  gamemode = 0;
  return;
}
void drawmode3() {
  int i, j, k;
  char *name;
  char temp[64];

#define M3X (IXSIZE / 3)
#define M3Y (IYSIZE / 4)

  clear();
  centerbig(20, "HOST NETWORK GAME");
  drawbigstring(M3X, M3Y, "SLOT    NAME");
  for (i = 0; i < MAXNETNODES; ++i) {
    if (!netnodes[i].used)
      continue;
    name = netnodes[i].name;
    sprintf(temp, "  %d     %s", i + 1, name);
    drawbigstring(M3X, M3Y + (i + 2) * bigfontyspace, temp);
  }
}

int getaction() {
  int what;
  what = ACT_NONE;
  if (checkpressed(XK_Left))
    what = ACT_LEFT;
  else if (checkpressed(XK_Right))
    what = ACT_RIGHT;
  else if (checkpressed(XK_Down))
    what = ACT_DOWN;
  else if (checkpressed(XK_Up))
    what = ACT_UP;
  else if (checkdown(XK_Return))
    what = ACT_ENTER;
  else if (checkdown(XK_Escape))
    what = ACT_QUIT;

  if (checkdown(XK_space))
    what |= ACT_PRIMARY;
  if (checkdown(XK_Tab))
    what |= ACT_SECONDARY;

  return what;
}
void processplayers() {
  player *pl, *pl2;
  pl = activeplayers.next;
  while (pl) {
    pl2 = pl;
    pl = pl->next;
    doplayer(pl2);
  }
}
void processquits() {
  int i;
  if (network != CLIENT)
    return;
  for (i = 0; i < MAXNETNODES; ++i) {
    if (netnodes[i].used && actions[i] == ACT_QUIT) {
      netnodes[i].used = 0;
    }
  }
}

void domode0() {
  int i;
  int sel;

  pulseon();

  initscores();

  for (;;) {
    menustart();
    additem("MAIN MENU");
    additem("EXIT GAME");
    additem("SINGLE PLAYER GAME");
    additem("GAME OPTIONS");
    additem("START NETWORK GAME");
    additem("JOIN NETWORK GAME");
    additem("CREDITS");
    sel = domenu(0, 1);
    if (!sel) {
      exitflag = 1;
      break;
    }
    if (sel == 1) {
      gamemode = 1;
      break;
    }
    if (sel == 2) {
      gamemode = 2;
      break;
    }
    if (sel == 3) {
      gamemode = 3;
      break;
    }
    if (sel == 4) {
      gamemode = 4;
      break;
    }
    if (sel == 5) {
      gamemode = 6;
      break;
    }
  }
}

void domode1() {
  int code;
  network = 0;
  initgame();
  pulseon();
  while (!(code = iterate()))
    ++framecount;
  if (code == CODE_QUIT)
    gamemode = 0;
}

unsigned char singleoptions[10] = {2, 1, 1, 1, 0, 0, 0, 0, 0, 0};
unsigned char gameoptions[10];
char *densities[] = {"PACKED", "HIGH", "MEDIUM", "LOW"};
char *generosities[] = {"LOW", "MEDIUM", "HIGH", "RIDICULOUS"};

void domode2() {
  int sel;
  for (;;) {
    menustart();
    additem("GAME OPTIONS");
    additem("RETURN TO MAIN MENU");
    additem("DENSITY: %s", densities[singleoptions[GO_DENSITY]]);
    additem("GENEROSITY: %s", generosities[singleoptions[GO_GENEROSITY]]);
    additem("INITIAL FLAME LENGTH: %d", singleoptions[GO_FLAMES] + 1);
    additem("INITIAL NUMBER OF BOMBS: %d", singleoptions[GO_BOMBS] + 1);
    sel = domenu(2, 0);
    if (!sel) {
      gamemode = 0;
      break;
    }
    if (sel == 1) {
      singleoptions[GO_DENSITY] += menudelta;
      singleoptions[GO_DENSITY] &= 3;
    }
    if (sel == 2) {
      singleoptions[GO_GENEROSITY] += menudelta;
      singleoptions[GO_GENEROSITY] &= 3;
    }
    if (sel == 3) {
      singleoptions[GO_FLAMES] += menudelta;
      singleoptions[GO_FLAMES] &= 7;
    }
    if (sel == 4) {
      singleoptions[GO_BOMBS] += menudelta;
      singleoptions[GO_BOMBS] &= 7;
    }
  }
}

void domode3() {
  int size;
  int i, j;
  int len;
  unsigned char *put;
  unsigned char temp[64];

  pulseoff();
  if (startmatcher() < 0) {
    failure("COULD NOT START MATCHER");
    return;
  }
  makereg();
  if (!registergame()) {
    failure("COULD NOT REGISTER GAME");
    return;
  }
  numnetnodes = 1;
  bzero(netnodes, sizeof(netnodes));
  netnodes[0].used = 1;
  memmove(netnodes[0].name, playername, 16);

  myslot = 0;
  drawmode3();
  copyup();

  for (;;) {
    scaninput();
    while (anydown())
      switch (takedown()) {
      case XK_Escape:
        unregistergame();
        gamemode = 0;
        return;
      case XK_space:
      case XK_Return:
        initmyrand();
        unregistergame();
        inform(PKT_OPTIONS);
        inform(PKT_BEGIN);
        network = HOST;
        gamemode = 5;
        return;
      }
    size = getmsg(40);
    if (!(size >= 21 && *mesg == PKT_JOIN || size >= 5 && *mesg == PKT_QUIT))
      continue;
    if (memcmp(mesg + 1, regpacket + 1, 4))
      continue;
    j = -1;
    for (i = 1; i < MAXNETNODES; ++i) {
      if (!netnodes[i].used) {
        if (j == -1)
          j = i;
        continue;
      }
      if (memcmp(&netnodes[i].netname.sin_addr.s_addr, &sender.sin_addr.s_addr,
                 4))
        continue;
      if (memcmp(&netnodes[i].netname.sin_port, &sender.sin_port, 2))
        continue;
      break;
    }
    if (*mesg == PKT_QUIT) {
      if (i < MAXNETNODES)
        bzero(netnodes + i, sizeof(struct netnode));
      *temp = PKT_ACK;
      memmove(temp + 1, mesg, 5);
      putmsg(&sender, temp, 6);
    } else {
      if (i == MAXNETNODES && j == -1) {
        *mesg = PKT_INVITE;
        memmove(mesg + 1, regpacket + 1, 4);
        mesg[5] = 0xff;
        putmsg(&sender, mesg, 6);
        continue;
      }
      if (i == MAXNETNODES)
        i = j;
      memmove(&netnodes[i].netname.sin_addr.s_addr, &sender.sin_addr.s_addr, 4);
      memmove(&netnodes[i].netname.sin_port, &sender.sin_port, 2);
      netnodes[i].netname.sin_family = AF_INET;
      netnodes[i].used = 1;
      memmove(netnodes[i].name, mesg + 5, 16);
    }

    inform(PKT_INVITE);
    drawmode3();
    copyup();
  }

  gamemode = 0;
}

void domode4() {
  unsigned char querystr[16];
  long now;
  int size;
  int count;
  unsigned char *take, *end;
  int i, j, k;
  struct netnode *nn;
  char temp[64];
  int sel;
  pulseoff();
  if (!openmatcher()) {
    failure("COULD NOT CONTACT MATCHER");
    return;
  }
  *querystr = PKT_QUERY;
  bzero(querystr + 1, 4);
  memmove(querystr + 5, gameversion, 4);
  now = longtime();
  numnetnodes = 0;
  while (longtime() - now < 10) {
    putmsg(&matchername, querystr, 9);
    size = getmsg(1000);
    if (size < 7)
      continue;
    if (*mesg != PKT_INFO)
      continue;
    if (memcmp(querystr + 1, mesg + 1, 8))
      continue;
    count = (mesg[9] << 8) | mesg[10];
    if (!count) {
      failure("NO GAMES AVAILABLE");
      return;
    }
    take = mesg + 11;
    end = mesg + size;
    menustart();
    additem("JOIN NETWORK GAME");
    additem("EXIT");

    while (count && take + 26 <= end) {
      if (numnetnodes < MAXNETNODES) {
        nn = netnodes + numnetnodes++;
        bzero(&nn->netname, sizeof(struct sockaddr_in));
        nn->netname.sin_family = AF_INET;
        memmove(&nn->netname.sin_addr, take + 4, 4);
        memmove(&nn->netname.sin_port, take + 8, 2);
        memmove(nn->unique, take, 4);
        memmove(nn->name, take + 10, 16);
        memmove(temp, take + 10, 16);
        temp[16] = 0;
        additem(temp);
      }
      take += 26;
      --count;
    }
    break;
  }
  if (!numnetnodes) {
    failure("NO GAMES AVAILABLE");
    return;
  }
  sel = domenu(-1, 0);
  if (!sel) {
    gamemode = 0;
    return;
  }
  if (!tryjoin(sel - 1)) {
    gamemode = 0;
    return;
  }
  initmyrand();
  network = CLIENT;
  gamemode = 5;
}
void domode5() {
  int code;
  initgame();
  pulseon();
  while (!(code = iterate()))
    ++framecount;
  if (code == CODE_QUIT) {
    network = 0;
    gamemode = 0;
  }
}

void domode6() {
  int sel;

  for (;;) {
    menustart();
    additem("CREDITS");
    additem("RETURN TO MAIN MENU");

    addtext("NAME: XBOMB");
    addtext("VERSION: 1.8");
    addtext("ORIGINAL: BOMBER");
    addtext("ADAPTED BY: CLASS OF 99");
    addtext("RELEASE DATE: APRIL 18, 2017");
    sel = domenu(1, 0);
    if (!sel) {
      gamemode = 0;
      break;
    }
  }
}

void (*modefunctions[])() = {
    domode0, domode1, domode2, domode3, domode4, domode5, domode6,
};

void domode() { modefunctions[gamemode](); }

void addscore();

void addscore() {
  player *pl;
  pl = activeplayers.next;

  while (pl) {
    if (!(pl->flags & FLG_DEAD))
      scores[pl->controller]++;

    pl = pl->next;
  }
}

int iterate() {
  pause();
  scaninput();
  erasesprites();
  clearspritelist();
  processbombs();
  dodetonations();
  processdecays();
  processflames();
  processgenerics();
  myaction = getaction();
  if (!network && myaction == ACT_QUIT)
    return CODE_QUIT;
  networktraffic();
  if (actions[myslot] == ACT_QUIT)
    return CODE_QUIT;
  processquits();
  processplayers();
  drawbombs();
  drawbonus();
  drawgenerics();
  drawdecays();
  drawflames();
  drawplayers();
  plotsprites();
  update();
  xsync();

  if (!activegeneric.next) {
    if (!numplayers || (network && numplayers == 1)) {
      addscore();

      if (!singleHOST)
        return CODE_ALLDEAD;
      else
        return CODE_QUIT;
    }
  }
  return CODE_CONT;
}

void main(int argc, char **argv) {
  int i, j, k, t;
  long t1, t2;
  int code;
  char *p, c;

  if (argc > 1)
    mname = argv[1];
  else
    mname = strdup("localhost");

  playername = getenv("USER");

  initmyrand();
  srand(342349);

  network = 0;
  getsocket();

  soundinit("/dev/dsp");
  openx(argc, argv);
  loadgfx();

  framecount = 0;
  gamemode = 0;
  exitflag = 0;

  while (!exitflag)
    domode();

  endsound();
  closex();
  if (argc < 2)
    free(mname);
}
