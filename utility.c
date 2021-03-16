#include "utility.h"
#include <sys/ipc.h>
#include <sys/shm.h>


fqueue_t *fq_create(void *addr, int capacity, int framesize)
{
  fqueue_t *fqueue = (fqueue_t*)addr;
  fqueue->capacity = capacity;
 
  fqueue->size = 0;
  fqueue->front = 0;
  fqueue->rear = fqueue->capacity - 1;
   
  fqueue->array = (char**)((char*) addr + sizeof(fqueue_t));
  char* fptr = (char*) addr + sizeof(fqueue_t) +
    fqueue->capacity * sizeof(char*);
  
  for (int i = 0; i < fqueue->capacity; ++i)
    {
      fqueue->array[i] = fptr + i * framesize;
    }
  return fqueue;
}

int fq_isfull(fqueue_t *fqueue)
{
  return fqueue->capacity == fqueue->size;
}

int fq_isempty(fqueue_t *fqueue)
{
  return !fqueue->size;
}

char *fq_enqueue(fqueue_t *fqueue)
{
  if (fq_isfull(fqueue)) return NULL;

  fqueue->rear = (fqueue->rear + 1) % fqueue->capacity;
  ++fqueue->size;
  
  return fqueue->array[fqueue->rear];
}

char *fq_dequeue(fqueue_t *fqueue)
{
  if (!fqueue->size) return NULL;

  char * frame = fqueue->array[fqueue->front];
  fqueue->front = (fqueue->front + 1) % fqueue->capacity;
  --fqueue->size;
  
  return frame;
}

char *fq_front(fqueue_t *fqueue)
{
  return fq_isempty(fqueue) ? NULL : fqueue->array[fqueue->front];
}

char *fq_rear(fqueue_t *fqueue)
{
  return fq_isempty(fqueue) ? NULL : fqueue->array[fqueue->rear];
}

int fq_size(fqueue_t *fqueue)
{
  if (fqueue) return fqueue->size;
  return -1;
}

camera_t *cam_create(void *addr, int width, int height, int fps, int bufsize, int key)
{
  camera_t *camera = (camera_t*)addr;

  camera->width = width;
  camera->height = height;
  camera->fsize = camera->width * camera->height;
  camera->fps = fps;
  camera->bufsize = bufsize;
  camera->key = key;
  camera->turn = WRITER;
  camera->frame_count = 0;
  camera->fqueue = (fqueue_t*)((char*)addr + sizeof(camera_t));
    
  return camera;
}

int cam_fcnt (camera_t *camera)
{
  return ++camera->frame_count;
}

fqueue_t *cam_fqueue(camera_t *camera)
{
  return camera->fqueue;
}


void cam_free(camera_t *camera)
{
  if (camera->fqueue) free(camera->fqueue);
  if (camera) free(camera);
}

void cam_setturn(camera_t *camera, int party)
{
  if (!camera) return;
  if (party != READER && party != WRITER) return;

  camera->turn = party;
}

int cam_key(camera_t *camera)
{
  return camera->key;
}

int cam_width(camera_t *camera)
{
  return camera->width;
}

int cam_height(camera_t *camera)
{
  return camera->height;
}

int cam_fsize(camera_t *camera)
{
  return camera->fsize;
}

int cam_fps(camera_t *camera)
{
  return camera->fps;
}

int cam_bufsize(camera_t *camera)
{
  return camera->bufsize;
}
int cam_turn(camera_t *camera)
{
  return camera->turn;
}

camqueue_t *cq_create(int capacity)
{
  camqueue_t *camqueue = (camqueue_t*)malloc(sizeof(camqueue_t));
  if (!camqueue) return NULL;

  camqueue->capacity = capacity;
  camqueue->size = 0;
  camqueue->front = 0;
  camqueue->rear = camqueue->capacity - 1;
  camqueue->array = (camera_t**)malloc(sizeof(camera_t*) * camqueue->capacity);
  if (!camqueue->array)
    {
      free(camqueue);
      return NULL;
    }
  return camqueue;
}

void cq_free(camqueue_t *camqueue)
{
  if (camqueue->array) free(camqueue->array);
  if (camqueue) free(camqueue);
}

int cq_size(camqueue_t *camqueue)
{
  return camqueue->size;
}

int cq_isfull(camqueue_t *camqueue)
{
  return camqueue->capacity == camqueue->size;
}

int cq_isempty(camqueue_t *camqueue)
 {
   return !camqueue->size;
 }

int cq_capacity(camqueue_t *camqueue)
{
  return camqueue->capacity;
}

camera_t *cq_front(camqueue_t *camqueue)
{
  return cq_isempty(camqueue) ? NULL: camqueue->array[camqueue->front]; 
}

camera_t *cq_rear(camqueue_t *camqueue)
{
  return cq_isempty(camqueue) ? NULL: camqueue->array[camqueue->rear]; 
}

void cq_enqueue(camqueue_t *camqueue, camera_t *camera)
{
  if (cq_isfull(camqueue)) return;

  camqueue->rear = (camqueue->rear + 1) % camqueue->capacity;
  camqueue->array[camqueue->rear] = camera;
  ++camqueue->size;
}

camera_t *cq_dequeue(camqueue_t *camqueue)
{
  if (cq_isempty(camqueue)) return NULL;

  camera_t *camera = camqueue->array[camqueue->front];
  camqueue->front = (camqueue->front + 1) % camqueue->capacity;
  --camqueue->size;

  return camera;
}

void cq_drop(camqueue_t *camqueue, int idx)
{
  if (!camqueue) return;
  if (cq_isempty(camqueue)) return;
  
  while (idx != camqueue->rear)
    {
      camqueue->array[idx] = camqueue->array[(idx + 1) % camqueue->capacity];
      idx = (idx + 1) % camqueue->capacity;
    }
  camqueue->rear = (idx + camqueue->capacity - 1) % camqueue->capacity;
  --camqueue->size;
    
}

int cq_contains(camqueue_t *camqueue, int key)
{
  if (!camqueue) return -1;

  if (cq_isempty(camqueue)) return -1;
  
  int i;
  
  for (i = cq_frontidx(camqueue); i != cq_rearidx(camqueue); i = (i + 1) % cq_capacity(camqueue))
    {
      if (cam_key(cq_get(camqueue, i)) == key) return i;
    }
  
  if (cam_key(cq_get(camqueue, i)) == key) return i;

  return -1;
}

void cq_requeue(camqueue_t *camqueue)
{
  camera_t *camera = camqueue->array[camqueue->front];
  camqueue->front = (camqueue->front + 1) % camqueue->capacity;
  camqueue->rear = (camqueue->rear + 1) % camqueue->capacity;
  camqueue->array[camqueue->rear] = camera;
}

camera_t *cq_get(camqueue_t *camqueue, int idx)
{
  if (camqueue->array[idx]) return camqueue->array[idx];
  return NULL;
}

int cq_frontidx(camqueue_t *camqueue)
{
  return camqueue->front;
}

int cq_rearidx(camqueue_t *camqueue)
{
  return camqueue->rear;
}

int getshmsize(int fsize, int bufsize)
{
  return sizeof(camera_t) + sizeof(fqueue_t) +
    bufsize * sizeof(char*) + bufsize * fsize;
}
