#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>

AVFormatContext *format_context_ = NULL;
AVCodecContext *codec_context_ = NULL;
int ret = 0;
int packet_count = 0;
int v_packet_count = 0;
int i_v_packet_count = 0;
int non_ref_packet_count = 0;

void custom_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
  if (level != AV_LOG_ERROR) {
    return;
  }
  printf(fmt, vl);
}

int packet_parse(const char *path) {
  av_log_set_callback(custom_log_callback);
  format_context_ = avformat_alloc_context();
  ret = 0;
  packet_count = 0;
  v_packet_count = 0;
  i_v_packet_count = 0;
  ret = avformat_open_input(&format_context_, path, NULL, NULL);
  if (ret < 0) {
    ret = -1;
    goto CLOSE;
  }
  ret = avformat_find_stream_info(format_context_, NULL);
  if (ret < 0) {
    ret = -2;
    goto CLOSE;
  }
  int video_stream_index = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video_stream_index == AVERROR_STREAM_NOT_FOUND) {
    ret = -3;
    goto CLOSE;
  }
  int audio_stream_index = av_find_best_stream(format_context_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if (audio_stream_index == AVERROR_STREAM_NOT_FOUND) {
    ret = -4;
    goto CLOSE;
  }
  AVCodecParameters *codec_params = format_context_->streams[video_stream_index]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);
  if (!codec) {
    ret = -5;
    goto CLOSE;
  }
  codec_context_ = avcodec_alloc_context3(codec);
  if (!codec_context_) {
    ret = -6;
    goto CLOSE;
  }
  ret = avcodec_open2(codec_context_, codec, NULL);
  if (ret < 0) {
    ret = -7;
    goto CLOSE;
  }
  AVStream *video_stream = format_context_->streams[video_stream_index];
  int64_t duration = av_rescale_q(video_stream->duration, video_stream->time_base, AV_TIME_BASE_Q) / 1000;
  while (1) {
    AVPacket packet;
    av_init_packet(&packet);
    packet.size = 0;
    ret = av_read_frame(format_context_, &packet);
    if (ret == 0) {
      if (packet.stream_index == video_stream_index) {
        v_packet_count++;
        if (packet.flags & AV_PKT_FLAG_KEY) {
          i_v_packet_count++;
          uint8_t nal_unit_type = packet.data[4] & 0x1f;
          uint8_t nal_ref_idc = (packet.data[4] >> 5) & 0x03;
          // printf("# %d %d\n", nal_unit_type, nal_ref_idc);
          // printf("- %hx %hx %hx %hx\n", packet.data[4], packet.data[5], packet.data[6], packet.data[7]);
        } else {
          uint8_t nal_unit_type = packet.data[4] & 0x1f;
          uint8_t nal_ref_idc = (packet.data[4] >> 5) & 0x03;
          if (nal_ref_idc == 0) {
            non_ref_packet_count++;
          }
          printf("@ %d %d\n", nal_unit_type, nal_ref_idc);
          // printf("# %hx %hx %hx %hx\n", packet.data[4], packet.data[5], packet.data[6], packet.data[7]);
        }
      }
      packet_count++;
    } else if (ret == AVERROR_EOF) {
      av_packet_unref(&packet);
      ret = 0;
      break;
    }
    if (packet.pts == AV_NOPTS_VALUE || packet.dts == AV_NOPTS_VALUE) {
      ret = -8;
      av_packet_unref(&packet);
      break;
    }
    if (packet.flags & AV_PKT_FLAG_CORRUPT) {
      ret = -9;
      av_packet_unref(&packet);
      break;
    }
    if (packet.data == NULL || packet.size == 0) {
      ret = -10;
      av_packet_unref(&packet);
      break;
    }
  }

  if (ret == 0) {
    printf("duration=%lld\n", duration);
    printf("non_ref_packet_count=%d\n", non_ref_packet_count);
    printf("packet_count=%d\nv_packet_count=%d\ni_v_packet_count=%d\n", packet_count, v_packet_count, i_v_packet_count);
  }

  goto CLOSE;

CLOSE:
  if (codec_context_) {
    avcodec_close(codec_context_);
    avcodec_free_context(&codec_context_);
    codec_context_ = NULL;
  }
  if (format_context_) {
    avformat_close_input(&format_context_);
    avformat_free_context(format_context_);
    format_context_ = NULL;
  }

  return ret;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    return -1;
  }
  int ret = packet_parse(argv[1]);
  return ret;
}