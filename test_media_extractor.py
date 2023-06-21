#!/usr/bin/python
# -*- coding: UTF-8 -*-


import media_extractor

if __name__ == "__main__":
    ret = media_extractor.parse_video_info('http://sns-video-bd.xhscdn.com/stream/110/258/01e4326d8b792647010370038764ff7d3c_258.mp4')
    if ret is not None:
        print(ret)
