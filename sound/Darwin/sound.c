/**
 *  @file   sound.c
 *  @brief  MacOS Sound
 *  @author KrizTioaN (christiaanboersma@hotmail.com)
 *  @date   2021-07-25
 *  @note   BSD-3 licensed
 *
 ***********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

char *soundnames[] =
{
"data/bomb1.raw",
"data/power1.raw",
"data/death.raw",
"data/drop.raw",
"data/bomb2.raw",
"data/power2.raw",
};

#define NUMSOUNDS	(sizeof(soundnames)/sizeof(char*))
#define MIXMAX 16

typedef struct sample
{
  char *data;
  int len;
} sample;

sample samples[NUMSOUNDS];

int soundworking=0;
int playing[MIXMAX],position[MIXMAX];

AudioComponent output_comp;
AudioComponentInstance output_instance;


OSStatus soundcallback(void *inRefCon,AudioUnitRenderActionFlags *ioActionFlags,const AudioTimeStamp *inTimeStamp,UInt32 inBusNumber,UInt32 inNumberFrames,AudioBufferList *ioData){

  int i,j,bytes_to_copy;
  char *buf=(char *)ioData->mBuffers[0].mData,*p,*pp;
  memset(buf,0,ioData->mBuffers[0].mDataByteSize);

  for(i=0;i<MIXMAX;i++)
  {
    if(!position[i]) continue;
    if(position[i]==samples[playing[i]].len)
    {
      position[i]=0;
      continue;
    }
    p=samples[playing[i]].data;
    if(!p) continue;
    p+=position[i]-1;
    if(position[i]+ioData->mBuffers[0].mDataByteSize<=samples[playing[i]].len) bytes_to_copy=ioData->mBuffers[0].mDataByteSize;
    else bytes_to_copy=samples[playing[i]].len-position[i];
    position[i]+=bytes_to_copy;
    pp=buf;
    j=bytes_to_copy;
    while(j--){
      *pp = *pp + *p - *pp * *p / 255; // avoid clipping
      pp++; p++;
    }
  }
  return noErr;
}

void readsound(int num)
{
  char name[256],*p;
  int fd,i;
  strncpy(name,soundnames[num],sizeof(name));
  fd=open(name,O_RDONLY);
  if(fd<=0) return;
  samples[num].len=lseek(fd,0,SEEK_END);
  lseek(fd,0,SEEK_SET);
  p=samples[num].data=malloc(samples[num].len);
  read(fd,p,samples[num].len);
  for(i=0;i<samples[num].len;i++) samples[num].data[i]-=127;
  close(fd);
}

void soundinit(char *name)
{
  int i;
	soundworking=0;
  AudioComponentDescription desc;
  desc.componentType=kAudioUnitType_Output;
  desc.componentSubType=kAudioUnitSubType_DefaultOutput;
  desc.componentFlags=0;
  desc.componentFlagsMask=0;
  desc.componentManufacturer=kAudioUnitManufacturer_Apple;
  output_comp=AudioComponentFindNext(NULL,&desc);
  if(!output_comp) return;
  if(AudioComponentInstanceNew(output_comp,&output_instance)) return;
  if(AudioUnitInitialize(output_instance)) return;
  AudioStreamBasicDescription streamFormat;
  UInt32 size = sizeof(AudioStreamBasicDescription);

  if(AudioUnitGetProperty(output_instance,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&streamFormat,&size)) return;
  streamFormat.mSampleRate=22050;
  streamFormat.mFormatID=kAudioFormatLinearPCM;
  streamFormat.mFormatFlags=kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsBigEndian;
  streamFormat.mFramesPerPacket=1;
  streamFormat.mChannelsPerFrame=2;
  streamFormat.mBitsPerChannel=8;
  streamFormat.mBytesPerFrame=2;
  streamFormat.mBytesPerPacket=2;
  if(AudioUnitSetProperty(output_instance,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&streamFormat,sizeof(streamFormat))) return;
  AURenderCallbackStruct renderer;
  renderer.inputProc=soundcallback;
  renderer.inputProcRefCon=NULL;
  if(AudioUnitSetProperty(output_instance,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Output,0,&renderer,sizeof (AURenderCallbackStruct))) return;
  if(AudioOutputUnitStart(output_instance)) return;
  soundworking=1;
  memset(samples,0,sizeof(samples));
  for(i=0;i<NUMSOUNDS;++i) readsound(i);
}

void playsound(int n)
{
  int i;
  if(n<NUMSOUNDS&&samples[n].len)
  {
    for(i=0;i<MIXMAX;i++)
    {
      if(!position[i])
      {
        position[i]=1;
        playing[i]=n;
        break;
      }
    }
  }
}

void endsound(void)
{
  int i;
  for(i=0;i<NUMSOUNDS;++i)
  {
    if(samples[i].len) free(samples[i].data);
  }
  AudioComponentInstanceDispose(output_instance);
  AudioOutputUnitStop(output_instance);
}
