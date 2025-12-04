/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2024-07-11 13:11:28
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-11-30 17:01:34
 */
/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2024-07-11 09:12:16
 * @LastEditors: zhouhao
 * @LastEditTime: 2024-07-11 13:01:34
 */
/* Copyright (C) 2019 RDA Technologies Limited and/or its affiliates("RDA").
 * All rights reserved.
 *
 * This software is supplied "AS IS" without any warranties.
 * RDA assumes no responsibility or liability for the use of the software,
 * conveys no license or title under any patent, copyright, or mask work
 * right to the product. RDA reserves the right to make changes in the
 * software without notification.  RDA also make no representation or
 * warranty that such application will be suitable for the specified use
 * without further testing or modification.
 * 
 */
#ifndef _LT_HTTP_H_
#define _LT_HTTP_H_
	
#ifdef __cplusplus
	extern "C" {
#endif
	
	
	
	/*========================================================================
	 *	function Definition
	 *========================================================================*/
void lt_http_app_init(void);
void lt_http_post_app_init(void);
void lt_http_put_app_init(void);
void lt_https_get_app_init(void);
	
void lt_http_dload_mp3( const char *file_name ,char *url);
char lt_http_dload_ble( const char *file_name ,char *url ,char *md5);
char lt_http_dload_config( const char *file_name ,char *url ,char *md5);
int verify_file_md5(const char *filename, const char *expected_md5);

void lt_http_get_api_request(char *url);
#ifdef __cplusplus
	}/*"C" */
#endif


#endif // _LT_HTTP_H_


