#include "utility.h"

void scan_cameras(camqueue_t *cq, int bufsize);
void quit(int signal);

static int cq_cap = 10;

camqueue_t *cq;

int main(int argc, const char *argv[])
{
  if (signal(SIGINT, quit) == SIG_ERR)
    sclose("signal");

  cq = cq_create(cq_cap);

  int fsize = WIDTH * HEIGHT;
  int shmsize = get_shmsize(fsize, QSIZE);

  while (1)
    {
      scan_cameras(cq, shmsize);
      if (cq_isempty(cq)) continue;

      fqueue_t *fq = cq->array[cq->front]->fqueue;

      fq_dequeue(fq);
    }
  
  quit(0);  
}

void scan_cameras(camqueue_t *cq, int bufsize)
{
  static char campath[16];
  int fdshm, idx;
  for (int i = 0; i < cq_cap; ++i)
    {
      sprintf(campath, "/camera_%d", i);
      fdshm = shm_open(campath, O_RDWR, 0);
      close(fdshm);
      idx = cq_contains(cq, campath);
      if (fdshm >= 0 && idx == -1) // New camera is found
	{
	  if (cq_enqueue(cq, campath, bufsize))
	    {
	      fprintf(stderr, "Error: Couldn't enqueue a new camera\n");
	      continue;
	    }
	  fprintf(stderr, "Info: A new camera at path %s has added.\n",
		  campath);
	}
      if (fdshm < 0 && idx != -1) // A camera is lost
	{
	  cq_drop(cq, idx);
	  fprintf(stderr, "Info: Camera at path %s is lost.\n", campath);
	}
    }
}

void quit(int signal)
{
  cq_free(cq);
  if (signal)
    fprintf(stderr, "\nInfo: Console signal recieved, safe quitting.\n");
  exit(signal);
}
