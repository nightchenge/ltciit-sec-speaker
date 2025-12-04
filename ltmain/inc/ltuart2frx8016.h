

#ifndef LTUART2FRX8016_DEMO_H
#define LTUART2FRX8016_DEMO_H

#ifdef __cplusplus
extern "C"
{
#endif


#define QL_CUR_UART_PORT 0
#define QL_CUR_UART_TX_PIN 18
#define QL_CUR_UART_TX_FUNC 0x00
#define QL_CUR_UART_RX_PIN 17
#define QL_CUR_UART_RX_FUNC 0x00


    typedef enum lt_msg_type
    {
        MSG_SEND,
        MSG_RECV
    } lt_msg_type_t;

    typedef struct lt_conf_msg
    {
        lt_msg_type_t type;
        uint32_t datalen;
        uint8_t *data;
    } lt_conf_msg_t;

/*
    enum
    {
        ASR_AUDIO_FM=1,
        ASR_AUDIO_MP3,
        ASR_AUDIO_SMS,
        ASR_CALL_FAMILY,
        ASR_CALL_SOS,
        ASR_STATE_VOLTAGE,
        ASR_STATE_WEATHER,
        ASR_STATE_SINGAL,
        ASR_STATE_MATHINE  

    }asr_function_num;
*/

    enum
    {
        ASR_WAKEUP=0x01,
        ASR_SHUTDOWN=0x02,
        ASR_SYSTEM=0x03,
        ASR_AUDIO,
        ASR_CALL,
        ASR_STATE

    }asr_function_num;

    enum
    {
        ASR_START   =   0x00,
        ASR_FM      =   0x02,
        ASR_MP3     =   0x01,
        ASR_SMS     =   0x04,
        ASR_FAMILY  =   0x05,
        ASR_SOS     =   0x06,
        
        ASR_LAST    =   0xFA,
        ASR_NEXT    =   0xFB,
        ASR_STOP    =   0xFF,

    }asr_state_num;


    typedef struct asr_type_state_s
    {
        uint8_t function;
        uint8_t state;
        void (*start_function)(void *);
        void (*stop_function)(void *);
        void (*next_function)(void *); 
        void (*last_function)(void *); 
        //void (*pause_function)(void *);
    }asr_type_state_t;

    void lt_uart2frx8016_init(void);
    void lt_uart2frx8016_send(char *data, int data_len);
    int lt_uart2frx8016_battery_get();
    void ltasr_callback_register(asr_type_state_t *reg);



void lt_shutdown();

int lt_ltasr_wakeup();

void lt_ota_start_public();


void set_ble_version(char *version);
char *get_asr_version();
char *get_ble_version();

#ifdef __cplusplus
}
#endif

#endif /* LTUART2FRX8016_DEMO_H */
