#include "utility.h"

void exit_sys(const char *message)
{
  perror(message);
  exit(EXIT_FAILURE);
}

fqueue_t *fq_create_at(void *addr, int capacity, size_t fsize)
{
  fqueue_t *fq = (fqueue_t *)addr;

  fq->capacity = capacity;
  fq->front = 0;
  fq->rear = capacity -1;
  fq->size = 0;
  fq->fsize = fsize;
  
  return fq;
}

char *fq_array(fqueue_t *fq, int idx)
{
  return (char *)fq + sizeof(fqueue_t) + fq->fsize * idx;
}

int fq_isfull(fqueue_t *fq)
{
  return fq->capacity == fq->size;
}

int fq_isempty(fqueue_t *fq)
{
  return !fq->size;
}

char *fq_enqueue(fqueue_t *fq)
{
  if (fq_isfull(fq)) return NULL;

  fq->rear = (fq->rear + 1) % fq->capacity;
  ++fq->size;
  
  return fq_array(fq, fq->rear);
}

char *fq_dequeue(fqueue_t *fq)
{
  if (fq_isempty(fq)) return NULL;

  char *ret = fq_array(fq, fq->front);

  fq->front = (fq->front + 1) % fq->capacity;
  --fq->size;
  
  return ret; 
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

camqueue_t *cq_create(int capacity)
{
  camqueue_t *cq = (camqueue_t *)malloc(sizeof(camqueue_t));

  if (!cq)
    {
      fprintf(stderr, "Error: Couldn't allocate memory for camera queue\n");
      return NULL;
    }

  cq->capacity = capacity;
  cq->front = 0;
  cq->rear = cq->capacity -1;
  cq->size = 0;
  cq->array = (camera_t **)malloc(sizeof(camera_t *) * cq->capacity);

  return cq;
}

int cq_isfull(camqueue_t *cq)
{
  return cq->size == cq->capacity;
}

int cq_isempty(camqueue_t *cq)
{
  return !cq->size;
}

int cq_contains(camqueue_t *cq, const char *shmpath)
{
  if (cq_isempty(cq)) return -1;
  for (int i = 0, k = cq->front; i < cq->size; ++i, k = (k + 1) % cq->capacity)
    {
      if (!strcmp(shmpath, cq->array[k]->shmpath))
	{
	  return k;
	}
    }
  
  return -1;
}

int cq_enqueue(camqueue_t *cq, const char *shmpath, int bufsize)
{
  if (cq_isfull(cq))
    {
      fprintf(stderr, "Error: Camera queue is full.\n");
      return -1;
    }

  if (cq_contains(cq, shmpath) != -1)
    {
      fprintf(stderr, "Error: This camera is already in queue\n");
      return -1;
    }

  cq->rear = (cq->rear + 1) % cq->capacity;
  cq->array[cq->rear] = cam_create(shmpath, bufsize);

  if (!cq->array[cq->rear])
    {
      cq->rear = (cq->capacity + cq->rear - 1) % cq->capacity;
      fprintf(stderr, "Error: Couldn't allocate memory for new camera\n");
      return -1;
    }
  
  ++cq->size;
  return 0;
}

void cq_dequeue(camqueue_t *cq)
{
  if (cq_isempty(cq))
    {
      fprintf(stderr, "Error: Camqueue is empty, cannot dequeue!\n");
      return;
    }
  free(cq->array[cq->front]);
  cq->front = (cq->front + 1) % cq->capacity;
  --cq->size;
}

void cq_free(camqueue_t *cq)
{
  if (!cq) return;
  if (cq_isempty(cq)) goto end;

  for (int i = 0, k = cq->front; i < cq->size; ++i, k = (k + 1) % cq->capacity)
      cam_free(cq->array[k]);
  
 end:
  free(cq->array);
  free(cq);
}

void cq_drop(camqueue_t *cq, int idx)
{
  if (!cq) return;
  if (cq_isempty(cq)) return;

  cam_free(cq->array[idx]);
  while (idx != cq->rear)
    {
      cq->array[idx] = cq->array[(idx + 1) % cq->capacity];
      idx = (idx + 1) % cq->capacity;
    }
  cq->rear = (idx + cq->capacity - 1) % cq->capacity;
  --cq->size;
    
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

camera_t *cam_create(const char *shmpath, int bufsize)
{
  camera_t *cam = (camera_t *)malloc(sizeof(camera_t));
  if (!cam)
    {
      fprintf(stderr, "Error: Couldn't allocate memory block for camera");
      return NULL;
    }

  int fdshm = shm_open(shmpath, O_RDWR, 0);
  if (fdshm == -1)
    {
      perror("Error: shm_open failed: ");
      return NULL;
    }

  void *addr = mmap(NULL, bufsize, PROT_READ|PROT_WRITE, MAP_SHARED, fdshm, 0);
  if (!addr)
    {
      perror("Error: Couldn't map shared for new camera: ");
      close(fdshm);
      return NULL;
    }

  strncpy(cam->shmpath, shmpath, 16);
  cam->fdshm = fdshm;
  cam->bufsize = bufsize;
  cam->fqueue = (fqueue_t *)addr;

  return cam;
}

void cam_free(camera_t *cam)
{
  if (!cam) return;

  munmap(cam->fqueue, cam->bufsize);
  close(cam->fdshm);
  free(cam);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int get_shmsize(int fsize, int qsize)
{
  return sizeof(fqueue_t) + qsize * sizeof(char**) + fsize * qsize;
}
