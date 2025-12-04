/**
 * @file mp3frame.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-09-27
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef __MP3FRAME_H__
#define __MP3FRAME_H__
#include "ebs.h"
#include <stdint.h>
#include <stdbool.h>
typedef struct mp3_info
{
    bool parsed;
    int stereo;
    int sampling_frequency;
    int mpeg25;
    int lsf;
    int lay;

    bool id3v2_parsed;
    uint32_t id3v2_size;
} mp3_info_t;

int lt_mp3_parse(const uint8_t *buf, int buf_len, mp3_info_t *inf);

#endif // __MP3FRAME_H__