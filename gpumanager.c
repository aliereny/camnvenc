// gcc -Wall -o gpumanager gpumanager.c utility.c `pkg-config --libs libavutil libavcodec` -L/usr/local/lib -lcrypto
// ./gpumanager 2 9870 9880 1280 800

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <libavcodec/avcodec.h>
//#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>

#include <openssl/md5.h>

#include "utility.h"

#define HWTYPE AV_HWDEVICE_TYPE_CUDA
#define PXFORMAT AV_PIX_FMT_CUDA
#define SWFORMAT AV_PIX_FMT_NV12

static AVBufferRef *hw_device_ctx;
static AVCodec *codec;
static const char enc_name[] = "h264_nvenc";
static int sessions, key_s, key_e, width, height, fsize, err;
static FILE *logfile;
static AVCodecContext **avctx;
static AVFrame *sw_frame, *hw_frame;

static void scan_keys(int key_s, int key_e,camqueue_t *camqueue);
static int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx);
static void checksum(const uint8_t* frame, char* str);
static int encode_write(AVCodecContext *avctx, AVFrame *frame, FILE *fout,
			int camkey, int frmcnt);

int main(int argc, const char *argv[])
{
  if (argc < 6)
    {
      fprintf(stderr, "Usage: %s <sessions> <start key> <end key> <width>"
	      "<height>\n", argv[0]);
      return -1;
    }

  sessions = atoi(argv[1]);
  key_s = atoi(argv[2]);
  key_e = atoi(argv[3]);
  width = atoi(argv[4]);
  height = atoi(argv[5]);
  fsize = width * height;

  camqueue_t *camqueue = cq_create(key_e - key_s); 
  
  err = av_hwdevice_ctx_create(&hw_device_ctx, HWTYPE, NULL, NULL, 0);
  if (err < 0)
    {
      fprintf(stderr, "Error: Failed to create a CUDA device. Error code: %s\n",
	      av_err2str(err));
      goto close;
    }

  logfile = fopen("log.txt", "w");
  if (!logfile)
    {
      fprintf(stderr, "Error: Couldn't open/create log.txt\n");
      err = -1;
      goto close;
    }

  codec = avcodec_find_encoder_by_name(enc_name);
  if (!codec)
    {
      fprintf(stderr, "Error: Couldn't find h264_enc encoder.\n");
      err = -1;
      goto close;
    }

  avctx = (AVCodecContext**)malloc(sizeof(AVCodecContext*) * sessions);
  if (!avctx)
    {
      fprintf(stderr, "Error: Couldn't create codec context array.\n");
      err = -1;
      goto close;
    }

  for (int i = 0; i < sessions; ++i)
    {
      avctx[i] = avcodec_alloc_context3(codec);
      if (!avctx[i])
	{
	  fprintf(stderr, "Error: Couldn't allocate context for a encoder. %s\n"
		  , av_err2str(err = AVERROR(ENOMEM)));
	  goto close;	 
	}
      avctx[i]->width     = width;
      avctx[i]->height    = height;
      avctx[i]->time_base = (AVRational){1, 30};
      avctx[i]->framerate = (AVRational){30, 1};
      avctx[i]->sample_aspect_ratio = (AVRational){1, 1};
      avctx[i]->pix_fmt   = PXFORMAT;

      err = set_hwframe_ctx(avctx[i], hw_device_ctx);
      if (err < 0)
	{
	  fprintf(stderr, "Error: Failed to set hardware context for encoder's "
		  "context.\n");
	  goto close;
	}
      
      err = avcodec_open2(avctx[i], codec, NULL);
      if (err < 0)
	{
	  fprintf(stderr, "Error: Couldn't open the encoder. Error code: %s\n",
		  av_err2str(err));
	  goto close;
	}
    }
  
 loop:
  while (1)
    {
      scan_keys(key_s, key_e, camqueue);

      if (cq_isempty(camqueue)) continue;
      fprintf(stderr, "debug 1\n");
      camera_t *cam = cq_front(camqueue);
      
      if (!cam)
	{
	  fprintf(stderr, "Error: Couldn't get camera from the queue!\n");
	  goto close;
	}

      while (cam_turn(cam) == WRITER);
      
      fprintf(stderr, "debug %p\n", cam_fqueue(cam));
      fqueue_t *fq = fq_create(cam_fqueue(cam), cam_bufsize(cam), cam_fsize(cam));
      fprintf(stderr, "debug %p\n", fq);
      if (!fq)
	{
	  fprintf(stderr, "Error: Couldn't get framequeue from the camera!\n");
	  goto close;
	}

      cam_setturn(cam, WRITER);
      //      if (fq_size(fq) < 60) continue;
	  
      for (int i = 0; i < 60; ++i)
	{
	  for (int i = 0; i < sessions; ++i)
	    {
	      fprintf(stderr, "debug 2\n");
	      cam = cq_get(camqueue, cq_frontidx(camqueue) + i);
	      if (!cam) goto loop;
	      fq = cam_fqueue(cam);
	      if (!(sw_frame = av_frame_alloc())) {
		err = AVERROR(ENOMEM);
		goto close;
	      }	  
	      sw_frame->width  = width;
	      sw_frame->height = height;
	      sw_frame->format = SWFORMAT;
	      if ((err = av_frame_get_buffer(sw_frame, 0)) < 0)
		goto close;
	      memcpy(sw_frame->data[0], fq_dequeue(fq), fsize);
	      if (!(hw_frame = av_frame_alloc())) {
		err = AVERROR(ENOMEM);
		goto close;
	      }
	      if ((err = av_hwframe_get_buffer(avctx[i]->hw_frames_ctx,
					       hw_frame, 0)) < 0) {
		fprintf(stderr, "Error: Couldn't allocate buffer for "
			"hardware. Code: %s.\n", av_err2str(err));
		goto close;
	      }
	      if (!hw_frame->hw_frames_ctx) {
		err = AVERROR(ENOMEM);
		goto close;
	      }
	      if ((err = av_hwframe_transfer_data(hw_frame, sw_frame, 0)) < 0) {
		fprintf(stderr, "Error while transferring frame data to surface."
			"Error code: %s.\n", av_err2str(err));
		goto close;
	      }
	      if ((err = (encode_write(avctx[i], hw_frame, logfile,
				       cam_key(cam), cam_fcnt(cam)))) < 0) {
		fprintf(stderr, "Error: Failed to encode.\n");
		goto close;
	      }
	      av_frame_free(&hw_frame);
	      av_frame_free(&sw_frame);
	    }
	}
    }

 close:
  av_frame_free(&sw_frame);
  av_frame_free(&hw_frame);
  for (int i = 0; i < sessions; ++i)
    {
      avcodec_free_context(avctx + i);
    }
  free(avctx);
  av_buffer_unref(&hw_device_ctx);
  if (logfile) fclose(logfile);
  return err;
}

//scan_keys(key_s, key_e, shmsize, camqueue);
static void scan_keys(int key_s, int key_e, camqueue_t *camqueue)
{
  int shmid, signal, idx;
  for (int key = key_s; key <= key_e; ++key)
    {
      shmid = shmget(key, getshmsize(fsize, 60), 0666);
      if (errno != ENOENT) fprintf(stderr, "Error: shmget\n");
      signal = shmid < 0 ? 0 : 1;
      idx = cq_contains(camqueue, key);

      if (!signal && idx != -1)
	{
	  fprintf(stderr, "debug 3\n");
	  camera_t *camera = cq_get(camqueue, idx);
	  fprintf(stderr, "Info: Camera with key %d lost and removed "
		  "from the queue!\n", cam_key(camera));
	  cq_drop(camqueue, idx);
	}
      if (signal && idx == -1)
	{
	  fprintf(stderr, "debug 4\n");
	  camera_t *shmptr = shmat(shmid, NULL, 0);
	  if (!shmptr)
	    {
	      fprintf(stderr, "Error: shmat failed with key %d; %s", key,
		      strerror(errno));
	      continue;
	    }
	  camera_t *camera = shmptr;
	  cq_enqueue(camqueue, camera);
	  fprintf(stderr, "Info: Camera with key %d added to the queue!\n", cam_key(camera));

	}
    }
}

static int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx)
{
  AVBufferRef *hw_frames_ref;
  AVHWFramesContext *frames_ctx = NULL;
  int err = 0;
  hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx);
  if (!hw_frames_ref)
    {
      fprintf(stderr, "Error: Failed to create hardware frame context.\n");
      return -1;
    }

  frames_ctx = (AVHWFramesContext *)(hw_frames_ref->data);
  frames_ctx->format    = PXFORMAT;
  frames_ctx->sw_format = SWFORMAT;
  frames_ctx->width     = width;
  frames_ctx->height    = height;
  frames_ctx->initial_pool_size = 20;

  err = av_hwframe_ctx_init(hw_frames_ref);
  if (!hw_frames_ref)
    {
      fprintf(stderr, "Error: Failed to initialize hardware frame context. "
	      "Error code: %s\n", av_err2str(err));
      av_buffer_unref(&hw_frames_ref);
      return err;
    }

  ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
  if (!ctx->hw_frames_ctx)
    {
      err = AVERROR(ENOMEM);
    }
  av_buffer_unref(&hw_frames_ref);

  return err;
}

static int encode_write(AVCodecContext *avctx, AVFrame *frame, FILE *fout,
			int camkey, int frmcnt)
{
  int ret = 0;
  AVPacket enc_pkt;
  av_init_packet(&enc_pkt);
  enc_pkt.data = NULL;
  enc_pkt.size = 0;
  if ((ret = avcodec_send_frame(avctx, frame)) < 0) {
    fprintf(stderr, "Error: Could'nt send frames to encoder "
	    "Code: %s\n", av_err2str(ret));
    goto end;
  }
  while (1) {
    ret = avcodec_receive_packet(avctx, &enc_pkt);
    if (ret)
      break;
    enc_pkt.stream_index = 0;
    char logbuf[100];
    char *target = logbuf;
    target += sprintf(logbuf, "camera=%6d frame=%7d checksum=", camkey, frmcnt);
    checksum(enc_pkt.data, target);
    ret = fwrite(logbuf, strlen(logbuf), 1, fout);
    av_packet_unref(&enc_pkt);
  }
 end:
  ret = ((ret == AVERROR(EAGAIN)) ? 0 : -1);
  return ret;
}

static void checksum(const uint8_t* frame, char* str)
{
  unsigned char digest[16];
  MD5_CTX ctx;

  MD5_Init(&ctx);
  MD5_Update(&ctx, frame, sizeof(frame));
  MD5_Final(digest, &ctx);
  
  char md5string[33];
  for(int i = 0; i < 16; ++i)
    sprintf(&md5string[i*2], "%02x", (unsigned int)digest[i]);
  sprintf(str, "%s\n", md5string);
}
