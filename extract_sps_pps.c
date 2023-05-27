#include <stdio.h>
#include <stdarg.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "base64/b64.h"

int convert_sps_pps(uint8_t *p_buf, int i_buf_size,
  uint8_t *out_sps_buf, int *out_sps_buf_size,
  uint8_t *out_pps_buf, int *out_pps_buf_size,
  int *p_nal_size) {
  int i_data_size = i_buf_size, i_nal_size;
  unsigned int i_loop_end;
  if (i_data_size < 7) {
    printf("Input Metadata too small\n");
    return -1;
  }
  if (p_nal_size) {
    *p_nal_size = (size_t) ((p_buf[4] & 0x03) + 1);
  }
  p_buf += 5;
  i_data_size -= 5;

  for (int j = 0; j < 2; j++) {
    if (i_data_size < 1) {
      printf("PPS too small after processing SPS/PPS %d\n", i_data_size);
      return -2;
    }
    i_loop_end = (unsigned int) (p_buf[0] & (j == 0 ? 0x1f : 0xff));
    p_buf++;
    i_data_size--;

    for (unsigned int i = 0; i < i_loop_end; i++) {
      if (i_data_size < 2) {
        printf("SPS is too small %d", i_data_size);
        return -3;
      }
      i_nal_size = (p_buf[0] << 8) | p_buf[1];
      p_buf += 2;
      i_data_size -= 2;

      if (i_data_size < i_nal_size) {
        printf("SPS size does not match NAL specified size %d\n", i_data_size);
        return -4;
      }
      if (j == 0) {
        out_sps_buf[0] = 0;
        out_sps_buf[1] = 0;
        out_sps_buf[2] = 0;
        out_sps_buf[3] = 1;
        memcpy(out_sps_buf + 4, p_buf, i_nal_size);
        *out_sps_buf_size = i_nal_size + 4;
      } else {
        out_pps_buf[0] = 0;
        out_pps_buf[1] = 0;
        out_pps_buf[2] = 0;
        out_pps_buf[3] = 1;
        memcpy(out_pps_buf + 4, p_buf, i_nal_size);
        *out_pps_buf_size = i_nal_size + 4;
      }

      p_buf += i_nal_size;
      i_data_size -= i_nal_size;
    }
  }
  return 0;
}

int convert_hevc_nal(uint8_t *extra_data, int extra_data_size,
  uint8_t *convert_buffer, int convert_buffer_size,
  int *sps_pps_size, int *nal_size) {
  int i, num_arrays;
  uint8_t *p_end = extra_data + extra_data_size;
  int i_sps_pps_size = 0;

  if (extra_data_size <= 3 || (!extra_data[0] && !extra_data[1] && extra_data[2] <= 1)) {
    return -1;
  }
  if (p_end - extra_data < 23) {
    printf("Input Metadata too small\n");
    return -2;
  }
  extra_data += 21;
  if (nal_size) {
    *nal_size = (size_t) ((*extra_data & 0x03) + 1);
  }
  extra_data++;
  num_arrays = *extra_data++;
  for (i = 0; i < num_arrays; i++) {
    int type, cnt, j;

    if (p_end - extra_data < 3) {
      printf("Input Metadata too small\n");
      return -3;
    }
    type = *(extra_data++) & 0x3f;
    (void) (type);

    cnt = extra_data[0] << 8 | extra_data[1];
    extra_data += 2;

    for (j = 0; j < cnt; j++) {
      int i_nal_size;

      if (p_end - extra_data < 2) {
        printf("Input Metadata too small\n");
        return -4;
      }

      i_nal_size = extra_data[0] << 8 | extra_data[1];
      extra_data += 2;

      if (i_nal_size < 0 || p_end - extra_data < i_nal_size) {
        printf("NAL unit size does not match Input Metadata size\n");
        return -5;
      }

      if (i_sps_pps_size + 4 + i_nal_size > convert_buffer_size) {
        printf("Output buffer too small");
        return -6;
      }

      convert_buffer[i_sps_pps_size++] = 0;
      convert_buffer[i_sps_pps_size++] = 0;
      convert_buffer[i_sps_pps_size++] = 0;
      convert_buffer[i_sps_pps_size++] = 1;

      memcpy(convert_buffer + i_sps_pps_size, extra_data, (size_t) i_nal_size);
      extra_data += i_nal_size;

      i_sps_pps_size += i_nal_size;
    }
  }
  *sps_pps_size = i_sps_pps_size;
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Please provide a video file path as input\n");
    return -1;
  }

  AVFormatContext *format_ctx = NULL;
  int ret = avformat_open_input(&format_ctx, argv[1], NULL, NULL);
  if (ret < 0) {
    printf("Error opening input file: %s, ret=%s\n", argv[1], av_err2str(ret));
    return -2;
  }

  ret = avformat_find_stream_info(format_ctx, NULL);
  if (ret < 0) {
    printf("Error finding stream info: %s\n", av_err2str(ret));
    avformat_close_input(&format_ctx);
    return -3;
  }

  // Select the video stream
  int video_stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video_stream_index < 0) {
    printf("Error finding the video stream\n");
    return -4;
  }

  // Find the decoder for the video stream
  AVCodecParameters *codec_params = format_ctx->streams[video_stream_index]->codecpar;
  const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);

  if (!codec || video_stream_index < 0) {
    printf("Unsupported codec found in input file = %s\n", argv[1]);
    avformat_close_input(&format_ctx);
    return -5;
  }

  AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
  if (!codec_ctx) {
    printf("Error allocating codec context\n");
    avformat_close_input(&format_ctx);
    return -6;
  }

  ret = avcodec_parameters_to_context(codec_ctx, codec_params);
  if (ret < 0) {
    printf("Error setting codec context parameters: %s\n", av_err2str(ret));
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
    return -7;
  }

  ret = avcodec_open2(codec_ctx, codec, NULL);
  if (ret < 0) {
    printf("Error opening codec: %s\n", av_err2str(ret));
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
    return -8;
  }
  int width = codec_params->width;
  int height = codec_params->height;
  uint8_t *extradata = codec_params->extradata;
  int extradata_size = codec_params->extradata_size;
  uint8_t *buffer_data = NULL, *sps_data = NULL, *pps_data = NULL;
  int buffer_size = 0, sps_size = 0, pps_size = 0;
  int nal_size = 0;
  int type = -1;
  if (extradata && extradata_size > 0) {
    uint8_t *data = extradata;
    int size = extradata_size;
    if (codec_params->codec_id == AV_CODEC_ID_HEVC) {
      type = 5;
      // HEVC格式的extradata，参考ITU-T Rec. H.265（02/2018）
      if (data[0] == 1 || data[1] == 1) {
        buffer_data = (uint8_t *) malloc(size + 20);
        int ret = convert_hevc_nal(data, size, buffer_data, size + 20, &buffer_size, &nal_size);
      }
    } else if (codec_params->codec_id == AV_CODEC_ID_H264) {
      // H.264格式的extradata
      type = 4;
      if (data[0] == 1) {
        sps_data = (uint8_t *) malloc((size_t) size + 20);
        pps_data = (uint8_t *) malloc((size_t) size + 20);
        int ret = convert_sps_pps(data, size, sps_data, &sps_size, pps_data, &pps_size, &nal_size);
      }
    } else {
      goto CLOSE;
      return -9;
    }
  }

  if (type == 4) {
    if (sps_data && sps_size > 0) {
      printf("H264\n");
      printf("%d\n", width);
      printf("%d\n", height);
      char *sps = b64_encode(sps_data, sps_size);
      printf("%s\n", sps);
      free(sps_data);
    }
    if (pps_data && pps_size > 0) {
      char *pps = b64_encode(pps_data, pps_size);
      printf("%s\n", pps);
      free(pps_data);
    }
  } else if (type == 5) {
    if (buffer_data && buffer_size > 0) {
      printf("H265\n");
      printf("%d\n", width);
      printf("%d\n", height);
      char *hevc = b64_encode(buffer_data, buffer_size);
      printf("%s\n", hevc);
      free(buffer_data);
    }
  } else {
    if (sps_data) {
      free(sps_data);
    }
    if (pps_data) {
      free(pps_data);
    }
    if (buffer_data) {
      free(buffer_data);
    }
  }

  goto CLOSE;

CLOSE:
  avcodec_close(codec_ctx);
  avcodec_free_context(&codec_ctx);
  avformat_close_input(&format_ctx);

  return 0;
}
