#!/usr/bin/python
# -*- coding: UTF-8 -*-


import media_extractor

if __name__ == "__main__":
    ret = media_extractor.parse_video_info('./files/long_gop.mp4')
    if ret is not None:
        print(ret)
