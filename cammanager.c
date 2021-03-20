#include "utility.h"

#include <time.h>

void quit(int signal);

int fdshm, mapped = 0, fsize;
const int width = WIDTH, height = HEIGHT, qsize = QSIZE, fps = FPS;
size_t bufsize;
void *addr;
FILE *urand;
const char *shmpath;

int main(int argc, const char *argv[])
{
  if (argc < 2 || argc > 2)
    {
      fprintf(stderr, "Usage: %s <shm_path>\n", argv[0]);
      exit(-1);
    }

  fsize = width*height;
  bufsize = get_shmsize(fsize, qsize);
  shmpath = argv[1];

  if (signal(SIGINT, quit) == SIG_ERR)
    sclose("signal");
  
  if ((fdshm = shm_open(shmpath, O_RDWR, 0)) != -1)
    {
      fprintf(stderr, "Error: This shared memory path is already in use!\n");
      close(fdshm);
      exit(-1);
    }
  if ((fdshm = shm_open(shmpath, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
    sclose("shmopen");

  if (ftruncate(fdshm, bufsize))
    sclose("ftruncate");
  
  if (!(addr = mmap(NULL, bufsize, PROT_READ|PROT_WRITE, MAP_SHARED, fdshm, 0)))
    sclose("mmap");
  mapped = 1;

  if (!(urand = fopen("/dev/urandom", "r")))
    sclose("fopen");

  fqueue_t *fq = fq_create_at(addr, qsize, fsize);
  int fcnt = 0;
  clock_t c = clock();
  char *frame;

  while (1)
    {
      if (dtime(c) >= 1.)
	{
	  fprintf(stderr, "Info: %2d frames are written into the shared "
		  "memory last second\n", fcnt);
	  fcnt = 0;
	  c = clock();
	  continue;
	}
      
      if (fcnt == fps) continue;

      if(!(frame = fq_enqueue(fq)))
	{
	  fprintf(stderr, "Error: Frames queue is full, terminating.\n");
	  quit(0);
	}
      
      if (fread(frame, 1, fsize, urand) < fsize)
	sclose("Error: Couldn't read random data, terminating.\n");
      ++fcnt;
    }

  quit(0);
}

void quit(int signal)
{
  if (urand) fclose(urand);
  if (mapped) munmap(addr, bufsize);
  if (fdshm != -1) close(fdshm);
  if (shm_unlink(shmpath) == -1)
    perror("shm_unlink");
  if (signal)
    fprintf(stderr, "\nInfo: Console signal recieved, safe quitting.\n");
  exit(signal);
}
