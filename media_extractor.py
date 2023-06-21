#!/usr/bin/python
# -*- coding: UTF-8 -*-

import os
import subprocess
from subprocess import PIPE

def parse_video_info(video_url) : 
    path = os.path.dirname(__file__)
    command = f'{path}/media_extractor {video_url}'
    pipe = subprocess.run(command, shell=True, encoding='utf-8', stdout=PIPE)
    ret = pipe.returncode
    lines = pipe.stdout.splitlines()
    resultDict = {}
    if ret == 0 :
        codecType = lines[0]
        width = int(lines[1])
        height = int(lines[2])
        displayAspectRatio = float(lines[3])
        rotation = float(lines[4])
        bitRate = int(lines[5])
        frameRate = int(lines[6])
        bitRateFactor = float(lines[7])
        isHdr = int(lines[8])
        hdrInfo = lines[9]
        startTime = int(lines[10])
        duration = int(lines[11])
        gop = int(lines[12])
        twiceEncode = int(lines[13])
        isDamaged = int(lines[14])
        resultDict['codecType'] = codecType
        resultDict['width'] = width
        resultDict['height'] = height
        resultDict['displayAspectRatio'] = displayAspectRatio
        resultDict['rotation'] = rotation
        resultDict['bitRate'] = bitRate
        resultDict['frameRate'] = frameRate
        resultDict['bitRateFactor'] = bitRateFactor
        resultDict['isHdr'] = isHdr
        if isHdr == 1 :
            resultDict['hdrInfo'] = hdrInfo
        resultDict['startTime'] = startTime
        resultDict['duration'] = duration
        resultDict['gop'] = gop
        resultDict['twiceEncode'] = twiceEncode
        resultDict['isDamaged'] = isDamaged
    else :
        errMsg = lines[0]
        isDamaged = int(lines[1])
        resultDict['errMsg'] = errMsg
        resultDict['isDamaged'] = isDamaged
    return resultDict


def extract_video_info(video_url) : 
    path = os.path.dirname(__file__)
    command = f'{path}/extract_video_info {video_url}'
    pipe = subprocess.run(command, shell=True, encoding='utf-8', stdout=PIPE)
    ret = pipe.returncode
    if ret == 0 :
        lines = pipe.stdout.splitlines()
        videoType = lines[0]
        wh = lines[1]
        width = wh.split(' ')[0]
        height = wh.split(' ')[1]
        resultDict = {}
        resultDict['videoType'] = videoType
        resultDict['width'] = int(width)
        resultDict['height'] = int(height)
        if videoType == 'H264' :
            extradataStr = lines[2]
            resultDict['extraData'] = extradataStr
        elif videoType == 'H265' :
            extradataStr = lines[2]
            resultDict['extraData'] = extradataStr
        return resultDict
    error_string = "error_code : " + str(ret)
    print(error_string)
    return None
