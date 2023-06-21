#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/display.h>

#define MAX_I_COUNT 3
#define ROW_COUNT 11
#define BASE_FPS 30
#define PRINT_TIME 0
#define PRINT_RESULT 1
int is_damaged = 0;
int64_t start = 0;
int64_t end = 0;
char message[1024];
int ret = 0;
uint8_t *buffer_data = NULL;
int buffer_size = 0;
char result_info[1024];

AVFormatContext *format_context_ = NULL;
AVIOContext *avio_context_ = NULL;
AVCodecContext *codec_context_ = NULL;

void allocate_array_size(int size) {
  buffer_data = NULL;
  buffer_size = size;
  buffer_data = (uint8_t*) malloc(size * sizeof(uint8_t));
}

void set_buffer_index(int index, int value) {
  buffer_data[index] = (uint8_t)(value & 0xFF);
}

int64_t get_current_time() {
  struct timeval time;
  gettimeofday(&time, NULL);
  return (int64_t)(time.tv_sec * 1000000.0 + time.tv_usec);
}

int return_error(char *msg, int ret) {
// #if PRINT_RESULT
//   printf("ret=%d, msg=%s\n", ret, msg);
// #endif
#if PRINT_TIME
  end = get_current_time();
  printf("time cost=%lld", (end - start) / 1000);
#endif
  return ret;
}

void custom_log_callback(void *ptr, int level, const char *fmt, va_list vl) {
  if (level != AV_LOG_ERROR) {
    return;
  }
  if (!is_damaged) {
// #if PRINT_RESULT
//     printf(fmt, vl);
// #endif
    sprintf(message, fmt, vl);
    is_damaged = 1;
  }
}

char *get_result_info() {
  return result_info;
}

int common_extract_video() {
  ret = avformat_find_stream_info(format_context_, NULL);
  if (ret < 0) {
    sprintf(message, "Error finding stream info: %s", av_err2str(ret));
    ret = -3;
    goto CLOSE;
  }
  int64_t bit_rate = format_context_->bit_rate;
  AVDictionaryEntry *entry = NULL;
  char *comment = NULL;
  while ((entry = av_dict_get(format_context_->metadata, "", entry, AV_DICT_IGNORE_SUFFIX))) {
    if (!entry) {
      continue;
    }
    if (strcmp(entry->key, "comment") == 0) {
      comment = entry->value;
    }
  }
  // Select the video stream
  int video_stream_index = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video_stream_index == AVERROR_STREAM_NOT_FOUND) {
    sprintf(message, "Error finding the video stream");
    ret = -4;
    goto CLOSE;
  }
  // Find the decoder for the video stream
  AVCodecParameters *codec_params = format_context_->streams[video_stream_index]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);
  if (!codec) {
    sprintf(message, "Unsupported codec found in input file");
    ret = -5;
    goto CLOSE;
  }
  codec_context_ = avcodec_alloc_context3(codec);
  if (!codec_context_) {
    sprintf(message, "Error allocating codec context");
    ret = -6;
    goto CLOSE;
  }
  ret = avcodec_parameters_to_context(codec_context_, codec_params);
  if (ret < 0) {
    sprintf(message, "Error setting codec context parameters: %s", av_err2str(ret));
    ret = -7;
    goto CLOSE;
  }
  ret = avcodec_open2(codec_context_, codec, NULL);
  if (ret < 0) {
    sprintf(message, "Error opening codec: %s", av_err2str(ret));
    ret = -8;
    goto CLOSE;
  }
  enum AVCodecID codec_id = codec_params->codec_id;
  if (codec_id != AV_CODEC_ID_H264 && codec_id != AV_CODEC_ID_HEVC) {
    goto CLOSE;
    sprintf(message, "Video codec type isn't h264 or h265");
    ret = -9;
    goto CLOSE;
  }
  AVStream *video_stream = format_context_->streams[video_stream_index];
  AVDictionaryEntry *m = NULL;
  int has_rotation = 0;
  double rotation = 0.0;
  m = av_dict_get(video_stream->metadata, "rotate", m, AV_DICT_MATCH_CASE);
  if (m) {
    rotation = atof(m->value);
    has_rotation = 1;
  }
  if (!has_rotation && video_stream->nb_side_data) {
    for (int i = 0; i < video_stream->nb_side_data; i++) {
      const AVPacketSideData *sd = &video_stream->side_data[i];
      if (sd->type == AV_PKT_DATA_DISPLAYMATRIX && sd->size >= 9*4) {
        double r = av_display_rotation_get((int32_t *)sd->data);
        if (!isnan(r)) {
          rotation = r;
        }
        break;
      }
    }
  }
  int dar_width = 0;
  int dar_height = 0;
  av_reduce(&dar_width, &dar_height, 
    codec_params->width * (int64_t) video_stream->sample_aspect_ratio.num,
    codec_params->height * (int64_t) video_stream->sample_aspect_ratio.den,
    1024 * 1024);
  int fps = av_q2d(video_stream->avg_frame_rate);
  int64_t duration = av_rescale_q(video_stream->duration, video_stream->time_base, AV_TIME_BASE_Q) / 1000;
  int64_t start_time = av_rescale_q(video_stream->start_time, video_stream->time_base, AV_TIME_BASE_Q) / 1000;
  if (bit_rate <= 0 && buffer_size > 0) {
    bit_rate = (int64_t) buffer_size * 1000 / duration * 8;
  }
  int width = codec_params->width;
  int height = codec_params->height;
  if (dar_width == 0 || dar_height == 0) {
    dar_width = width;
    dar_height = height;
  }
  float bit_rate_factor;
  if (fps < BASE_FPS) {
    bit_rate_factor = bit_rate * 1.0f / width / height;
  } else {
    bit_rate_factor = bit_rate * 1.0f / width / height / (fps / BASE_FPS);
  }
  if (codec_id == AV_CODEC_ID_HEVC) {
    bit_rate_factor = bit_rate_factor * 1.2;
  }
  enum AVColorSpace color_space = codec_params->color_space;
  enum AVColorPrimaries color_primary = codec_params->color_primaries;
  enum AVColorTransferCharacteristic color_trc = codec_params->color_trc;
  int is_hdr = 0;
  char hdr_info[20];
  if (color_space == AVCOL_SPC_BT2020_NCL || 
      color_space == AVCOL_SPC_BT2020_CL || 
      color_primary == AVCOL_PRI_BT2020 || 
      color_trc == AVCOL_TRC_BT2020_10 || 
      color_trc == AVCOL_TRC_BT2020_12) {
    is_hdr = 1;
  }
  if (is_hdr) {
    if (color_space == AVCOL_SPC_BT2020_NCL) {
      sprintf(hdr_info, "BT2020NCL");
    } else if (color_space == AVCOL_SPC_BT2020_CL) {
      sprintf(hdr_info, "BT2020CL");
    } else if (color_primary == AVCOL_PRI_BT2020) {
      sprintf(hdr_info, "BT2020");
    } else if (color_trc == AVCOL_TRC_BT2020_10) {
      sprintf(hdr_info, "BT2020_10");
    } else if (color_trc == AVCOL_TRC_BT2020_12) {
      sprintf(hdr_info, "BT2020_12");
    }
  }

  int packet_count = 0;
  int packet_cost_time = 0;
  int64_t last_i_time_pts = 0;
  while (1) {
    AVPacket packet;
    av_init_packet(&packet);
    packet.size = 0;
    ret = av_read_frame(format_context_, &packet);
    if (ret == 0) {
      if (packet.stream_index == video_stream_index) {
        if (packet.flags & AV_PKT_FLAG_KEY) {
          AVRational time_base = format_context_->streams[packet.stream_index]->time_base;
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
      sprintf(message, "Media packet has time exception");
      ret = -10;
      av_packet_unref(&packet);
      break;
    }
    if (packet.flags & AV_PKT_FLAG_CORRUPT) {
      sprintf(message, "Media packet has been corruptted");
      ret = -11;
      av_packet_unref(&packet);
      break;
    }
    if (packet.data == NULL || packet.size == 0) {
      sprintf(message, "Media packet has no data");
      ret = -12;
      av_packet_unref(&packet);
      break;
    }
    if (is_damaged) {
      ret = -13;
      av_packet_unref(&packet);
      if (avio_context_) {
        if (format_context_) {
          av_freep(format_context_->pb);
        }
        avio_context_free(&avio_context_);
        avio_context_ = NULL;
      }
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
      sprintf(result_info, "%s%d", message, 1);
#if PRINT_RESULT
      printf("%s\n", result_info);
#endif
#if PRINT_TIME
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
    sprintf(message, "Video cann't compute gop");
    ret = -14;
    goto CLOSE;
  }
  char codec_type[5];
  if (codec_id == AV_CODEC_ID_H264) {
    sprintf(codec_type, "H264");
  } else if (codec_id == AV_CODEC_ID_HEVC) {
    sprintf(codec_type, "H265");
  }
  double dar = dar_width * 1.0 / dar_height;
  int gop = (int) (packet_cost_time / packet_count);
  int twice_encode = 0;
  if (comment != NULL && strstr(comment, "vid:v") != NULL) {
    twice_encode = 1;
  }
  sprintf(result_info, "%s\n%d\n%d\n%f\n%f\n%lld\n%d\n%f\n%d\n%s\n%lld\n%lld\n%d\n%d\n%d", 
    codec_type, width, height, dar, rotation, bit_rate, fps, bit_rate_factor, 
    is_hdr, hdr_info, start_time, duration, gop, twice_encode, 0);
  goto CLOSE;
CLOSE:
  if (avio_context_) {
    if (format_context_) {
      av_freep(format_context_->pb);
    }
    avio_context_free(&avio_context_);
    avio_context_ = NULL;
  }
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
#if PRINT_TIME
  end = get_current_time();
  printf("time cost=%lld ms\n", (end - start) / 1000);
#endif
  if (ret != 0) {
    sprintf(result_info, "%s\n%d", message, 1);
#if PRINT_RESULT
    printf("%s\n", result_info);
#endif
  } else {
#if PRINT_RESULT
    printf("%s\n", result_info);
#endif
  }
  if (ret != 0) {
    return return_error(message, ret);
  }
  return ret;
}

int extract_video_data() {
  is_damaged = 0;
  ret = 0;
#if PRINT_TIME
  start = get_current_time();
#endif
  av_log_set_callback(custom_log_callback);
  format_context_ = avformat_alloc_context();
  avio_context_ = avio_alloc_context(buffer_data, buffer_size, 0, NULL, NULL, NULL, NULL);
  format_context_->pb = avio_context_;
  ret = avformat_open_input(&format_context_, NULL, NULL, NULL);
  if (ret < 0) {
    sprintf(message, "Error opening input file, ret=%s", av_err2str(ret));
    ret = -2;
    goto CLOSE;
  }
  return common_extract_video();

CLOSE:
  if (avio_context_) {
    if (format_context_) {
      av_freep(format_context_->pb);
    }
    avio_context_free(&avio_context_);
    avio_context_ = NULL;
  }
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
#if PRINT_TIME
  end = get_current_time();
  printf("time cost=%lld ms\n", (end - start) / 1000);
#endif
  if (ret != 0) {
    sprintf(result_info, "%s\n%d", message, 1);
#if PRINT_RESULT
    printf("%s\n", result_info);
#endif
  }
  if (ret != 0) {
    return return_error(message, ret);
  }
  return ret;
}

int extract_video_file(const char *path) {
  is_damaged = 0;
  ret = 0;
#if PRINT_TIME
  start = get_current_time();
#endif
  av_log_set_callback(custom_log_callback);
  format_context_ = avformat_alloc_context();
  ret = avformat_open_input(&format_context_, path, NULL, NULL);
  if (ret < 0) {
    sprintf(message, "Error opening input file: %s, ret=%s", path, av_err2str(ret));
    ret = -2;
    goto CLOSE;
  }
  return common_extract_video();

CLOSE:
  if (avio_context_) {
    if (format_context_) {
      av_freep(format_context_->pb);
    }
    avio_context_free(&avio_context_);
    avio_context_ = NULL;
  }
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
#if PRINT_TIME
  end = get_current_time();
  printf("time cost=%lld ms\n", (end - start) / 1000);
#endif
  if (ret != 0) {
    sprintf(result_info, "%s\n%d", message, 1);
#if PRINT_RESULT
    printf("%s\n", result_info);
#endif
  }
  if (ret != 0) {
    return return_error(message, ret);
  }
  return ret;
}

int test_data(const char *path) {
  FILE *fp;
  uint8_t* buffer;
  long file_size;

  fp = fopen(path, "rb");
  if (!fp) {
    return -1;
  }
  // 获取文件大小
  fseek(fp, 0, SEEK_END);
  file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  // 分配内存缓冲区
  buffer = (uint8_t *) malloc(file_size);
  if (!buffer) {
    printf("Memory allocation failed\n");
    fclose(fp);
    return -2;
  }

  // 读取文件内容到内存缓冲区中
  if (fread(buffer, file_size, 1, fp) != 1) {
    printf("Failed to read file\n");
    fclose(fp);
    free(buffer);
    return -3;
  }
  allocate_array_size((int) file_size);
  for (int i = 0; i < (int) file_size; i++) {
    set_buffer_index(i, (int)buffer[i]);
  }
  ret = extract_video_data();
  fclose(fp);
  return ret;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    return -1;
  }
  // return test_data(argv[1]);
  return extract_video_file(argv[1]);
}

