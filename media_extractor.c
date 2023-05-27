#include <stdio.h>
#include <stdarg.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>

#define MAX_I_COUNT 3
#define ROW_COUNT 11

int return_error(char *msg, int ret) {
  printf("%s\n", msg);
  return ret;
}

void custom_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
  if (level != AV_LOG_ERROR) {
    return;
  }
  printf(fmt, vl);
}

int main(int argc, char **argv) {
  av_log_set_callback(custom_log_callback);
  char msg[1024];
  int ret = 0;
  if (argc < 2) {
    sprintf(msg, "Please provide a video file path as input");
    ret = -1;
  }
  if (ret != 0) {
    return return_error(msg, ret);
  }
  AVFormatContext *format_ctx = NULL;
  ret = avformat_open_input(&format_ctx, argv[1], NULL, NULL);
  if (ret < 0) {
    sprintf(msg, "Error opening input file: %s, ret=%s", argv[1], av_err2str(ret));
    ret = -2;
  } else {
    ret = 0;
  }
  if (ret != 0) {
    return return_error(msg, ret);
  }
  ret = avformat_find_stream_info(format_ctx, NULL);
  if (ret < 0) {
    sprintf(msg, "Error finding stream info: %s", av_err2str(ret));
    ret = -3;
    avformat_close_input(&format_ctx);
  } else {
    ret = 0;
  }
  if (ret != 0) {
    return return_error(msg, ret);
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
    avformat_close_input(&format_ctx);
  }
  if (ret != 0) {
    return return_error(msg, ret);
  }
  // Find the decoder for the video stream
  AVCodecParameters *codec_params = format_ctx->streams[video_stream_index]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);
  if (!codec) {
    sprintf(msg, "Unsupported codec found in input file = %s", argv[1]);
    ret = -5;
    avformat_close_input(&format_ctx);
  }
  if (ret != 0) {
    return return_error(msg, ret);
  }
  AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx) {
    sprintf(msg, "Error allocating codec context");
    ret = -6;
    avformat_close_input(&format_ctx);
  }
  if (ret != 0) {
    return return_error(msg, ret);
  }
  ret = avcodec_parameters_to_context(codec_ctx, codec_params);
  if (ret < 0) {
    sprintf(msg, "Error setting codec context parameters: %s", av_err2str(ret));
    ret = -7;
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
  } else {
    ret = 0;
  }
  if (ret != 0) {
    return return_error(msg, ret);
  }
  ret = avcodec_open2(codec_ctx, codec, NULL);
  if (ret < 0) {
    sprintf(msg, "Error opening codec: %s", av_err2str(ret));
    ret = -8;
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
  } else {
    ret = 0;
  }
  if (ret != 0) {
    return return_error(msg, ret);
  }
  enum AVCodecID codec_id = codec_params->codec_id;
  if (codec_id != AV_CODEC_ID_H264 && codec_id != AV_CODEC_ID_HEVC) {
    goto CLOSE;
    sprintf(msg, "Video codec type isn't h264 or h265");
    ret = -9;
  }
  if (ret != 0) {
    return return_error(msg, ret);
  }
  AVStream *video_stream = format_ctx->streams[video_stream_index];
  int fps = av_q2d(video_stream->avg_frame_rate);
  int64_t duration = av_rescale_q(video_stream->duration, video_stream->time_base, AV_TIME_BASE_Q) / 1000;
  int64_t start_time = av_rescale_q(video_stream->start_time, video_stream->time_base, AV_TIME_BASE_Q) / 1000;
  int width = codec_params->width;
  int height = codec_params->height;
  float bit_rate_num = bit_rate * 1.0f / width / height;
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
      goto CLOSE;
      av_packet_unref(&packet);
      break;
    }
    if (packet.flags & AV_PKT_FLAG_CORRUPT) {
      sprintf(msg, "Media packet has been corruptted");
      ret = -11;
      goto CLOSE;
      break;
    }
    if (packet.data == NULL || packet.size == 0) {
      sprintf(msg, "Media packet has no data");
      ret = -12;
      goto CLOSE;
      break;
    }
  }
  if (ret != 0) {
    return return_error(msg, ret);
  }
  if (packet_count == 0 || packet_cost_time == 0) {
    sprintf(msg, "Video cann't compute gop");
    ret = -13;
    goto CLOSE;
  }
  if (ret != 0) {
    return return_error(msg, ret);
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
  printf("%f\n",  bit_rate_num);
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
  avcodec_close(codec_ctx);
  avcodec_free_context(&codec_ctx);
  avformat_close_input(&format_ctx);

  return ret;
}

