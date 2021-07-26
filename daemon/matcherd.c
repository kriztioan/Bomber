/**
 *  @file   matcherd.c
 *  @brief  Multiplayer Matcher
 *  @author David Ashley (dash@xdr.com)
 *  @date   1999-02-07
 *  @note   GPL licensed
 *
 ***********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>


#define MAXMSG 256
#define PORT 5521

#define TIMETOLIVE 600 // seconds

typedef unsigned char uchar;
char logging=0;

unsigned char mesg[MAXMSG];
int running;
int lastsize;
int network;
char masterhostname[256];

#define PKT_REGISTER 16
#define PKT_ACK 24
#define PKT_INFO 40
#define PKT_QUERY 32

#define MAXMATCHES 16


struct registration
{
	uchar id;
	uchar unique[4];
	uchar password[4];
	uchar version[4];
	uchar name[16];
	uchar status;
};
struct query
{
	uchar id;
	uchar password[4];
	uchar version[4];
};

struct gamehost
{
	struct gamehost *next,*prev;
	uchar machine[4];
	uchar port[2];
	struct registration reg;
	long timeout;
};


struct gamehost *freehosts=0,*activehosts=0;



int udpsocket,myport;
struct sockaddr_in myname={0},mastername={0};
socklen_t senderlength;
struct sockaddr_in sender={0};

long longtime(void)
{
struct timeb tb;
	ftime(&tb);
	return tb.time;
}

char *timestr()
{
static char timestring[80];

time_t t;
int l;
	time(&t);
	strcpy(timestring,ctime(&t));
	l=strlen(timestring);
	if(l && timestring[l-1]=='\n') timestring[l-1]=0;
	return timestring;
}


int putmsg(struct sockaddr_in *toname,unsigned char *msg,int len)
{
int status;
/*  int i;
 printf("send: matcher: ");
 for(i = 0; i < len; i++) printf("%d ", (int) msg[i]);
 printf("\n");*/
	status=sendto(udpsocket,msg,len,0,
		(struct sockaddr *)toname,sizeof(struct sockaddr_in));
	return status;
}
void ack()
{
uchar copy[256];

	*copy=PKT_ACK;
	memmove(copy+1,mesg,lastsize);
	putmsg(&sender,copy,lastsize+1);
}

int getmsg(int seconds)
{
int size;

		lastsize=-1;
		memset(&sender,0,sizeof(sender));
		senderlength=sizeof(sender);
		if(seconds)
		{
			struct timeval timeout;
			fd_set readfds;
			int res;

			timeout.tv_sec=seconds;
			timeout.tv_usec=0;
			FD_ZERO(&readfds);
			FD_SET(udpsocket,&readfds);
			res=select(udpsocket+1,&readfds,0,0,&timeout);
			if(res<=0) return -1;
		}
		lastsize=size=recvfrom(udpsocket,mesg,MAXMSG,0,
			(struct sockaddr *)&sender,&senderlength);
		return size;
}

long longind(unsigned char *p)
{
	return (p[0]<<24L) | (p[1]<<16L) | (p[2]<<8) | p[3];
}
short shortind(unsigned char *p)
{
	return (p[0]<<8L) | p[1];
}


void openport(int portwant)
{
int status;

	myport=portwant;
	udpsocket=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if(udpsocket==-1)
	{
		perror("socket()");
		exit(1);
	}
	memset(&myname,0,sizeof(myname));
	myname.sin_family=AF_INET;
	myname.sin_addr.s_addr=htonl(INADDR_ANY);
	myname.sin_port=htons(myport);

	status=bind(udpsocket,(struct sockaddr *) &myname,sizeof(myname));
	if(status==-1)
	{
		perror("bind()");
		exit(1);
	}
}

#define PERBLOCK 512

struct gamehost *newhost()
{
struct gamehost *h,*block;
int i;

	if(!freehosts)
	{
		block=malloc(sizeof(struct gamehost)*PERBLOCK);
		if(!block) return 0;
		freehosts=block;
		i=PERBLOCK-1;
		while(i--)
		{
			block->next=block+1;
			++block;
		}
		block->next=0;
	}
	h=freehosts;
	freehosts=freehosts->next;
	memset(h,0,sizeof(struct gamehost));
	return h;
}

void freehost(struct gamehost *h)
{
	h->next=freehosts;
	freehosts=h;
}

struct gamehost *findmatch(struct registration *key)
{
struct gamehost *h;
	h=activehosts;
	while(h)
	{
		if(!memcmp(&h->reg,key,sizeof(struct registration)-1))
			return h;
		h=h->next;
	}
	return 0;
}



void insert(struct gamehost *h)
{
	if(activehosts)
	{
		h->next=activehosts;
		h->prev=activehosts->prev;
		activehosts->prev=h;
		activehosts=h;
	} else
	{
		h->next=h->prev=0;
		activehosts=h;
	}
}
void delete(struct gamehost *h)
{
	if(h->prev)
		h->prev->next=h->next;
	else
		activehosts=h->next;
	if(h->next)
		h->next->prev=h->prev;
	freehost(h);
}
void localtoremote()
{

  struct ifaddrs *myaddrs, *ifa;
  void *in_addr;
  char buf[64];
  if(getifaddrs(&myaddrs)!=0){return;}
  for(ifa=myaddrs;ifa!=NULL;ifa=ifa->ifa_next)
  {
    if(ifa->ifa_addr==NULL) continue;
    if(!(ifa->ifa_flags&IFF_UP)) continue;
    if(ifa->ifa_addr->sa_family!=AF_INET) continue;
    struct sockaddr_in *s4=(struct sockaddr_in*)ifa->ifa_addr;
    in_addr=&s4->sin_addr;
    if(!inet_ntop(ifa->ifa_addr->sa_family, in_addr, buf, sizeof(buf))) continue;
    if(strncmp("127.0.", buf, 6) == 0) continue;
    inet_aton(buf,&(sender.sin_addr));
    return;
  }
}
void doreg()
{
struct registration *new;
struct gamehost *match;
long now;

 if(strncmp("127.0.", inet_ntoa(sender.sin_addr), 6)==0) localtoremote();

	new=(struct registration *)mesg;
	match=findmatch(new);
	if(logging)
	{
		unsigned long addr=ntohl(sender.sin_addr.s_addr);
		unsigned short port=ntohs(sender.sin_port);
		printf("reg  :%s:%lu.%lu.%lu.%lu:%u  %c%x '%s'\n",timestr(),
			(addr>>24)&255,(addr>>16)&255,(addr>>8)&255,addr&255,port,
                       new->status ? '+' : '-',(uint)match,new->name);
		fflush(stdout);
	}
	if(!match && !new->status) {ack();return;}
	if(match && new->status) {ack();return;}
	if(!match && new->status)
	{
		match=newhost();
		if(!match) return; // No memory, what can we do?
		memmove(match->machine,&sender.sin_addr.s_addr,4);
		memmove(match->port,&sender.sin_port,2);
		match->reg=*new;
		now=longtime();
		match->timeout=now+TIMETOLIVE;
		ack();
		insert(match);
		return;
	} else // match && !new->status
	{
		delete(match);
		ack();
		return;
	}
}

void doquery()
{
uchar *password;
uchar *version;
struct gamehost *h;
uchar response[2048],*rput,*countersave;
int counter;

	if(logging)
	{
		unsigned long addr=ntohl(sender.sin_addr.s_addr);
		unsigned short port=ntohs(sender.sin_port);
		printf("query:%s:%lu.%lu.%lu.%lu:%d\n",timestr(),
			(addr>>24)&255,(addr>>16)&255,(addr>>8)&255,addr&255,port);
		fflush(stdout);
	}
	password=mesg+1;
	version=mesg+5;
	h=activehosts;
	rput=response;
	*rput++=PKT_INFO;
	memmove(rput,password,4);
	rput+=4;
	memmove(rput,version,4);
	rput+=4;
	countersave=rput;
	*rput++=0;
	*rput++=0;
	counter=0;

	while(h)
	{
		if(!memcmp(password,h->reg.password,4) &&
			!memcmp(version,h->reg.version,4) && counter<MAXMATCHES)
		{
			++counter;
			memmove(rput,h->reg.unique,4);
			rput+=4;
			memmove(rput,h->machine,4);
			rput+=4;
			memmove(rput,h->port,2);
			rput+=2;
			memmove(rput,h->reg.name,sizeof(h->reg.name));
			rput+=sizeof(h->reg.name);
		}
		h=h->next;
	}
	*countersave++=counter>>8;
	*countersave++=counter;
	putmsg(&sender,response,rput-response);

}

void purge(long cutoff)
{
struct gamehost *h,*h2;
	h=activehosts;
	while(h)
	{
		h2=h;
		h=h->next;
		if(cutoff-h2->timeout>0)
		{
			delete(h2);
		}
	}
}

char pidfile[128];
void writepid()
{
 int fd;
 int size;
 pid_t pid;
 strncpy(pidfile,"/tmp/matcher.pid",128);
 fd=open(pidfile, O_WRONLY | O_CREAT);
 pid=getpid();
 size=write(fd,&pid,sizeof(pid_t));
 close(fd);
}

void signalhandler(int signal)
{
  running=0;
}
void registersignals(){
  signal(SIGHUP,signalhandler);
  signal(SIGINT,signalhandler);
  signal(SIGQUIT,signalhandler);
  signal(SIGKILL,signalhandler);
  signal(SIGTERM,signalhandler);
  signal(SIGSTOP,signalhandler);
}

int main(int argc,char **argv)
{
int i;
int want;
int size;
long purgetime;
long now;

        registersignals();
	want=PORT;
	if(argc>1)
	{
		for(i=1;i<argc;++i)
			if(!strncmp(argv[i],"-p",2))
			{
				if(strlen(argv[i])>2) want=atoi(argv[i]+2);
				else if(i+1<argc) want=atoi(argv[i+1]);
			}
	}
	freehosts=0;
	openport(want);
	purgetime=longtime()+TIMETOLIVE;
        writepid();
	running=1;
	while(running)
	{
		size=getmsg(10);
		if(size>=1)
			switch(*mesg)
			{
			case PKT_REGISTER:
				if(size<sizeof(struct registration)) continue;
				doreg();
				break;
			case PKT_QUERY:
				if(size<sizeof(struct query)) continue;
				doquery();
				break;
			}
		now=longtime();
		if(now-purgetime>0) // avoid year 203x bug...
		{
			purge(purgetime);
			purgetime+=TIMETOLIVE;
		}
	}
	unlink(pidfile);
}
