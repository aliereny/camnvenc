#include "utility.h"

#include <openssl/md5.h>

void checksum(const char* frame, size_t size, char *string);
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
      if (fq_isempty(fq)) continue;
      char *frame = fq_dequeue(fq);
      char csum[33];
      checksum(frame, fsize, csum);
      fprintf(stderr, "%s\n", csum);
    }
  
  quit(0);  
}

void checksum(const char* frame, size_t size, char *string)
{
  unsigned char digest[16];
  MD5_CTX ctx;

  MD5_Init(&ctx);
  MD5_Update(&ctx, frame, size);
  MD5_Final(digest, &ctx);
  char md5string[33];
  for(int i = 0; i < 16; ++i)
    sprintf(&md5string[i*2], "%02x", (unsigned int)digest[i]);
  strcpy(string, md5string);
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
