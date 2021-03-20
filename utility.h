#ifndef UTILITY
#define UTILITY

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define sclose(msg) \
  do { perror(msg); quit(0); } while (0)

#define dtime(t0) \
  ((double) (clock() - (t0)) / CLOCKS_PER_SEC)

typedef struct fqueue_t
{
  int capacity;
  int front;
  int rear;
  int size;
  int fsize;
} fqueue_t;

typedef struct camera_t
{
  char shmpath[16];
  int fdshm;
  int bufsize;
  int frmcnt;
  fqueue_t *fqueue;
} camera_t;

typedef struct camqueue_t
{
  int capacity;
  int front;
  int rear;
  int size;
  camera_t **array;
} camqueue_t;

void exit_sys(const char *message);
  
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

fqueue_t *fq_create_at(void *addr, int capacity, size_t fsize);

int fq_isfull(fqueue_t *fq);
int fq_isempty(fqueue_t *fq);

char *fq_enqueue(fqueue_t *fq);
char *fq_dequeue(fqueue_t *fq);
char *fq_array(fqueue_t *fq, int idx);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

camqueue_t *cq_create(int capacity);

camera_t *cq_front(camqueue_t *cq);
camera_t *cq_rear(camqueue_t *cq);
camera_t *cq_next(camqueue_t *cq);

int cq_isfull(camqueue_t *cq);
int cq_isempty(camqueue_t *cq);
int cq_contains(camqueue_t *cq, const char *shmpath);
int cq_enqueue(camqueue_t *cq, const char *shmpath, int bufsize);
 
void cq_dequeue(camqueue_t *cq);
void cq_free(camqueue_t *cq);
void cq_drop(camqueue_t *cq, int idx);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

camera_t *cam_create(const char *shmpath, int bufsize);

void cam_free(camera_t *cam);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int get_shmsize(int fsize, int qsize);
  
#endif
