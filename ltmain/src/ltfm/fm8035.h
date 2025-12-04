/************************************************Copyright(c)***********************************
**                                   Quintic(Nanjing) Microelectronics Co,Ltd.
**                                   
**                                   http://www.quinticcorp.com
**
**--------------------File Info----------------------------------------------------------------
** File Name:                  qndriver.h
** subversion number:   160      
**----------------------------------------------------------------------------------------
************************************************************************************************/

#ifndef _8035_H
#define _8035_H
#define QN_8035
#define _QNFUNC_H_


#define CCS_RX  0
#define CCS_TX  1
#define FREQ2CHREG(freq)   ((freq-6000)/5)
#define CHREG2FREQ(ch)     (ch*5+6000)
#define _QNCOMMON_H_
#define QND_REG_NUM_MAX   85  // for qn8035
//setting the QN8035 clock source and clock type,recommendation use 32768Hz clock as chip's clock.
#define QND_SINE_WAVE_CLOCK         0x00    //inject sine-wave clock  
#define QND_DIGITAL_CLOCK           0x80    //inject digital clock,default is inject digital clock
#define QND_CRYSTAL                 32768   //crystal unit:Hz
#define QND_CRYSTAL_DEFAULT         QND_CRYSTAL

#define QND_MODE_SLEEP      0     
#define QND_MODE_WAKEUP     1
// RX / TX value is using upper 8 bit

#define QND_MODE_RX         0x8000
#define QND_MODE_TX         0x4000
// AM / FM value is using lower 8 bit 
// need to check datasheet to get right bit 
#define QND_MODE_FM         0x0000

#define BAND_FM        0


// tune
#define QND_FSTEP_50KHZ      0
#define QND_FSTEP_100KHZ      1
#define QND_FSTEP_200KHZ      2
// output format
#define QND_OUTPUT_ANALOG     0
#define QND_OUTPUT_IIS        1

// stereo mode
#define QND_TX_AUDIO_MONO              0x10
#define QND_TX_AUDIO_STEREO            0x00

#define QND_RX_AUDIO_MONO              0x20
#define QND_RX_AUDIO_STEREO            0x00

#define QND_CONFIG_MONO               0x01
#define QND_CONFIG_MUTE               0x02    
#define QND_CONFIG_SOFTCLIP           0x03
#define QND_CONFIG_AUTOAGC               0x04
#define QND_CONFIG_AGCGAIN               0x05    

#define QND_CONFIG_EQUALIZER           0x06    
#define QND_CONFIG_VOLUME               0x07          
#define QND_CONFIG_BASS_QUALITY       0x08
#define QND_CONFIG_BASS_FREQ           0x09
#define QND_CONFIG_BASS_GAIN           0x0a
#define QND_CONFIG_MID_QUALITY        0x0b
#define QND_CONFIG_MID_FREQ           0x0c
#define QND_CONFIG_MID_GAIN           0x0d
#define QND_CONFIG_TREBLE_FREQ        0x0e
#define QND_CONFIG_TREBLE_GAIN        0x0f

#define QND_ENABLE_EQUALIZER          0x10
#define QND_DISABLE_EQUALIZER         0x00


#define QND_CONFIG_AUDIOPEAK_DEV      0x11
#define QND_CONFIG_PILOT_DEV          0x12
#define QND_CONFIG_RDS_DEV            0x13

// input format
#define QND_INPUT_ANALOG     0
#define QND_INPUT_IIS        1

// i2s mode
#define QND_I2S_RX_ANALOG   0x00
#define QND_I2S_RX_DIGITAL  0x40
#define QND_I2S_TX_ANALOG   0x00
#define QND_I2S_TX_DIGITAL  0x20

//i2s clock data rate
#define QND_I2S_DATA_RATE_32K  0x00
#define QND_I2S_DATA_RATE_40K  0x10
#define QND_I2S_DATA_RATE_44K  0x20
#define QND_I2S_DATA_RATE_48K  0x30

//i2s clock Bit Wise
#define QND_I2S_BIT_8    0x00
#define QND_I2S_BIT_16   0x40
#define QND_I2S_BIT_24   0x80
#define QND_I2S_BIT_32   0xc0

//i2s Control mode
#define QND_I2S_MASTER   1
#define QND_I2S_SLAVE    0

//i2s Control mode
#define QND_I2S_MSB   0x00
#define QND_I2S_I2S   0x01
#define QND_I2S_DSP1  0x02
#define QND_I2S_DSP2  0x03
#define QND_I2S_LSB   0x04

#define QND_EQUALIZE_BASS    0x00
#define QND_EQUALIZE_MID    0x01
#define QND_EQUALIZE_TREBLE 0x02
// RDS, TMC
#define QND_EUROPE_FLEXIBILITY_DISABLE  0
#define QND_EUROPE_FLEXIBILITY_ENABLE   1
#define QND_RDS_OFF              0
#define QND_RDS_ON               1
#define QND_RDS_BUFFER_NOT_READY 0
#define QND_RDS_BUFFER_READY     1


#define CHIPID_QN8000    0x00
#define CHIPID_QN8005    0x20
#define CHIPID_QN8005B1  0x21
#define CHIPID_QN8006    0x30
#define CHIPID_QN8006LB  0x71
#define CHIPID_QN8007B1  0x11
#define CHIPID_QN8007    0x10
#define CHIPID_QN8006A1  0x30
#define CHIPID_QN8006B1  0x31
#define CHIPID_QN8016    0xe0
#define CHIPID_QN8016_1  0xb0
#define CHIPID_QN8015    0xa0
#define CHIPID_QN8065    0xa0
#define CHIPID_QN8067    0xd0
#define CHIPID_QN8065N   0xa0
#define CHIPID_QN8027    0x40
#define CHIPID_QN8025    0x80
#define CHIPID_QN8035    0x84


#define RDS_INT_ENABLE  1
#define RDS_INT_DISABLE 0
//For antena impedance match
#define QND_HIGH_IMPEDANCE         1
#define QND_LOW_IMPEDANCE         0

#define QND_BAND_NUM     6
#define RSSINTHRESHOLD   4


typedef unsigned char  UINT8;             
//typedef char           INT8;              
typedef unsigned short UINT16;            
//typedef short          INT16;    
/*typedef unsigned int   UINT32;            
typedef signed   int   INT32;  
typedef float          FP32;              
typedef double         FP64;              
*/



#define _QNCONFIG_H_



/********************* country selection**************/
#define COUNTRY_CHINA            0
#define COUNTRY_USA                1
#define COUNTRY_JAPAN            2
/************************EDN******************************/

/********************* minor feature selection*********************/

#define  QN_CCA_MAX_CH     50

/**********************************************************************************************
// Performance configuration 
***********************************************************************************************/
#define SMSTART_VAL     19
#define HCCSTART_VAL    33
#define SNCSTART_VAL    55
#define CCA_THRESHOLD   0  //the smaller is this value,the more is number of seeking channels ,fake channels is increased also.


/**********************************************************************************************
// limitation configuration 
***********************************************************************************************/

#define QND_READ_RSSI_DELAY    10

#define QND_DELAY_BEFORE_UNMUTE  500

// auto scan
#define QND_MP_THRESHOLD       0x28   
#define PILOT_READ_OUT_DELAY_TIME 70
#define PILOT_SNR_READ_OUT_DELAY_TIME  (150-PILOT_READ_OUT_DELAY_TIME)
#define CH_SETUP_DELAY_TIME    300           





/**********************************************************************************************
 definition register 
**********************************************************************************************/
#define SYSTEM1         0x00
#define CCA             0x01
#define SNR            	0x02
#define RSSISIG         0x03
#define STATUS1         0x04
#define CID1            0x05
#define CID2            0x06
#define	CH				0x07
#define	CH_START		0x08
#define	CH_STOP			0x09
#define	CH_STEP			0x0A
#define	RDSD0			0x0B
#define	RDSD1			0x0C
#define	RDSD2			0x0D
#define	RDSD3			0x0E
#define	RDSD4			0x0F
#define	RDSD5			0x10
#define	RDSD6			0x11
#define	RDSD7			0x12
#define	STATUS2			0x13
#define	VOL_CTL			0x14
#define	XTAL_DIV0		0x15
#define	XTAL_DIV1		0x16
#define	XTAL_DIV2		0x17
#define INT_CTRL		0x18
#define SMP_HLD_THRD	0x19
#define	RXAGC_GAIN		0x1A
#define GAIN_SEL		0x1B
#define	SYSTEM_CTL1		0x1C
#define	SYSTEM_CTL2		0x1D
#define RDSCOSTAS		0x1E
#define REG_TEST		0x1F
#define STATUS4			0x20
#define	CCA1			0x27
#define	SMSTART			0x34
#define	SNCSTART		0x35
#define	HCCSTART		0x36
#define NCCFIR3         0x40
#define REG_DAC			0x4C
/**********************************************************************************************
 definition operation bit of register
**********************************************************************************************/
#define CCA_CH_DIS          0x01
#define CHSC                    0x02
#define RDSEN                    0x08
#define CH_CH		        0x03
#define CH_CH_START         0x0c
#define CH_CH_STOP            0x30
#define STNBY               0x20
#define IMR                 0x40
#define RX_MONO_MASK        0x04
#define RDS_RXUPD           0x80
#define RDSSYNC             0x10


#define _QNSYS_H_

#define CHANNEL_FILTER





// external driver interface 
// logical layer
/*****************************************************************************
Driver API Macro Definition
*****************************************************************************/
//crystal unit:Hz 
/*
#define XTAL_DIV(Crystal)   ((((Crystal)<<1)/32768 +1)>>1)
#define PLL_DLT(Crystal)    (((28500000<<1)/(((Crystal)<<1)/(XTAL_DIV(Crystal)*512)+1>>1)+1>>1)-442368)

#define QNM_SetCrystal(Crystal)    \
        QND_WriteReg(0x15, (UINT8)XTAL_DIV(Crystal));\
        QND_WriteReg(0x16, ((UINT8)(XTAL_DIV(Crystal) >> 8) & 0x07)|((UINT8)PLL_DLT(Crystal) << 3));\
        QND_WriteReg(0x17, (UINT8)(PLL_DLT(Crystal) >> 5))
*/
//crystal unit:Hz 
#define QNM_SetCrystal(Crystal)    \
        do \
	    { \
    		UINT16 xtal_div;\
    		UINT16 pll_dlt;\
    		UINT8 regValue; \
    		xtal_div= ((Crystal<<1)/32768 +1)>>1;\
    		pll_dlt = (((28500000<<1)/(((Crystal<<1)/(xtal_div*512)+1)>>1)+1)>>1)-442368; \
    		regValue = (UINT8)xtal_div;\
    		QND_WriteReg(0x15, regValue);\
    		regValue = (UINT8)(xtal_div >> 8) & 0x07;\
    		regValue |= (UINT8)pll_dlt << 3;\
    		QND_WriteReg(0x16, regValue);\
    		regValue = (UINT8)(pll_dlt >> 5);\
    		QND_WriteReg(0x17, regValue);\
		} while(0)
#define QNM_SetAudioInputImpedance(AudioImpendance) \
        QND_WriteReg(REG_VGA, QND_ReadReg(REG_VGA) | (AudioImpendance & 0x3f))   
#define QNM_ResetToDefault() \
        QND_WriteReg(SYSTEM2, SWRST) 
#define QNM_SetFMWorkingMode(Modemask, Mode) \
        QND_WriteReg(SYSTEM1, Mode|(QND_ReadReg(SYSTEM1) &~ Modemask)
#define QNM_EnableAGC() \
        QND_WriteReg(TXAGC_GAIN, ~TAGC_GAIN_SEL&(QND_ReadReg(TXAGC_GAIN)))
#define QNM_DisableAGC()\
        QND_WriteReg(TXAGC_GAIN,   1|(TAGC_GAIN_SEL|(QND_ReadReg(TXAGC_GAIN)) )
#define QNM_EnableSoftClip() \
        QND_WriteReg(TXAGC_GAIN,    TX_SFTCLPEN |(QND_ReadReg(TXAGC_GAIN)) )
#define QNM_DisableSoftClip() \
        QND_WriteReg(TXAGC_GAIN,    ~TX_SFTCLPEN &(QND_ReadReg(TXAGC_GAIN)) )
#define QNM_GetMonoMode() \
        QND_ReadReg(STATUS1) & ST_MO_RX
#define QNM_SetRxThreshold(db) \
        QND_WriteReg(CCA, db)
#define QNM_SetAudioOutFormatIIS() \
        QND_WriteReg(CCA, (QND_ReadReg(CCA) | RXI2S))
#define QNM_SetAudioOutFormatAnalog() \
        QND_WriteReg(CCA, (QND_ReadReg(CCA) & ~RXI2S))
#define QNM_SetAudioInFormatIIS() \
        QND_WriteReg(CCA, (QND_ReadReg(CCA) | RXI2S))
#define QNM_SetAudioInFormatAnalog() \
        QND_WriteReg(CCA, (QND_ReadReg(CCA) & ~RXI2S))
#define QNM_GetRssi() \
        QND_ReadReg(RSSISIG)
#define QND_AntenaInputImpedance(impendance) \
        QND_WriteReg(77, impendance)

#define QND_READ(adr)    QND_ReadReg(adr)
#define QND_WRITE(adr, value)  QND_WriteReg(adr, value)
extern void   QNF_SetRegBit(UINT8 reg, UINT8 bitMask, UINT8 data_val) ;

extern UINT8   qnd_Country;
extern UINT16  qnd_CH_START;
extern UINT16  qnd_CH_STOP;
extern UINT8   qnd_CH_STEP;

extern UINT8  qnd_R16; 
extern UINT8  qnd_R17;
extern UINT8  qnd_R46;

/*
  System General Control 
*/
extern UINT16 QNF_GetCh();

extern void QND_Delay(UINT16 ms) ;
extern UINT8 QND_GetRSSI(UINT16 ch) ;
extern UINT8 QND_Init() ;
extern void  QND_TuneToCH(UINT16 ch) ;
extern void  QND_SetSysMode(UINT16 mode) ;
extern void  QND_SetCountry(UINT8 country) ;



#define QN_RX
#define _QNRX_H_
typedef void  (*QND_SeekCallBack)(UINT16 ch, UINT8 bandtype);
extern UINT8  qnd_ChCount;
extern UINT16 qnd_ChList[QN_CCA_MAX_CH];
extern UINT8  qnd_StepTbl[3];
extern UINT8  qnd_AutoScanAll;

extern void   QND_SetSeekCallBack(QND_SeekCallBack func);
extern void   QND_RXConfigAudio(UINT8 optiontype, UINT8 option) ;
extern UINT16 QND_RXSeekCH(UINT16 start, UINT16 stop, UINT8 step, UINT8 db, UINT8 up) ;
extern UINT8  QND_RXSeekCHAll(UINT16 start, UINT16 stop, UINT8 step, UINT8 db, UINT8 up) ;
// patch 
#define QN_RDS

#define _QNRDS_H_

extern UINT8 QND_RDSEnable(UINT8 on) ;
extern UINT8 QND_RDSCheckBufferReady(void) ;

void QND_RDSHighSpeedEnable(UINT8 on) ;
int QND_RDSModeDetect(void)  ;


extern UINT8 QND_RDSDetectSignal(void) ;
extern void  QND_RDSLoadData(UINT8 *rdsRawData, UINT8 upload) ;

typedef int    QlI2CStatus;
typedef void * ql_task_t;
#define QL_FM_TASK_STACK_SIZE     		    1024 *8
#define QL_FM_TASK_PRIO          	 	    APP_PRIORITY_NORMAL
#define QL_FM_TASK_EVENT_CNT      		    5

#define QL_FM_LOG_LEVEL QL_LOG_LEVEL_INFO
#define QL_FM_LOG(msg, ...) QL_LOG(QL_FM_LOG_LEVEL, "ql_ltfm", msg, ##__VA_ARGS__)
#define QL_FM_LOG_PUSH(msg, ...) QL_LOG_PUSH("ql_ltfm", msg, ##__VA_ARGS__)


#define FM_I2C                             i2c_1
#define FM_ADDR_R                          (0x21 >>1)
#define FM_ADDR_W                          (0x20 >>1)

#endif
