/**
 * @file mp3frame.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-09-27
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include "mp3frame.h"
#include "ql_audio.h"


typedef struct mp3_header
{
    uint32_t sync1 : 8;

    uint32_t crc : 1;
    uint32_t layer : 2;
    uint32_t version : 2;
    uint32_t sync2 : 3;

    uint32_t private_bit : 1;
    uint32_t padding : 1;
    uint32_t sample_rate : 2;
    uint32_t bit_rate : 4;

    uint32_t emphasis : 2;
    uint32_t original : 1;
    uint32_t copyright : 1;
    uint32_t mode_extension : 2;
    uint32_t channel2mode : 2;
} mp3_header_t;

static const uint8_t sample_num[3][3] = {
    {48, 144, 144},
    {48, 144, 72},
    {48, 144, 72},
};

static const uint16_t sample_rate[3][3] = {
    {44100, 48000, 32000},
    {22050, 24000, 16000},
    {11025, 12000, 8000},
};

static const uint16_t bits_rate[3][3][14] = {
    [0][0][0] = 32,
    [0][0][1] = 64,
    [0][0][2] = 96,
    [0][0][3] = 128,
    [0][0][4] = 160,
    [0][0][5] = 192,
    [0][0][6] = 224,
    [0][0][7] = 256,
    [0][0][8] = 288,
    [0][0][9] = 320,
    [0][0][10] = 352,
    [0][0][11] = 384,
    [0][0][12] = 416,
    [0][0][13] = 448,
    [0][1][0] = 32,
    [0][1][1] = 48,
    [0][1][2] = 56,
    [0][1][3] = 64,
    [0][1][4] = 80,
    [0][1][5] = 96,
    [0][1][6] = 112,
    [0][1][7] = 128,
    [0][1][8] = 160,
    [0][1][9] = 192,
    [0][1][10] = 224,
    [0][1][11] = 256,
    [0][1][12] = 320,
    [0][1][13] = 384,
    [0][2][0] = 32,
    [0][2][1] = 40,
    [0][2][2] = 48,
    [0][2][3] = 56,
    [0][2][4] = 64,
    [0][2][5] = 80,
    [0][2][6] = 96,
    [0][2][7] = 112,
    [0][2][8] = 128,
    [0][2][9] = 160,
    [0][2][10] = 192,
    [0][2][11] = 224,
    [0][2][12] = 256,
    [0][2][13] = 320,
    [1][0][0] = 32,
    [1][0][1] = 48,
    [1][0][2] = 56,
    [1][0][3] = 64,
    [1][0][4] = 80,
    [1][0][5] = 96,
    [1][0][6] = 112,
    [1][0][7] = 128,
    [1][0][8] = 144,
    [1][0][9] = 160,
    [1][0][10] = 176,
    [1][0][11] = 192,
    [1][0][12] = 224,
    [1][0][13] = 256,
    [1][1][0] = 8,
    [1][1][1] = 16,
    [1][1][2] = 24,
    [1][1][3] = 32,
    [1][1][4] = 40,
    [1][1][5] = 48,
    [1][1][6] = 56,
    [1][1][7] = 64,
    [1][1][8] = 80,
    [1][1][9] = 96,
    [1][1][10] = 112,
    [1][1][11] = 128,
    [1][1][12] = 144,
    [1][1][13] = 160,
    [1][2][0] = 8,
    [1][2][1] = 16,
    [1][2][2] = 24,
    [1][2][3] = 32,
    [1][2][4] = 40,
    [1][2][5] = 48,
    [1][2][6] = 56,
    [1][2][7] = 64,
    [1][2][8] = 80,
    [1][2][9] = 96,
    [1][2][10] = 112,
    [1][2][11] = 128,
    [1][2][12] = 144,
    [1][2][13] = 160,

    [2][0][0] = 32,
    [2][0][1] = 48,
    [2][0][2] = 56,
    [2][0][3] = 64,
    [2][0][4] = 80,
    [2][0][5] = 96,
    [2][0][6] = 112,
    [2][0][7] = 128,
    [2][0][8] = 144,
    [2][0][9] = 160,
    [2][0][10] = 176,
    [2][0][11] = 192,
    [2][0][12] = 224,
    [2][0][13] = 256,
    [2][1][0] = 8,
    [2][1][1] = 16,
    [2][1][2] = 24,
    [2][1][3] = 32,
    [2][1][4] = 40,
    [2][1][5] = 48,
    [2][1][6] = 56,
    [2][1][7] = 64,
    [2][1][8] = 80,
    [2][1][9] = 96,
    [2][1][10] = 112,
    [2][1][11] = 128,
    [2][1][12] = 144,
    [2][1][13] = 160,
    [2][2][0] = 8,
    [2][2][1] = 16,
    [2][2][2] = 24,
    [2][2][3] = 32,
    [2][2][4] = 40,
    [2][2][5] = 48,
    [2][2][6] = 56,
    [2][2][7] = 64,
    [2][2][8] = 80,
    [2][2][9] = 96,
    [2][2][10] = 112,
    [2][2][11] = 128,
    [2][2][12] = 144,
    [2][2][13] = 160,
};

static bool check_header(uint32_t header)
{
    /* header */
    if ((header & 0xffe00000) != 0xffe00000)
        return false;
    /* version check */
    if ((header & (3 << 19)) == 1 << 19)
        return false;
    /* layer check */
    if ((header & (3 << 17)) == 0)
        return false;
    /* bit rate */
    if ((header & (0xf << 12)) == 0xf << 12)
        return false;
    /* frequency */
    if ((header & (3 << 10)) == 3 << 10)
        return false;
    return true;
}

static int sync_buffer(mp3_info_t *inf, uint8_t *buf, int len, bool free_match)
{
    uint8_t b[4] = {0};
    int h = 0;
    uint8_t *p = buf;
    long length = len;
    int i;

    for (i = 0; i < length; i++)
    {
        b[0] = b[1];
        b[1] = b[2];
        b[2] = b[3];
        b[3] = *p++;
        if (i >= 3)
        {
            uint32_t head;
            head = b[0];
            head <<= 8;
            head |= b[1];
            head <<= 8;
            head |= b[2];
            head <<= 8;
            head |= b[3];
            h = check_header(head);
            if (h)
            {
                mp3_header_t *f = (mp3_header_t *)b;
                int mode, stereo, sampling_frequency, mpeg25, lsf; //, samprate;
                if (head & (1 << 20))
                {
                    lsf = (head & (1 << 19)) ? 0x0 : 0x1;
                    mpeg25 = 0;
                }
                else
                {
                    lsf = 1;
                    mpeg25 = 1;
                }

                mode = ((head >> 6) & 0x3);
                stereo = (mode == 3) ? 1 : 2;
                if (mpeg25)
                    sampling_frequency = 6 + ((head >> 10) & 0x3);
                else
                    sampling_frequency = ((head >> 10) & 0x3) + (lsf * 3);
                if (free_match)
                {
                    h = ((stereo == inf->stereo) && (lsf == inf->lsf) && (mpeg25 == inf->mpeg25) && (sampling_frequency == inf->sampling_frequency));
                    if (!h)
                    {
                        inf->stereo = stereo;
                        inf->lsf = lsf;
                        inf->mpeg25 = mpeg25;
                        inf->sampling_frequency = sampling_frequency;
                        inf->lay = 4 - f->layer;
                        inf->parsed = false;
                        // ql_aud_data_done(); // 告诉内核数据已写完,所有数据解码完成后停止播放,这一步必须要有
                        // ql_aud_wait_play_finish(QL_WAIT_FOREVER);
                        ql_aud_player_stop();
                        h = 1;
                    }
                }
                else
                {
                    inf->stereo = stereo;
                    inf->lsf = lsf;
                    inf->mpeg25 = mpeg25;
                    inf->sampling_frequency = sampling_frequency;
                    inf->lay = 4 - f->layer;
                }
            }
            if (h)
            {
                return i - 3;
            }
        }
    }
    return -1;
}

int lt_mp3_parse(const uint8_t *buf, int buf_len, mp3_info_t *inf)
{
    int err = 0;
    int offset = 0;
    int frame_len = 0;
    uint8_t *p = (uint8_t *)buf;
    int len = buf_len;
    // _frame_log("inf->parsed=%d", inf->parsed);
    while (len > frame_len)
    {
        if ((offset = sync_buffer(inf, p, len, inf->parsed)) < 0)
        {
            return -1;
        }

        if (offset)
        {
            if (buf_len > offset)
            {
                p += offset;
                len -= offset;
            }
            else
            {
                break; //! RTP暂时不会把整帧数据分开传输
            }
        }
        uint8_t mpeg_ver = 0;
        uint8_t layer = 0;
        mp3_header_t *f = (mp3_header_t *)p;
        if (f->version == 0)
            mpeg_ver = 2;
        else if (f->version == 2)
            mpeg_ver = 1;
        else
            mpeg_ver = 0;
        layer = 3 - f->layer;
        int bitrate = bits_rate[mpeg_ver][layer][f->bit_rate - 1] * 1000;
        int samprate = sample_rate[mpeg_ver][f->sample_rate];
        int samples = sample_num[mpeg_ver][layer];
        frame_len = ((samples * bitrate) / samprate) + ((layer == 0) ? f->padding * 4 : f->padding);
        if (frame_len <= 0)
        {
            p++;
            len--;
            frame_len = 0;
            continue;
        }
        if (!inf->parsed)
        {
            if (len >= frame_len + 4)
            {
                uint8_t *b = p + frame_len;
                uint32_t head;
                head = b[0];
                head <<= 8;
                head |= b[1];
                head <<= 8;
                head |= b[2];
                head <<= 8;
                head |= b[3];
                if (!check_header(head))
                {
                    p++;
                    len--;
                    continue;
                }
                inf->parsed = true;
            }
            else if (len == frame_len)
            {
                inf->parsed = true;
            }
        }
        if (len >= frame_len)
        {
            if (len > PACKET_WRITE_MAX_SIZE)
            {
                do
                {
                    err = ql_aud_play_stream_start(QL_AUDIO_FORMAT_MP3, (const void *)p, len > PACKET_WRITE_MAX_SIZE ? PACKET_WRITE_MAX_SIZE : len, QL_AUDIO_PLAY_TYPE_LOCAL, NULL);
                    if (QL_AUDIO_SUCCESS != err)
                    {
                        // rtsp_play_log("ql_aud_play_stream_start failed");
                    }
                    len -= PACKET_WRITE_MAX_SIZE;
                    p += PACKET_WRITE_MAX_SIZE;
                } while (len > 0);
            }
            else
            {
                err = ql_aud_play_stream_start(QL_AUDIO_FORMAT_MP3, (const void *)p, len, QL_AUDIO_PLAY_TYPE_LOCAL, NULL);
                if (QL_AUDIO_SUCCESS != err)
                {
                    // rtsp_play_log("ql_aud_play_stream_start failed");
                }
                len = 0;
            }
        }
    }

    return 0;
}