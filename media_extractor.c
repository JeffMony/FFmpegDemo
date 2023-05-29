#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>

#define MAX_I_COUNT 3
#define ROW_COUNT 11
#define BASE_FPS 30
#define DEBUG 1
int is_damaged = 0;
int64_t start = 0;
int64_t end = 0;

int64_t get_current_time() {
  struct timeval time;
  gettimeofday(&time, NULL);
  return (int64_t)(time.tv_sec * 1000000.0 + time.tv_usec);
}

int return_error(char *msg, int ret) {
  printf("%s\n", msg);
#if DEBUG
  end = get_current_time();
  printf("time cost=%lld", (end - start) / 1000);
#endif
  return ret;
}

void custom_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
  if (level != AV_LOG_ERROR) {
    return;
  }
  printf(fmt, vl);
  is_damaged = 1;
}

int extract_video_info(const char *path) {
  #if DEBUG
  start = get_current_time();
#endif
  av_log_set_callback(custom_log_callback);
  char msg[1024];
  int ret = 0;
  AVFormatContext *format_ctx = NULL;
  printf("1\n");
  ret = avformat_open_input(&format_ctx, path, NULL, NULL);
  if (ret < 0) {
    sprintf(msg, "Error opening input file: %s, ret=%s", path, av_err2str(ret));
    ret = -2;
    goto CLOSE;
  }
  printf("2\n");
  ret = avformat_find_stream_info(format_ctx, NULL);
  if (ret < 0) {
    sprintf(msg, "Error finding stream info: %s", av_err2str(ret));
    ret = -3;
    goto CLOSE;
  }
  int64_t bit_rate = format_ctx->bit_rate;
  AVDictionaryEntry *entry = NULL;
  char *comment = NULL;
  while ((entry = av_dict_get(format_ctx->metadata, "", entry, AV_DICT_IGNORE_SUFFIX))) {
    if (strcmp(entry->key, "comment") == 0) {
      comment = entry->value;
    }
  }
  // Select the video stream
  int video_stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video_stream_index == AVERROR_STREAM_NOT_FOUND) {
    sprintf(msg, "Error finding the video stream");
    ret = -4;
    goto CLOSE;
  }
  // Find the decoder for the video stream
  AVCodecParameters *codec_params = format_ctx->streams[video_stream_index]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);
  if (!codec) {
    sprintf(msg, "Unsupported codec found in input file = %s", path);
    ret = -5;
    goto CLOSE;
  }
  AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx) {
    sprintf(msg, "Error allocating codec context");
    ret = -6;
    goto CLOSE;
  }
  ret = avcodec_parameters_to_context(codec_ctx, codec_params);
  if (ret < 0) {
    sprintf(msg, "Error setting codec context parameters: %s", av_err2str(ret));
    ret = -7;
    goto CLOSE;
  }
  ret = avcodec_open2(codec_ctx, codec, NULL);
  if (ret < 0) {
    sprintf(msg, "Error opening codec: %s", av_err2str(ret));
    ret = -8;
    goto CLOSE;
  }
  enum AVCodecID codec_id = codec_params->codec_id;
  if (codec_id != AV_CODEC_ID_H264 && codec_id != AV_CODEC_ID_HEVC) {
    goto CLOSE;
    sprintf(msg, "Video codec type isn't h264 or h265");
    ret = -9;
    goto CLOSE;
  }
  AVStream *video_stream = format_ctx->streams[video_stream_index];
  int fps = av_q2d(video_stream->avg_frame_rate);
  int64_t duration = av_rescale_q(video_stream->duration, video_stream->time_base, AV_TIME_BASE_Q) / 1000;
  int64_t start_time = av_rescale_q(video_stream->start_time, video_stream->time_base, AV_TIME_BASE_Q) / 1000;
  int width = codec_params->width;
  int height = codec_params->height;
  float bit_rate_factor;
  if (fps < BASE_FPS) {
    bit_rate_factor = bit_rate * 1.0f / width / height;
  } else {
    bit_rate_factor = bit_rate * 1.0f / width / height / (fps / BASE_FPS);
  }
  enum AVColorSpace color_space = codec_params->color_space;
  enum AVColorPrimaries color_primary = codec_params->color_primaries;
  enum AVColorTransferCharacteristic color_trc = codec_params->color_trc;
  int is_hdr = 0;
  if (color_space == AVCOL_SPC_BT2020_NCL || 
      color_space == AVCOL_SPC_BT2020_CL || 
      color_primary == AVCOL_PRI_BT2020 || 
      color_trc == AVCOL_TRC_BT2020_10 || 
      color_trc == AVCOL_TRC_BT2020_12) {
    is_hdr = 1;
  }

  int packet_count = 0;
  int packet_cost_time = 0;
  int64_t last_i_time_pts = 0;
  while (1) {
    AVPacket packet;
    av_init_packet(&packet);
    packet.size = 0;
    ret = av_read_frame(format_ctx, &packet);
    if (ret == 0) {
      if (packet.stream_index == video_stream_index) {
        if (packet.flags & AV_PKT_FLAG_KEY) {
          AVRational time_base = format_ctx->streams[packet.stream_index]->time_base;
          int64_t current_pts = av_rescale_q(packet.pts, time_base, AV_TIME_BASE_Q) / 1000;
          packet_cost_time += (current_pts - last_i_time_pts);
          last_i_time_pts = current_pts;
          packet_count++;
        }
      }
    } else if (ret == AVERROR_EOF) {
      av_packet_unref(&packet);
      ret = 0;
      break;
    }
    if (packet.pts == AV_NOPTS_VALUE || packet.dts == AV_NOPTS_VALUE) {
      sprintf(msg, "Media packet has time exception");
      ret = -10;
      av_packet_unref(&packet);
      break;
    }
    if (packet.flags & AV_PKT_FLAG_CORRUPT) {
      sprintf(msg, "Media packet has been corruptted");
      ret = -11;
      av_packet_unref(&packet);
      break;
    }
    if (packet.data == NULL || packet.size == 0) {
      sprintf(msg, "Media packet has no data");
      ret = -12;
      av_packet_unref(&packet);
      break;
    }
    if (is_damaged) {
      ret = -13;
      av_packet_unref(&packet);
      if (codec_ctx) {
        avcodec_close(codec_ctx);
        avcodec_free_context(&codec_ctx);
      }
      if (format_ctx) {
        avformat_close_input(&format_ctx);
      }
#if DEBUG
      end = get_current_time();
      printf("time cost=%lld ms \n", (end - start) / 1000);
#endif
      return ret;
    }
  }
  if (ret != 0) {
    goto CLOSE;
  }
  if (packet_count == 0 || packet_cost_time == 0) {
    sprintf(msg, "Video cann't compute gop");
    ret = -14;
    goto CLOSE;
  }
  printf("%d\n", ROW_COUNT);
  if (codec_id == AV_CODEC_ID_H264) {
    printf("H264\n");
  } else if (codec_id == AV_CODEC_ID_HEVC) {
    printf("H265\n");
  }
  printf("%d\n", width);
  printf("%d\n", height);
  printf("%lld\n", bit_rate);
  printf("%d\n", fps);
  printf("%f\n",  bit_rate_factor);
  printf("%d\n", is_hdr);
  printf("%lld\n", start_time);
  printf("%lld\n", duration);
  printf("%d\n", packet_cost_time / packet_count);
  if (comment != NULL) {
    printf("%s\n", comment);
  } else {
    printf("invalid\n");
  }
  goto CLOSE;

CLOSE:
  if (codec_ctx) {
    avcodec_close(codec_ctx);
    avcodec_free_context(&codec_ctx);
  }
  if (format_ctx) {
    avformat_close_input(&format_ctx);
  }
#if DEBUG
  end = get_current_time();
  printf("time cost=%lld ms\n", (end - start) / 1000);
#endif
  if (ret != 0) {
    return return_error(msg, ret);
  }
  return ret;
}

int main(int argc, char **argv) {
 if (argc < 2) {
    printf("Please provide a video file path as input");
    return -1;
  }
  return extract_video_info(argv[1]);
}

