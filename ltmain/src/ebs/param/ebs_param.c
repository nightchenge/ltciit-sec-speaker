/**
 * @file ebs_param.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-08
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include "ebs_param.h"
#include <string.h>
#include <stdlib.h>
#include "util_crc.h"
#include "ql_api_dev.h"

uint8_t resources_code_remote[RESOURCES_CODE_LEN] = {0xF3, 0x32, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01};

// db_ebs_param_t ebs_param = {
//     // .resources_code = {0xF6, 0x32, 0x02, 0x13, 0x00, 0x00, 0x00, 0x03, 0x14, 0x01, 0x04, 0x01},
//     // 65326251010000314010401
//     .resources_code = {0xF6, 0x53, 0x26, 0x25, 0x10, 0x10, 0x00, 0x03, 0x14, 0x01, 0x04, 0x01},
//     .phy_addr = {0x17, 0x01, 0x00, 0x65, 0x43, 0x21},
//     .reg_ip = {0xC0A863E9, 0xC0A863E9}, // 0xC0A863E9
//     .reg_port = {8801, 8801},
//     .lbk_ip = {0xC0A863E9, 0xC0A863E9},
//     .lbk_port = {8802, 8802},
//     .reg_ip_4g = {0x9923B2DE, 0x9923B2DE}, // 0x9923B2DE
//     .reg_port_4g = {8801, 8801},
//     .lbk_ip_4g = {0x9923B2DE, 0x9923B2DE},
//     .lbk_port_4g = {8802, 8802},
//     .heart_period = 20,
//     .lbk_period = 20,
//     .chn_priority = {'1', '2', '3', '\0'},
//     .is_ipsc = 0,
//     .is_upper_first = 1,
//     .is_chn_switch = 1,
//     .fm_list.fm_cnt = 1,
//     .fm_list.fm_param[0].fm_freq = 93700,
//     .fm_list.fm_param[0].fm_index = 1,
//     .fm_list.fm_param[0].fm_priority = 0,
//     .is_frame_crc = 1,
//     .is_fm_remain = 1,
//     .fm_remain_period = 600,
//     .volume = 100,
//     .volume_db_max = -2,
// };

db_ebs_param_t ebs_param = {
    .resources_code = {0xF4, 0x45, 0x04, 0x03, 0x00, 0x00, 0x00, 0x03, 0x14, 0x01, 0x04, 0x01}, // 45040300000001
    .phy_addr = {0x17, 0x01, 0x00, 0x65, 0x43, 0x21},
    .reg_ip = {0xDA5A8D92, 0xDA5A8D92}, // 0xC0A863E9 218.90.141.146
    .reg_port = {28434, 28434},
    .lbk_ip = {0xDA5A8D92, 0xDA5A8D92},
    .lbk_port = {38882, 38882},
    .reg_ip_4g = {0xDA5A8D92, 0xDA5A8D92}, // 0x9923B2DE
    .reg_port_4g = {28434, 28434},
    .lbk_ip_4g = {0xDA5A8D92, 0xDA5A8D92},
    .lbk_port_4g = {38882, 38882},
    .heart_period = 60,
    .lbk_period = 60,
    .chn_priority = {'1', '2', '3', '\0'},
    .is_ipsc = 0,
    .is_upper_first = 1,
    .is_chn_switch = 1,
    .fm_list.fm_cnt = 1,
    .fm_list.fm_param[0].fm_freq = 93700,
    .fm_list.fm_param[0].fm_index = 1,
    .fm_list.fm_param[0].fm_priority = 0,
    .is_frame_crc = 1,
    .is_fm_remain = 1,
    .fm_remain_period = 600,
    .volume = 100,
    .volume_db_max = -2,
    .crc = 0,
};

db_ebs_param_t ebs_param_bak = {
    .resources_code = {0xF4, 0x45, 0x04, 0x03, 0x00, 0x00, 0x00, 0x03, 0x14, 0x01, 0x04, 0x01},
    .phy_addr = {0x17, 0x01, 0x00, 0x65, 0x43, 0x21},
  //  .reg_ip = {0xDA5A8D92, 0xDA5A8D92}, // 0xC0A863E9 218.90.141.146 088963C3
    // .reg_ip = {0x088963C3, 0x088963C3},
    // .reg_port = {8801, 8801},
    // .lbk_ip = {0x088963C3, 0x088963C3},
    // .lbk_port = {8802, 8802},
    .reg_ip = {0xDA5A8D92, 0xDA5A8D92}, // 0xC0A863E9 218.90.141.146
    .reg_port = {28434, 28434},
    .lbk_ip = {0xDA5A8D92, 0xDA5A8D92},
    .lbk_port = {38882, 38882},
    .reg_ip_4g = {0xDA5A8D92, 0xDA5A8D92}, // 0x9923B2DE
    .reg_port_4g = {28434, 28434},
    .lbk_ip_4g = {0xDA5A8D92, 0xDA5A8D92},
    .lbk_port_4g = {38882, 38882},
    .heart_period = 60,
    .lbk_period = 60,
    .chn_priority = {'1', '2', '3', '\0'},
    .is_ipsc = 0,
    .is_upper_first = 1,
    .is_chn_switch = 1,
    .fm_list.fm_cnt = 1,
    .fm_list.fm_param[0].fm_freq = 93700,
    .fm_list.fm_param[0].fm_index = 1,
    .fm_list.fm_param[0].fm_priority = 0,
    .is_frame_crc = 1,
    .is_fm_remain = 1,
    .fm_remain_period = 600,
    .volume = 100,
    .volume_db_max = -2,
    .crc = 0,
};

db_ebs_nms_t ebs_nms = {
    .index = 0,
    .ip = 0,
    .mask = 0,
    .gw = 0,
};

int32_t ebs_param_init(uint32_t stage)
{
#if PLATFORM == PLATFORM_LINUX
    extern int32_t db_ebs_param_entry_get(db_ebs_param_t * entry);
    if (0 != db_ebs_param_entry_get(&ebs_param))
    {
        db_ebs_param_entry_add(&ebs_param);
        db_ebs_param_entry_modify(&ebs_param);
    }

    startup_phy_addr_get(ebs_param.phy_addr);
    db_save(EBS_DB_FILE);
    memcpy(ebs_param_bak.phy_addr, ebs_param.phy_addr, sizeof(ebs_param_bak.phy_addr));
#elif PLATFORM == PLATFORM_Ex800G
    if (ql_file_exist(PARAM_FILE) != QL_FILE_OK)
    {
        int ret = 0;
        char devinfo[64] = {0};
        ret = ql_dev_get_imei(devinfo, 64, 0);
        if (ret != QL_DEV_SUCCESS)
        {
            ebs_log("ql_dev_get_imei failed, ret=%d\n", ret);
            return -1;
        }
        uint8_t phy[PHY_ADDR_LEN] = {0};
        if (dec_str2bcd_arr(devinfo, phy, PHY_ADDR_LEN) != 0)
        {
            ebs_log("dec_str2bcd_arr failed\n");
            return -1;
        }
        if (memcmp(phy, ebs_param_bak.phy_addr, PHY_ADDR_LEN) != 0)
        {
            memcpy(ebs_param_bak.phy_addr, phy, PHY_ADDR_LEN);
            ebs_param_save();
        }
        QFILE fd = ql_fopen(PARAM_FILE, "wb+");
        ebs_param_bak.crc = crc32_mpeg2((uint8_t *)&ebs_param_bak, sizeof(db_ebs_param_t) - 4);
        ql_fwrite(&ebs_param_bak, sizeof(db_ebs_param_t), 1, fd);
        ql_fclose(fd);
    }
    else
    {
        QFILE fd = ql_fopen(PARAM_FILE, "rb+");
        ql_fread(&ebs_param, sizeof(db_ebs_param_t), 1, fd);
        ql_fclose(fd);
        uint32_t crc = crc32_mpeg2((uint8_t *)&ebs_param, sizeof(db_ebs_param_t) - 4);
        if (crc != ebs_param.crc)
        {
            ebs_log("crc error, crc=%u, ebs_param.crc=%u\n", crc, ebs_param.crc);
            ebs_param_reset();
        }
    }
#endif
    return 0;
}

int ebs_param_save(void)
{
#if PLATFORM == PLATFORM_LINUX
    db_ebs_param_entry_modify(&ebs_param);
    return db_save(EBS_DB_FILE);
#elif PLATFORM == PLATFORM_Ex800G
    QFILE fd = ql_fopen(PARAM_FILE, "wb+");
    ebs_param.crc = crc32_mpeg2((uint8_t *)&ebs_param, sizeof(db_ebs_param_t) - 4);
    ql_fwrite(&ebs_param, sizeof(db_ebs_param_t), 1, fd);
    ql_fclose(fd);
    return 0;
#endif
}

int ebs_param_reset(void)
{
#if PLATFORM == PLATFORM_LINUX
    db_ebs_param_entry_get(&ebs_param);
    memcpy(ebs_param_bak.phy_addr, ebs_param.phy_addr, sizeof(ebs_param.phy_addr));
    db_ebs_param_entry_modify(&ebs_param_bak);
    db_ebs_nms_t ebs_nms;
    ebs_nms.index = 0;
    ebs_nms.ip = 0xC0A8640C;
    ebs_nms.mask = 0xFFFFFF00;
    ebs_nms.gw = 0xC0A86401;
    db_ebs_nms_entry_modify(&ebs_nms);
    return db_save(EBS_DB_FILE);
#elif PLATFORM == PLATFORM_Ex800G
    // memcpy(ebs_param_bak.phy_addr, ebs_param.phy_addr, sizeof(ebs_param.phy_addr));
    // ebs_param_bak.crc = crc32_mpeg2((uint8_t *)&ebs_param_bak, sizeof(db_ebs_param_t) - 4);
    // memcpy(&ebs_param, &ebs_param_bak, sizeof(db_ebs_param_t));
    // QFILE fd = ql_fopen(PARAM_FILE, "wb+");
    // ql_fwrite(&ebs_param_bak, sizeof(db_ebs_param_t), 1, fd);
    // ql_fclose(fd);
    ql_remove(PARAM_FILE);
    ebs_param_init(0);
    return 0;
#endif
}
