/**
 *  @file   sound.c
 *  @brief  Linux Sound
 *  @author David Ashley (dash@xdr.com)
 *  @date   1999-02-07
 *  @note   GPL licensed
 *
 ***********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

char dirlist[]="data";

#define NUMSOUNDS	(sizeof(soundnames)/sizeof(char*))
#define MIXMAX 16

#define SOUND_EXIT -2
#define SOUND_QUIET -1

char *soundnames[] =
{
"bomb1.raw",
"power1.raw",
"death.raw",
"drop.raw",
"bomb2.raw",
"power2.raw",
};
typedef struct sample
{
	char *data;
	int len;
} sample;

sample samples[NUMSOUNDS];

int soundworking=0;
int fragment;
int dsp;
int soundwrite,soundread;
int *soundbuffer;
int soundbufferlen;



soundinit(char *name)
{
int fd[2];
char devname[256];
int value;

	soundworking=0;
	pipe(fd);
	soundread=fd[0];
	soundwrite=fd[1];
	if(fork())
	{
		close(soundread);
		return;
	}
	close(soundwrite);
	memset(samples,0,sizeof(samples));
	strcpy(devname,name);
	dsp=open(devname,O_RDWR);
	if(dsp<0) goto failed;
	fragment=0x20009;
	ioctl(dsp,SNDCTL_DSP_SETFRAGMENT,&fragment);
    value = 1;
    ioctl(dsp, SNDCTL_DSP_STEREO, &value);
    value=22050;
	ioctl(dsp,SNDCTL_DSP_SPEED,&value);
	ioctl(dsp,SNDCTL_DSP_GETBLKSIZE,&fragment);
	if(!fragment) {close(dsp);goto failed;}
	soundbufferlen=fragment*sizeof(int);
	soundbuffer=malloc(soundbufferlen);
	if(!soundbuffer) goto failed;
	value=fcntl(soundread,F_SETFL,O_NONBLOCK);
	soundworking=1;
failed:
	doall();
	exit(0);
}
int readsound(int num)
{
char name[256],*p1,*p2,ch;
int i,file,size,len;
	p1=dirlist;
	for(;;)
	{
		p2=name;
		while(*p1 && (ch=*p1++)!=',')
			*p2++=ch;
		if(p2>name && p2[-1]!='/') *p2++='/';
		strcpy(p2,soundnames[num]);
		file=open(name,O_RDONLY);
		if(file>=0) break;
		if(!*p1)
		{
			samples[num].len=-1;
			return 0;
		}
	}
	size=lseek(file,0,SEEK_END);
	lseek(file,0,SEEK_SET);
	len=samples[num].len=(size+fragment-1)/fragment;
	len*=fragment;
	p1=samples[num].data=malloc(len);
	if(p1)
	{
		i=read(file,p1,size);
		if(len-size) memset(p1+size,0,len-size);
		while(size--) *p1++ ^= 0x80;
	} else
		samples[num].data=0;
	close(file);
}

doall()
{
unsigned char clip[8192];
int i,j;
char commands[64],commandlen,com;
char *p;
int *ip;
int playing[MIXMAX],position[MIXMAX];
int which;

	while(!soundworking)
	{
		commandlen=read(soundread,commands,1);
		if(commandlen!=1) continue;
		com=*commands;
		if(com==SOUND_EXIT) exit(0);
	}
	for(i=0;i<8192;i++)
	{
		j=i-4096;
		clip[i]=j > 127 ? 255 : (j<-128 ? 0 : j+128);
	}
	for(i=0;i<NUMSOUNDS;++i)
		readsound(i);
	memset(playing,0,sizeof(playing));
	memset(position,0,sizeof(position));
	for(;;)
	{
		commandlen=read(soundread,commands,64);

		if(commandlen<0)
		{
			commandlen=0;
			if(errno==EPIPE) exit(0);
		} else if(commandlen==0) exit(0);
		p=commands;
		while(commandlen--)
		{
			com=*p++;
			if(com==SOUND_QUIET) {memset(position,0,sizeof(position));continue;}
			if(com==SOUND_EXIT) exit(0);
			if(com<NUMSOUNDS)
			{
				for(i=0;i<MIXMAX;++i)
					if(!position[i])
					{
						position[i]=1;
						playing[i]=com;
						break;
					}
			}
		}
		memset(soundbuffer,0,soundbufferlen);
		for(i=0;i<MIXMAX;++i)
		{
			if(!position[i]) continue;
			which=playing[i];
			if(position[i]==samples[which].len)
			{
				position[i]=0;
				continue;
			}
			p=samples[which].data;
			if(!p) continue;
			p+=fragment*(position[i]++ -1);
			ip=soundbuffer;
			j=fragment;
			while(j--) *ip++ += *p++;
		}
		j=fragment;
		ip=soundbuffer;
		p=(char *) soundbuffer;
		while(j--) *p++ = clip[4096+*ip++];
		write(dsp,(char *)soundbuffer,fragment);
	}
}

void playsound(int n)
{
char c;
	c=n;
	write(soundwrite,&c,1);
}

void endsound(void)
{
	playsound(SOUND_EXIT);
}
