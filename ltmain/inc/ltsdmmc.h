#ifndef _LTSDMMC_H_
#define _LTSDMMC_H_

typedef int    QlSDMMCStatus;
typedef void * ql_task_t;

/*===========================================================================
 * Functions declaration
 ===========================================================================*/
void lt_sdmmc_app_init(void);

int lt_sdmmc_get_state();
#endif // _LTSDMMC_H_