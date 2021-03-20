#include "utility.h"

#include <openssl/md5.h>

#include <libavcodec/avcodec.h>

#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>

#define HWTYPE AV_HWDEVICE_TYPE_CUDA
#define PXFORMAT AV_PIX_FMT_CUDA
#define SWFORMAT AV_PIX_FMT_NV12

char *checksum(const void* frame, size_t size);
void scan_cameras(camqueue_t *cq, int bufsize);
void quit(int signal);

int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx);
int encode_write(AVCodecContext *avctx, AVFrame *frame, FILE *fout,
		 char *cam_shm_path, int frmcnt);

AVBufferRef *hw_device_ctx;
AVCodecContext *avctx[2];
AVCodec *codec;
AVFrame *sw_frame, *hw_frame;
const char enc_name[] = "h264_nvenc";

FILE *logfile;
int width, height, fsize, shmsize, sessions = 2, err;
const int cq_cap = 10;
camqueue_t *cq;


int main(int argc, const char *argv[])
{

  /*if (argc < 2 || argc > 2)
    {
      fprintf(stderr, "Usage: %s <sessions>", argv[0]);
      exit(-1);
    }

    sessions = atoi(argv[1]);*/
  
  if (signal(SIGINT, quit) == SIG_ERR)
    sclose("signal");

  cq = cq_create(cq_cap);
  
  width = WIDTH;
  height = HEIGHT;
  fsize = width * height;
  shmsize = get_shmsize(fsize, QSIZE);

  if ((err = av_hwdevice_ctx_create(&hw_device_ctx, HWTYPE, NULL, NULL, 0)) < 0)
    {
      fprintf(stderr, "Error: Failed to create a CUDA device. Error code: %s\n",
	      av_err2str(err));
      quit(-1);
    }

  logfile = fopen("log.txt", "w");
  if (!logfile)
    sclose("Error: Couldn't open/create log.txt\n");
  
  if (!(codec = avcodec_find_encoder_by_name(enc_name)))
    sclose("Error: Couldn't find h264_enc encoder.\n");
 
  for (int i = 0; i < sessions; ++i)
    {
      avctx[i] = avcodec_alloc_context3(codec);
      if (!avctx[i])
	{
	  fprintf(stderr, "Error: Couldn't allocate context for a encoder. %s\n"
		  , av_err2str(err = AVERROR(ENOMEM)));
	  quit(-1);
	}
      
      avctx[i]->width     = width;
      avctx[i]->height    = height;
      avctx[i]->time_base = (AVRational){1, 30};
      avctx[i]->framerate = (AVRational){30, 1};
      avctx[i]->sample_aspect_ratio = (AVRational){1, 1};
      avctx[i]->pix_fmt   = PXFORMAT;

      if ((err = set_hwframe_ctx(avctx[i], hw_device_ctx)) < 0)
	{
	  fprintf(stderr, "Error: Failed to set hardware context for encoder's "
		  "context.\n");
	  quit(-1);
	}
      
      if ((err = avcodec_open2(avctx[i], codec, NULL)) < 0)
	{
	  fprintf(stderr, "Error: Couldn't open the encoder. Error code: %s\n",
		  av_err2str(err));
	  quit(-1);
	}
    }

  camera_t *cams[2];
  int ready_cams;
  fqueue_t *fq;
  
  while (1)
    {
      scan_cameras(cq, shmsize);
      if (cq_isempty(cq)) continue;

      if ((cams[0] = cq_front(cq))->fqueue->size >= 60)
	  ready_cams = 1;
      else continue;
      
      if (cq->size > 1 && (cams[1] = cq_next(cq))->fqueue->size > 60)
	ready_cams = 2;

      for (int i = 0; i < 60; ++i)
	{
	  for (int i = 0; i < ready_cams; ++i)
	    {
	      fq = cams[i]->fqueue;
	      if (!(sw_frame = av_frame_alloc())) {
		err = AVERROR(ENOMEM);
		quit(-1);
	      }	  
	      sw_frame->width  = width;
	      sw_frame->height = height;
	      sw_frame->format = SWFORMAT;
	      if ((err = av_frame_get_buffer(sw_frame, 0)) < 0)
		quit(-1);
	      memcpy(sw_frame->data[0], fq_dequeue(fq), fsize);
	      if (!(hw_frame = av_frame_alloc())) {
		err = AVERROR(ENOMEM);
		quit(-1);
	      }
	      if ((err = av_hwframe_get_buffer(avctx[i]->hw_frames_ctx,
					       hw_frame, 0)) < 0) {
		fprintf(stderr, "Error: Couldn't allocate buffer for "
			"hardware. Code: %s.\n", av_err2str(err));
		quit(-1);
	      }
	      if (!hw_frame->hw_frames_ctx) {
		err = AVERROR(ENOMEM);
		quit(-1);
	      }
	      if ((err = av_hwframe_transfer_data(hw_frame, sw_frame, 0)) < 0) {
		fprintf(stderr, "Error while transferring frame data to surface."
			"Error code: %s.\n", av_err2str(err));
		quit(-1);
	      }
	      if ((err = (encode_write(avctx[i], hw_frame, logfile,
				       cams[i]->shmpath, cams[i]->frmcnt))) < 0){
		fprintf(stderr, "Error: Failed to encode.\n");
		quit(-1);
	      }
	      av_frame_free(&hw_frame);
	      av_frame_free(&sw_frame);
	      ++cams[i]->frmcnt;
	    }
	}
    }
  
  quit(0);  
}

char *checksum(const void* frame, size_t size)
{
  unsigned char digest[16];
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, frame, size);
  MD5_Final(digest, &ctx);
  char *md5string = (char *)malloc(33);
  if (!md5string) return NULL;
  for(int i = 0; i < 16; ++i)
    sprintf(&md5string[i*2], "%02x", (unsigned int)digest[i]);
  return md5string;
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
  av_frame_free(&sw_frame);
  av_frame_free(&hw_frame);
  for (int i = 0; i < sessions; ++i)
    {
      avcodec_free_context(avctx + i);
    }
  av_buffer_unref(&hw_device_ctx);
  if (logfile) fclose(logfile);
  cq_free(cq);
  if (signal)
    fprintf(stderr, "\nInfo: Console signal recieved, safe quitting.\n");
  exit(signal);
}

int set_hwframe_ctx(AVCodecContext *ctx, AVBufferRef *hw_device_ctx)
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

int encode_write(AVCodecContext *avctx, AVFrame *frame, FILE *fout,
			char *shmpath, int frmcnt)
{
  int ret = 0;
  AVPacket enc_pkt;
  static char logbuf[128];
  char *csum;
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
    if (!(csum = checksum(enc_pkt.data, enc_pkt.size)))
      {
	fprintf(stderr, "Error: Couldn't generate checksum\n");
	return -1;
      }
    sprintf(logbuf, "camera=%s frame=%7d checksum=%s\n",
	    shmpath, frmcnt, csum);
    free(csum);
    ret = fwrite(logbuf, strlen(logbuf), 1, fout);
    av_packet_unref(&enc_pkt);
  }
 end:
  ret = ((ret == AVERROR(EAGAIN)) ? 0 : -1);
  return ret;
}
