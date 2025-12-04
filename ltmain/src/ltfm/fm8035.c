#include <stdio.h>
#include "fm8035.h"
#include "ql_i2c.h"
#include "ql_api_osi.h"
#include "ql_gpio.h"
#include "ql_log.h"
#include "led.h"
#include "ltplay.h"
#include "ltsystem.h"

extern UINT8 QND_ReadReg(UINT8 adr);
extern UINT8 QND_WriteReg(UINT8 adr, UINT8 value);

#define R_TXRX_MASK 0x30

UINT32 qnd_Crystal = QND_CRYSTAL_DEFAULT;
UINT8 qnd_PrevMode;
UINT8 qnd_Country = COUNTRY_CHINA;
UINT16 qnd_CH_START = 7600;
UINT16 qnd_CH_STOP = 10800;
UINT8 qnd_CH_STEP = 1;

UINT8 qnd_AutoScanAll = 0;
UINT8 qnd_IsStereo;
UINT8 qnd_ChCount;
UINT8 qnd_R16;
UINT8 qnd_R17;
UINT8 qnd_R46;
UINT16 qnd_ChList[QN_CCA_MAX_CH];
UINT8 qnd_StepTbl[3] = {5, 10, 20};
QND_SeekCallBack qnd_CallBackFunc = 0;

UINT8 QND_I2C_READ(UINT8 Regis_Addr, UINT8 *DataPtr)
{
    ql_I2cRead(FM_I2C, FM_ADDR_R, Regis_Addr, DataPtr, 1);
    return *DataPtr;
}

UINT8 QND_I2C_WRITE(UINT8 Regis_Addr, UINT8 Data)
{
    ql_I2cWrite(FM_I2C, FM_ADDR_W, Regis_Addr, &Data, 1);
    // QL_FM_LOG("Rsgis_Addr = %d, Data = %d", Regis_Addr, Data);
    return 1;
}

UINT8 QND_WriteReg(UINT8 Regis_Addr, UINT8 Data)
{
    return QND_I2C_WRITE(Regis_Addr, Data);
}

UINT8 QND_ReadReg(UINT8 Regis_Addr)
{
    UINT8 Data;
    QND_I2C_READ(Regis_Addr, &Data);
    return Data;
}

/**********************************************************************
void QNF_RXInit()
**********************************************************************
Description: set to SNR based MPX control. Call this function before
             tune to one specific channel
            设置为基于SNR的MPX控制。之前调用此函数调谐到一个特定频道
Parameters:
None
Return Value:
None
**********************************************************************/
void QNF_RXInit()
{
    QNF_SetRegBit(0x1B, 0x08, 0x00);         // Let NFILT adjust freely
    QNF_SetRegBit(0x2C, 0x3F, 0x12);         // When SNR<ccth31, ccfilt3 will work
    QNF_SetRegBit(0x1D, 0x40, 0x00);         // Let ccfilter3 adjust freely
    QNF_SetRegBit(0x41, 0x0F, 0x0A);         // Set a hcc index to trig ccfilter3's adjust
    QND_WriteReg(0x45, 0x50);                // Set aud_thrd will affect ccfilter3's tap number
    QNF_SetRegBit(0x40, 0x70, 0x70);         // snc/hcc/sm snr_rssi_sel; snc_start=0x40; hcc_start=0x30; sm_start=0x20
    QNF_SetRegBit(0x19, 0x80, 0x80);         // Use SNR for ccfilter selection criterion
    QNF_SetRegBit(0x3E, 0x80, 0x80);         // it is decided by programming this register
    QNF_SetRegBit(0x41, 0xE0, 0xC0);         // DC notching High pass filter bandwidth; remove low freqency dc signals
    QNF_SetRegBit(0x42, 0x10, 0x10);         // disable the vtune monitor
    QNF_SetRegBit(0x34, 0x7F, SMSTART_VAL);  // set SNCSTART
    QNF_SetRegBit(0x35, 0x7F, SNCSTART_VAL); // set SNCSTART
    QNF_SetRegBit(0x36, 0x7F, HCCSTART_VAL); // set HCCSTART
}

/**********************************************************************
void QNF_SetMute(UINT8 On)
**********************************************************************
Description: set register specified bit

Parameters:
On:        1: mute, 0: unmute
Return Value:
None
**********************************************************************/
void QNF_SetMute(UINT8 On)
{
    if (On)
    {
        lt_audio_pa_disable();
        QNF_SetRegBit(0x4C, 0x0B, 0x0B);
    }
    else
    {
        // QND_Delay(QND_DELAY_BEFORE_UNMUTE);
        QNF_SetRegBit(0x4C, 0x0B, 0x00);
        QND_Delay(50);
        lt_audio_pa_enable();
    }
}

/**********************************************************************
void QNF_SetRegBit(UINT8 reg, UINT8 bitMask, UINT8 data_val)
**********************************************************************
Description: set register specified bit
            设置寄存器指定位
Parameters:
    reg:        register that will be set
    bitMask:    mask specified bit of register
    data_val:    data will be set for specified bit
Return Value:
    None
**********************************************************************/
void QNF_SetRegBit(UINT8 reg, UINT8 bitMask, UINT8 data_val)
{
    UINT8 temp;
    temp = QND_ReadReg(reg);
    temp &= (UINT8)(~bitMask);
    temp |= data_val & bitMask;
    //    temp |= data_val;
    QND_WriteReg(reg, temp);
}

/**********************************************************************
UINT16 QNF_GetCh()
**********************************************************************
Description: get current channel frequency
            获取当前通道频率
Parameters:
    None
Return Value:
    channel frequency
**********************************************************************/
UINT16 QNF_GetCh()
{
    UINT8 tCh;
    UINT8 tStep;
    UINT16 ch = 0;
    // set to reg: CH_STEP
    tStep = QND_ReadReg(CH_STEP);
    tStep &= CH_CH;
    ch = tStep;
    tCh = QND_ReadReg(CH);
    ch = (ch << 8) + tCh;
    return CHREG2FREQ(ch);
}

/**********************************************************************
UINT8 QNF_SetCh(UINT16 freq)
**********************************************************************
Description: set channel frequency

Parameters:
    freq:  channel frequency to be set
Return Value:
    1: success
**********************************************************************/
UINT8 QNF_SetCh(UINT16 freq)
{
    // calculate ch parameter used for register setting
    UINT8 tStep;
    UINT8 tCh;
    UINT16 f;
    UINT16 pll_dlt;

    // this is a software patch for improving sensitivity at 85.4M,85.5M and 85.6M frequency
    if (freq == 8540 || freq == 8550 || freq == 8560)
    {
        pll_dlt = (UINT16)qnd_R16 >> 3;  // getting the pll_dlt lower 5 bits.
        pll_dlt |= (UINT16)qnd_R17 << 5; // getting the pll_dlt higher 8 bits.
        pll_dlt -= 1039;
        QND_WriteReg(0x16, (UINT8)pll_dlt << 3);
        QND_WriteReg(0x17, (UINT8)(pll_dlt >> 5));
        if (freq == 8540)
            QND_WriteReg(0x46, 0x9D);
        else if (freq == 8550)
            QND_WriteReg(0x46, 0x69);
        else
            QND_WriteReg(0x46, 0x34);
        freq = 8570;
    }
    else
    {
        QND_WriteReg(0x16, qnd_R16);
        QND_WriteReg(0x17, qnd_R17);
        QND_WriteReg(0x46, qnd_R46);
    }

    f = FREQ2CHREG(freq);
    // set to reg: CH
    tCh = (UINT8)f;
    QND_WriteReg(CH, tCh);
    // set to reg: CH_STEP
    tStep = QND_ReadReg(CH_STEP);
    tStep &= ~CH_CH;
    tStep |= ((UINT8)(f >> 8) & CH_CH);
    QND_WriteReg(CH_STEP, tStep);
    return 1;
}

/**********************************************************************
void QNF_ConfigScan(UINT16 start,UINT16 stop, UINT8 step)
**********************************************************************
Description: config start, stop, step register for FM/AM CCA or CCS

Parameters:
    start
        Set the frequency (10kHz) where scan to be started,
        eg: 7600 for 76.00MHz.
    stop
        Set the frequency (10kHz) where scan to be stopped,
        eg: 10800 for 108.00MHz
    step
        1: set leap step to (FM)100kHz / 10kHz(AM)
        2: set leap step to (FM)200kHz / 1kHz(AM)
        0:  set leap step to (FM)50kHz / 9kHz(AM)
Return Value:
         None
**********************************************************************/
void QNF_ConfigScan(UINT16 start, UINT16 stop, UINT8 step)
{
    // calculate ch para
    UINT8 tStep = 0;
    UINT8 tS;
    UINT16 fStart;
    UINT16 fStop;

    fStart = FREQ2CHREG(start);
    fStop = FREQ2CHREG(stop);
    // set to reg: CH_START
    tS = (UINT8)fStart;
    QND_WriteReg(CH_START, tS);
    tStep |= ((UINT8)(fStart >> 6) & CH_CH_START);
    // set to reg: CH_STOP
    tS = (UINT8)fStop;
    QND_WriteReg(CH_STOP, tS);
    tStep |= ((UINT8)(fStop >> 4) & CH_CH_STOP);
    // set to reg: CH_STEP
    tStep |= step << 6;
    QND_WriteReg(CH_STEP, tStep);
}

/**********************************************************************
void QNF_SetAudioMono(UINT8 modemask, UINT8 mode)
**********************************************************************
Description:    Set audio output to mono.

Parameters:
  modemask: mask register specified bit
  mode
        QND_RX_AUDIO_MONO:    RX audio to mono
        QND_RX_AUDIO_STEREO:  RX audio to stereo
        QND_TX_AUDIO_MONO:    TX audio to mono
        QND_TX_AUDIO_STEREO:  TX audio to stereo
Return Value:
  None
**********************************************************************/
void QNF_SetAudioMono(UINT8 modemask, UINT8 mode)
{
    if (mode == QND_RX_AUDIO_MONO)
        QNF_SetRegBit(SYSTEM1, modemask, 0x04);
    else
        QNF_SetRegBit(SYSTEM1, modemask, mode);
}

/**********************************************************************
int QND_Delay()
**********************************************************************
Description: Delay for some ms, to be customized according to user
             application platform

Parameters:
        ms: ms counts
Return Value:
        None

**********************************************************************/
void QND_Delay(UINT16 ms)
{
    ql_rtos_task_sleep_ms(ms);
}

/**********************************************************************
UINT8 QND_GetRSSI(UINT16 ch)
**********************************************************************
Description:    Get the RSSI value
Parameters:
Return Value:
RSSI value  of the channel setted
**********************************************************************/
UINT8 QND_GetRSSI(UINT16 ch)
{
    QND_SetSysMode(QND_MODE_RX | QND_MODE_FM);
    QNF_ConfigScan(ch, ch, qnd_CH_STEP);
    QNF_SetCh(ch);
    QNF_SetRegBit(0x00, 0x33, 0x13); // Enter CCA mode. This speed up the channel locking.
    QND_Delay(100);
    if (QND_ReadReg(RDSCOSTAS) & 0x80) // this is a software patch for reading RSSI
    {
        return (QND_ReadReg(RSSISIG) + 9);
    }
    else
    {
        return QND_ReadReg(RSSISIG);
    }
}

/**********************************************************************
void QN_ChipInitialization()
**********************************************************************
Description: chip first step initialization, called only by QND_Init()

Parameters:
None
Return Value:
None
**********************************************************************/
UINT8 qnd_ChipID;
UINT8 qnd_IsQN8035B;
void QN_ChipInitialization()
{
    QND_WriteReg(0x00, 0x81);
    QND_Delay(10);
    // qnd_ChipID = QND_ReadReg(CID1) & 0x03;
    // QL_FM_LOG("***-------qnd_ChipID:0X%02X",qnd_ChipID);
    // qnd_ChipID = QND_ReadReg(0x06);
    // printf("WHT-------qnd_ChipID:0X%02X\r\n",qnd_ChipID);
    // qnd_IsQN8035B = QND_ReadReg(0x58) & 0x1f;
    // QL_FM_LOG("***-------qnd_IsQN8035B:0X%02X",qnd_IsQN8035B);
    /*********User sets chip working clock **********/
    // change crystal frequency setting here
    QNF_SetRegBit(0x01, 0x80, QND_DIGITAL_CLOCK); // setting clock source type:sine-wave clock or digital clock
    QNF_SetRegBit(0x39, 0x3f, 11);                // set SNR threshold to be 11
    QNM_SetCrystal(qnd_Crystal);
    QND_Delay(10);
    QND_WriteReg(0x54, 0x47); // mod pll setting
    QND_WriteReg(0x19, 0x40); // AGC setting
    QND_WriteReg(0x2d, 0xD6); // notch filter threshold adjusting
    QND_WriteReg(0x43, 0x10); // notch filter threshold enable
    QND_WriteReg(0x00, 0x51); // reset the FSM
    QND_WriteReg(0x00, 0x21); // enter standby mode
    QND_Delay(50);
    // these variables are used in QNF_SetCh() function.
    qnd_R16 = QND_ReadReg(0x16);
    qnd_R17 = QND_ReadReg(0x17);
    qnd_R46 = QND_ReadReg(0x46);
    QND_Delay(50);
    QND_WriteReg(0x00, 0x21);
    QND_WriteReg(0x00, 0x11);
    QND_WriteReg(0x0a, 0x02);
    QND_WriteReg(0x07, 0xB8);
    QND_WriteReg(0x0a, 0x03);
    QND_WriteReg(0x07, 0x3A);
    QND_WriteReg(0x14, 0x07); // ��������
}

// unsigned int Rotry_Read_Byte_Rotry=696;
unsigned char CH_Setup_Flag = 1;
void QN8035_FM_Setup()
{
    float num;
    num = qnd_ChList[2];

    unsigned int Rotry_Read_Byte_Rotry = FREQ2CHREG(num);

    if (CH_Setup_Flag == 1)
    {
        CH_Setup_Flag = 0;
        QND_WriteReg(0x0a, Rotry_Read_Byte_Rotry / 256); // ����Ƶ��
        QND_WriteReg(0x07, Rotry_Read_Byte_Rotry % 256); // ����Ƶ��
    }
}
/**********************************************************************
int QND_Init()
**********************************************************************
Description: Initialize device to make it ready to have all functionality ready for use.

Parameters:
    None
Return Value:
    1: Device is ready to use.
    0: Device is not ready to serve function.
**********************************************************************/
UINT8 QND_Init()
{
    QN_ChipInitialization();
    QND_SetCountry(COUNTRY_CHINA);
    QND_RXConfigAudio(QND_CONFIG_MUTE, 1);
    // QND_RXSeekCHAll(qnd_CH_START, qnd_CH_STOP, qnd_CH_STEP, 0, 0) ;

    return 1;
}

/**********************************************************************
void QND_SetSysMode(UINT16 mode)
***********************************************************************
Description: Set device system mode(like: sleep ,wakeup etc)
Parameters:
mode:  set the system mode , it will be set by  some macro define usually:
SLEEP : set chQNF_SetRegBitip to sleep mode
WAKEUP: wake up chip
RX:     set chip work on RX mode
Return Value:
None
**********************************************************************/
void QND_SetSysMode(UINT16 mode)
{
    UINT8 val;
    switch (mode)
    {
    case QND_MODE_SLEEP: // set sleep mode
        //       QNF_SetMute(1);
        qnd_PrevMode = QND_ReadReg(SYSTEM1);
        QNF_SetRegBit(SYSTEM1, R_TXRX_MASK, STNBY);
        break;
    case QND_MODE_WAKEUP: // set wakeup mode
        QND_WriteReg(SYSTEM1, qnd_PrevMode);
        QNF_SetMute(1); // avoid "sha sha" noise from sleep to wakeup mode during.
        QNF_SetMute(0);
        break;
    default:
        val = (UINT8)(mode >> 8);
        if (val)
        {
            val = val >> 3;
            if (val & 0x10)
                // set to new mode if it's not same as old
                if ((QND_ReadReg(SYSTEM1) & R_TXRX_MASK) != val)
                {
                    QNF_SetMute(1);
                    QNF_SetRegBit(SYSTEM1, R_TXRX_MASK, val);
                }
        }
        break;
    }
}

/**********************************************************************
void QND_TuneToCH(UINT16 ch)
**********************************************************************
Description: Tune to the specific channel. call QND_SetSysMode() before
call this function
Parameters:
ch
Set the frequency (10kHz) to be tuned,
eg: 101.30MHz will be set to 10130.
Return Value:
None
**********************************************************************/
void QND_TuneToCH(UINT16 ch)
{
    // UINT8 rssi;
    // UINT8 minrssi;
    UINT8 reg;

    QNF_RXInit();
    QNF_SetMute(1);
    if ((ch == 8430) || (ch == 7290) || (ch == 6910)) // Peter has a list of channel to flip IMR. Please ask him for update
    {
        QNF_SetRegBit(CCA, IMR, IMR); // this is a software patch
    }
    else
    {
        QNF_SetRegBit(CCA, IMR, 0x00);
    }
    QNF_ConfigScan(ch, ch, qnd_CH_STEP);
    QNF_SetCh(ch);
    QNF_SetRegBit(0x00, 0x03, 0x03); // Enter CCA mode. This speed up the channel locking.
    // Auto tuning
    QND_WriteReg(0x4F, 0x80);
    reg = QND_ReadReg(0x4F);
    reg >>= 1;
    QND_WriteReg(0x4F, reg);
    QNF_SetMute(0);
}

/**********************************************************************
void QND_SetCountry(UINT8 country)
***********************************************************************
Description: Set start, stop, step for RX and TX based on different
             country
Parameters:
country:
Set the chip used in specified country:
    CHINA:
    USA:
    JAPAN:
Return Value:
    None
**********************************************************************/
void QND_SetCountry(UINT8 country)
{
    qnd_Country = country;
    switch (country)
    {
    case COUNTRY_CHINA:
        qnd_CH_START = 8750;
        qnd_CH_STOP = 10800;
        qnd_CH_STEP = 1;
        break;
    case COUNTRY_USA:
        qnd_CH_START = 8810;
        qnd_CH_STOP = 10790;
        qnd_CH_STEP = 2;
        break;
    case COUNTRY_JAPAN:
        qnd_CH_START = 7600;
        qnd_CH_STOP = 9000;
        qnd_CH_STEP = 1;
        break;
    default:
        break;
    }
}

/***********************************************************************
Description: set call back function which can be called between seeking
channel
Parameters:
func : the function will be called between seeking
Return Value:
None
**********************************************************************/
void QND_SetSeekCallBack(QND_SeekCallBack func)
{
    qnd_CallBackFunc = func;
}

/***********************************************************************
UINT16 QND_RXValidCH(UINT16 freq, UINT8 db);
***********************************************************************
Description: to validate a ch (frequency)(if it's a valid channel)
Freq: specific channel frequency, unit: 10Khz
  eg: 108.00MHz will be set to 10800.
Step:
  FM:
  QND_FMSTEP_100KHZ: set leap step to 100kHz
  QND_FMSTEP_200KHZ: set leap step to 200kHz
  QND_FMSTEP_50KHZ:  set leap step to 50kHz
Return Value:
  0: not a valid channel
  other: a valid channel at this frequency
***********************************************************************/
UINT16 QND_RXValidCH(UINT16 freq, UINT8 step)
{
    UINT8 regValue;
    UINT8 timeOut;
    QNF_ConfigScan(freq, freq, step);
    QNF_SetRegBit(SYSTEM1, 0x03, 0x02); // channel scan mode,channel frequency is decided by internal CCA

    timeOut = 0;
    do
    {
        regValue = QND_ReadReg(SYSTEM1);
        timeOut++;
        // QL_FM_LOG("QND_RXValidCH time ==%d\n!",timeOut);
        ql_rtos_task_sleep_ms(1);
    } while ((regValue & CHSC) && timeOut < 20); // when seeking a channel or time out,be quited the loop
    regValue = QND_ReadReg(0x04) & 0x08; // reading the rxcca_fail flag of RXCCA status
    if (regValue & 0x08)
    {
        return 0;
    }
    else
    {

        return freq;
    }
}

/***********************************************************************
UINT16 QND_RXSeekCH(UINT16 start, UINT16 stop, UINT16 step, UINT8 db, UINT8 up);
***********************************************************************
Description: Automatically scans the frequency range, and detects the
first channel(FM, it will be determine by the system mode which set
by QND_SetSysMode).
A threshold value needs to be passed in for channel detection.
Parameters:
start
Set the frequency (10kHz) where scan will be started,
eg: 76.00MHz will be set to 7600.
stop
Set the frequency (10kHz) where scan will be stopped,
eg: 108.00MHz will be set to 10800.
step
FM:
QND_FMSTEP_100KHZ: set leap step to 100kHz
QND_FMSTEP_200KHZ: set leap step to 200kHz
QND_FMSTEP_50KHZ:  set leap step to 50kHz
db:
Set threshold for quality of channel to be searched,
the db value range:0~(63-CCA_THRESHOLD).
up:
Set the seach direction :
Up;0,seach from stop to start
Up:1 seach from start to stop
Return Value:
The channel frequency (unit: 10kHz)
***********************************************************************/
UINT16 QND_RXSeekCH(UINT16 start, UINT16 stop, UINT8 step, UINT8 db, UINT8 up)
{
    UINT8 stepValue;
    UINT16 freq = start;
    UINT16 validCH;
    // UINT8 fm_flag;

    up = (start <= stop) ? 1 : 0;

    QNF_SetMute(1);
    stepValue = qnd_StepTbl[step];
    QNF_SetRegBit(GAIN_SEL, 0x08, 0x08);                                               // NFILT program is enabled
    QNF_SetRegBit(CCA1, 0x30, 0x30);                                                   // use threshold extension filter
    QNF_SetRegBit(CCA, 0x3F, ((CCA_THRESHOLD + db) > 63) ? 63 : (CCA_THRESHOLD + db)); // setting the threshold for CCA
    QNF_SetRegBit(0x39, 0x3f, 9);                                                      // set SNR threshold to be 11
    QNF_SetRegBit(0x3A, 0xC0, 0x40);                                                   // set CCA_NAGC to be 20ms
    freq = freq + (up ? stepValue : -stepValue);
    do
    {
        validCH = QND_RXValidCH(freq, step);
        if (validCH == 0)
        {
            if ((!up && (freq <= stop)) || (up && (freq >= stop)))
            {
                break;
            }
            else
            {
                freq = freq + (up ? stepValue : -stepValue);
            }
            if (SND_FM == ltplay_get_src())
            {

                //  set_function_state(FM,freq);
                set_function_state(FM_CHECK, freq);
            }
        }
    } while (validCH == 0);
    QND_TuneToCH(freq);
    return freq;
}

/**********************************************************************
UINT8 QND_RXSeekCHAll(UINT16 start, UINT16 stop, UINT16 step, UINT8 db, UINT8 up)
**********************************************************************
Description:    Automatically scans the complete FM or AM band and detects
            all the available  channels(AM or FM, it will be determine by
            the workmode which set by QND_SetSysmode). A threshold value
            needs to be passed in for the channel detection.
Parameters:
    start
        Set the frequency (10kHz) where scan will be started,
        eg: 76.00MHz will be set to 7600.
    stop
        Set the frequency (10kHz) where scan will be stopped,
        eg: 108.00MHz will be set to 10800.
    Step
        FM:
            QND_FMSTEP_100KHZ: set leap step to 100kHz
            QND_FMSTEP_200KHZ: set leap step to 200kHz
            QND_FMSTEP_50KHZ:  set leap step to 50kHz
        AM:
        QND_AMSTEP_***:
    db:
    Set threshold for quality of channel to be searched,
    the db value range:0~(63-CCA_THRESHOLD).
    up:
        Set the seach direction :
        Up;0,seach from stop to start
        Up:1 seach from start to stop

Return Value:
  The channel count found by this function
  -1: no channel found
**********************************************************************/

unsigned char fm_num = 0;
UINT8 QND_RXSeekCHAll(UINT16 start, UINT16 stop, UINT8 step, UINT8 db, UINT8 up)
{
    UINT16 freq;
    UINT16 temp;
    UINT8 stepvalue;

    stop = stop > qnd_CH_STOP ? qnd_CH_STOP : stop;
    QNF_SetMute(1);
    qnd_ChCount = 0;
    fm_num = 0;
    up = (start < stop) ? 1 : 0;
    qnd_AutoScanAll = 1;
    stepvalue = qnd_StepTbl[step];
    QNF_SetRegBit(GAIN_SEL, 0x08, 0x08);                                               // NFILT program is enabled
    QNF_SetRegBit(CCA1, 0x30, 0x30);                                                   // use threshold extension filter
    QNF_SetRegBit(CCA, 0x3F, ((CCA_THRESHOLD + db) > 63) ? 63 : (CCA_THRESHOLD + db)); // setting the threshold for CCA
    QNF_SetRegBit(0x39, 0x3F, 8);                                                      // set SNR threshold to be 11
    // set CCA_NAGC to be 20ms,
    // 0x00:10ms;0x40:20ms
    // 0x80:40ms;0xC0:60ms
    QNF_SetRegBit(0x3A, 0xC0, 0x40);
    for (freq = start; (up ? (freq <= stop) : (freq >= stop));) // add support for both direction scan
    {
        temp = QND_RXValidCH(freq, step);
        if (temp)
        {
            qnd_ChList[qnd_ChCount] = temp;
            QL_FM_LOG("qnd_ChList[%d] = %d", qnd_ChCount, temp);
            qnd_ChCount++;
        }
        freq += (up ? stepvalue : -stepvalue);
    }
    QL_FM_LOG("RX SEEK ALL SUCCESS!");
    QL_FM_LOG("qnd_ChCount  %d", qnd_ChCount);
    // QND_TuneToCH((qnd_ChCount >= 1)? qnd_ChList[1] : stop);
    qnd_AutoScanAll = 0;
    QNF_SetMute(0);
    return qnd_ChCount;
}

/************************************************************************
void QND_RXConfigAudio(UINT8 optiontype, UINT8 option )
*************************************************************************
Description: config audio
Parameters:
  optiontype: option
    QND_CONFIG_MONO; ��option��control mono, 0: stereo receive mode ,1: mono receiver mode
    QND_CONFIG_MUTE; ��option��control mute, 0:mute disable,1:mute enable
    QND_CONFIG_VOLUME: 'option' control the volume gain,range : 0~47(-47db~0db)

Return Value:
    none
**********************************************************************/
void QND_RXConfigAudio(UINT8 optiontype, UINT8 option)
{
    UINT8 regVal;
    switch (optiontype)
    {
    case QND_CONFIG_MONO:
        if (option)
            QNF_SetAudioMono(RX_MONO_MASK, QND_RX_AUDIO_MONO);
        else
            QNF_SetAudioMono(RX_MONO_MASK, QND_RX_AUDIO_STEREO);
        break;
    case QND_CONFIG_MUTE:
        if (option)
            QNF_SetMute(1);
        else
            QNF_SetMute(0);
        break;
    case QND_CONFIG_VOLUME: // set volume control gain
        if (option > 47)
            option = 47;
        regVal = (UINT8)(option / 6);      // volume: [-42db, 0db]
        QNF_SetRegBit(0x14, 0x07, regVal); // set analog gain
        regVal = (UINT8)(option % 6);
        QNF_SetRegBit(0x14, 0x38, (UINT8)((5 - regVal) << 3)); // set digital gain
        break;
    default:
        break;
    }
}

/**********************************************************************
UINT8 QND_RDSEnable(UINT8 mode)
**********************************************************************
Description: Enable or disable chip to work with RDS related functions.
Parameters:
          on: QND_RDS_ON:  Enable chip to receive/transmit RDS data.
                QND_RDS_OFF: Disable chip to receive/transmit RDS data.
Return Value:
           QND_SUCCESS: function executed
**********************************************************************/
UINT8 QND_RDSEnable(UINT8 on)
{
    if (on == QND_RDS_ON)
    {
        QNF_SetRegBit(SYSTEM1, 0x08, 0x08); // RDS enable
    }
    else if (on == QND_RDS_OFF)
    {
        QNF_SetRegBit(SYSTEM1, 0x08, 0x00); // RDS disable
    }
    else
    {
        return 0;
    }
    return 1;
}

/**********************************************************************
void QND_RDSHighSpeedEnable(UINT8 on)
**********************************************************************
Description: Enable or disable chip to work with RDS related functions.
Parameters:
  on:
    1: enable 4x rds to receive/transmit RDS data.
    0: disable 4x rds, enter normal speed.
Return Value:
  none
**********************************************************************/
void QND_RDSHighSpeedEnable(UINT8 on)
{
    QNF_SetRegBit(0x18, 0x08, on ? 0x08 : 0x00);
}

/***********************************
char QND_RDSModeDetect(void)
************************************
Description: Check the RDS mode for

Parameters:
None
Return Value:
1: 4kb/s RDS signal is detected
0: 1kb/s RDS signal is detected
-1: No RDS signal is detected
************************************/
int QND_RDSModeDetect(void)
{
    // UINT8 i;
    UINT8 val;
    QND_RDSEnable(1);
    for (int i = 1; i >= 0; i--)
    {
        QND_RDSHighSpeedEnable(i);
        QND_Delay(1500);
        val = QND_RDSDetectSignal();
        if (val & 0x10)
            return i;
    }
    QND_RDSEnable(0);
    return -1;
}

/**********************************************************************
UINT8 QND_DetectRDSSignal(void)
**********************************************************************
Description: detect the RDSS signal .

Parameters:
    None
Return Value:
    the value of STATUS3
**********************************************************************/
UINT8 QND_RDSDetectSignal(void)
{
    UINT8 val = QND_ReadReg(STATUS2);
    return val;
}

/**********************************************************************
void QND_RDSLoadData(UINT8 *rdsRawData, UINT8 upload)
**********************************************************************
Description: Load (TX) or unload (RX) RDS data to on-chip RDS buffer.
             Before calling this function, always make sure to call the
             QND_RDSBufferReady function to check that the RDS is capable
             to load/unload RDS data.
Parameters:
  rdsRawData :
    8 bytes data buffer to load (on TX mode) or unload (on RXmode)
    to chip RDS buffer.
  Upload:
    1-upload
    0--download
Return Value:
    QND_SUCCESS: rds data loaded to/from chip
**********************************************************************/
void QND_RDSLoadData(UINT8 *rdsRawData, UINT8 upload)
{
    UINT8 i;
    UINT8 temp;
    {
        // RX MODE
        for (i = 0; i <= 7; i++)
        {
            temp = QND_ReadReg(RDSD0 + i);
            rdsRawData[i] = temp;
        }
    }
}

/**********************************************************************
UINT8 QND_RDSCheckBufferReady(void)
**********************************************************************
Description: Check chip RDS register buffer status before doing load/unload of
RDS data.

Parameters:
    None
Return Value:
    QND_RDS_BUFFER_NOT_READY: RDS register buffer is not ready to use.
    QND_RDS_BUFFER_READY: RDS register buffer is ready to use. You can now
    unload (for RX) data from RDS buffer
**********************************************************************/
UINT8 QND_RDSCheckBufferReady(void)
{
    UINT8 val;
    UINT8 rdsUpdated;
    rdsUpdated = QND_ReadReg(STATUS2);
    do
    {
        val = QND_ReadReg(STATUS2) ^ rdsUpdated;
    } while (!(val & RDS_RXUPD));
    return QND_RDS_BUFFER_READY;
}

///////////////////////////////////////////////////////////////////

#if 0
static void ql_key_fm_scan()
{
    QL_FM_LOG("FM scan... ...");
    QND_RXSeekCH(qnd_CH_START, qnd_CH_STOP, qnd_CH_STEP, 2, 0) ;
    ql_rtos_task_sleep_ms(1000);
}

static void ql_key_fm_set()
{
    QL_FM_LOG("fm_num  %d",fm_num);
    QL_FM_LOG("FM set %d",qnd_ChList[fm_num] );
    QND_TuneToCH(qnd_ChList[fm_num] );
    fm_num++;
    if(fm_num == qnd_ChCount)
    {
        fm_num=0;
    }
}

#endif
