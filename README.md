#### ffmpeg 编译
1.下载FFmpeg源码<br>
2.编译FFmpeg源码<br>
配置参数
```
./configure --prefix=/Users/jeffmony/tools/ffmpeg_src --enable-shared --enable-pthreads --enable-version3 --enable-avresample --cc=clang --host-cflags= --host-ldflags= --enable-ffplay --enable-gnutls --enable-gpl --enable-libaom --enable-libbluray --enable-libmp3lame --enable-libopus --enable-librubberband --enable-libsnappy --enable-libtesseract --enable-libtheora --enable-libvidstab --enable-libvorbis --enable-libvpx --enable-libwebp --enable-libx264 --enable-libx265 --enable-libxvid --enable-lzma --enable-libfontconfig --enable-libfreetype --enable-frei0r --enable-libass --enable-libopencore-amrnb --enable-libopencore-amrwb --enable-libopenjpeg --enable-libspeex --enable-libsoxr --enable-videotoolbox --disable-libjack --disable-indev=jack --disable-x86asm 
```

编译源码
```
make
```

链接编译好的库
```
make install
```

这儿不是交叉编译，只是编译本地可用的FFmpeg库。<br>

3.查看编译好的文件<br>
有4个文件，其中bin下面是本地可执行的库，include下面是各个模块的头文件，lib下面是动态库和静态库，share下面是FFmpeg中的demo代码。
```
jeffmony@JeffMonydeMacBook-Pro ffmpeg_src % ls
bin include lib share
```

#### FFmpeg 运行
例如ffmpeg_log.c文件
```
#include <stdio.h>
#include <libavutil/log.h>

int main(int argc, char *argv[])
{
    av_log_set_level(AV_LOG_DEBUG);

    av_log(NULL, AV_LOG_DEBUG, "hello world!\n");

    return 0;
}
```

执行的脚本是：
```
gcc -g -o ffmpeg_log ffmpeg_log.c -I/Users/jeffmony/tools/ffmpeg_src/include -L/Users/jeffmony/tools/ffmpeg_src/lib -lavutil
```
最终会生成一个可执行文件。

#### FFmpeg 拓展
##### 1.查看视频文件基本信息
具体见mediainfo.c
```
gcc -g -o mediainfo mediainfo.c -I/Users/jeffmony/tools/ffmpeg_src/include -L/Users/jeffmony/tools/ffmpeg_src/lib -lavformat
```
运行的结果如下:
```
jeffmony@JeffMonydeMacBook-Pro ffmpeg_demo % ./mediainfo jeffmony.mp4 
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'jeffmony.mp4':
  Metadata:
    major_brand     : isom
    minor_version   : 512
    compatible_brands: isomiso2avc1mp41
    encoder         : Lavf58.20.100
  Duration: 00:02:41.83, start: 0.000000, bitrate: 670 kb/s
    Stream #0:0(und): Video: h264 (High) (avc1 / 0x31637661), yuv420p, 960x540, 633 kb/s, 25 fps, 25 tbr, 90k tbn, 50 tbc (default)
    Metadata:
      handler_name    : VideoHandler
    Stream #0:1(und): Audio: aac (HE-AACv2) (mp4a / 0x6134706D), 44100 Hz, stereo, fltp, 32 kb/s (default)
    Metadata:
      handler_name    : SoundHandler
```
##### 2.从视频中提取音频流
具体见extractor_audio.c
```
gcc -g -o extractor_audio extractor_audio.c `pkg-config --libs --cflags libavutil libavformat libavcodec`
```

```
./extractor_audio jeffmony.mp4 output.aac
```
将jeffmony.mp4中的音频流取出来，保存为output.aac



<br><br>欢迎关注我的公众号JeffMony，我会持续为你带来音视频---算法---Android---python 方面的知识分享<br><br>
![](./JeffMony.jpg)
