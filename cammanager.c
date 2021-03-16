// gcc -Wall -o cammanager cammanager.c utility.c `pkg-config --libs libavutil libavcodec`
// ./cammanager 1280 800 30 300 9870

#include "utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <libavcodec/avcodec.h>
//#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>

#include <unistd.h>

fqueue_t *init_fq(void *addr, int size);

int main(int argc, const char *argv[])
{
  key_t key;
  int shmid, shsize, ret = 0;
  int width, height, fsize, fps, bufsize;
  char *shm;

  if (argc < 6)
    {
      fprintf(stderr, "Usage: %s <width> <height> <fps> <buffer size> <key>\n", argv[0]);
      return -1;
    }

  key = atoi(argv[5]);
  width  = atoi(argv[1]);
  height = atoi(argv[2]);
  fps = atoi(argv[3]);
  bufsize = atoi(argv[4]);
  fsize = width * height;
  shsize = getshmsize(fsize, bufsize);
  
  if((shmid = shmget(key, shsize, IPC_CREAT | 0666)) < 0)
    {
      perror("Error: shmget failed.");
      exit(-1);
    }

  if (!(shm = shmat(shmid, NULL, 0)))
    { 
      perror("Error: shmat failed.");
      ret = -1;
      goto close;
    }
  
  camera_t *camera = cam_create(shm, width, height, fps, bufsize, key);

  fqueue_t *fqueue = fq_create(cam_fqueue(camera), cam_bufsize(camera), cam_fsize(camera));

  
  fprintf(stderr, "debug %p\n", fqueue);
  fprintf(stderr, "%p\n", shm + shsize);
  
  camqueue_t* cq = cq_create(10);
  camera_t *cam = (camera_t*) shm;
  cq_enqueue(cq, cam);
  fqueue_t *fq = cam_fqueue(cam);
  
  FILE *rdata;
  if (!(rdata = fopen("/dev/urandom", "r")))
    {
      perror("Error: Couldn't get random data from /dev/urandom");
      ret = -1;
      goto close;
    }


  clock_t it = clock(); // initialization time
  clock_t t = clock();
  int fcount = 0;

  char *frame;
  while (1)
    {
      if ((float) (clock() - it) / CLOCKS_PER_SEC  > 30) break;
      
      if ((float) (clock() - t) / CLOCKS_PER_SEC> 0.99)
	{
	  printf("%d frames are written last sec\n", fcount);
	  fcount = 0;
	  t = clock();
	}

      if (fcount != fps)
	{
	  while (cam_turn(camera) == READER);
	  if (!(frame = fq_enqueue(fqueue)))
	    {
	      fprintf(stderr, "Error: Camera buffer is full!\n");
	      ret = -1;
	      goto close;
	    }
	  if(fread(frame, 1, fsize, rdata) < fsize)
	    {
	      fprintf(stderr, "Error: Couldn't read random data!\n");
	      ret = -1;
	      goto close;
	    }
	  cam_setturn(camera, READER);
	  ++fcount;
	}
    }

 close:
  if (rdata) fclose(rdata);
  shmctl(shmid, IPC_RMID, 0);
  return ret;
}
