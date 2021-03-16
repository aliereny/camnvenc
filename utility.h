#ifndef FQUEUE
#define FQUEUE

#include <stdlib.h>
#include <stdio.h>

#define READER 1
#define WRITER -1


typedef struct fqueue_t
{
  int capacity;
  int size;
  int front;
  int rear;
  char **array;
} fqueue_t;

typedef struct camera_t
{
  int width;
  int height;
  int fsize;
  int fps;
  int bufsize;
  int key;
  int turn;
  int frame_count;
  fqueue_t *fqueue;
  
} camera_t;

typedef struct camqueue_t
{
  int capacity;
  int size;
  int front;
  int rear;
  camera_t **array;
} camqueue_t;

int getshmsize(int fsize, int bufsize);

// Frame queue function declarations
fqueue_t *fq_create(void *addr, int capacity, int framesize);

char* fq_enqueue(fqueue_t *fqueue);
char* fq_dequeue(fqueue_t *fqueue);
char* fq_front(fqueue_t *fqueue);
char* fq_rear(fqueue_t *fqueue);

int fq_isfull(fqueue_t *fqueue);
int fq_isempty(fqueue_t *fqueue);
int fq_size(fqueue_t *fqueue);

// Camera function declarations
camera_t *cam_create(void *addr, int width, int height, int fps, int bufsize, int key);

fqueue_t *cam_fqueue(camera_t *camera);

int cam_key(camera_t *camera);
int cam_efc(camera_t *camera);
int cam_width(camera_t *camera);
int cam_height(camera_t *camera);
int cam_fsize(camera_t *camera);
int cam_fps(camera_t *camera);
int cam_turn(camera_t *camera);
int cam_bufsize(camera_t *camera);
int cam_fcnt (camera_t *camera);
  
void cam_setturn(camera_t *camera, int party);

// Camera queue function declarations
camqueue_t *cq_create(int capacity);

camera_t *cq_front(camqueue_t *camqueue);
camera_t *cq_dequeue(camqueue_t *camqueue);
camera_t *cq_rear(camqueue_t *camqueue);
camera_t *cq_get(camqueue_t *camqueue, int idx);

int cq_capacity(camqueue_t *camqueue);
int cq_size(camqueue_t *camqueue);
int cq_isfull(camqueue_t *camqueue);
int cq_isempty(camqueue_t *camqueue);
int cq_contains(camqueue_t *camqueue, int key);
int cq_size(camqueue_t *camqueue);
int cq_rearidx(camqueue_t *camqueue);
int cq_frontidx(camqueue_t *camqueue);

void cq_free(camqueue_t *camqueue);
void cq_enqueue(camqueue_t *camqueue, camera_t *camera);
void cq_drop(camqueue_t *camqueue, int idx);
void cq_requeue(camqueue_t *camqueue);

#endif
