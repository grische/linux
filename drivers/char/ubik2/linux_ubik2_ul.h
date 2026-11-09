/*-----------------------------------------------------------------------------*\
	UBIK2 Upper Layer Protocol
    channel based data exchange MIPS <-> UBIK2
\*-----------------------------------------------------------------------------*/

#ifndef _UBIK2_UL_H_
#define _UBIK2_UL_H_

#if defined(__KERNEL__)
/*--- #define UL_STATISTIC ---*/
#endif/*--- #if defined(__KERNEL__) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _logic_chan_ul {
	logic_chan_controlchan = 0,       /*--- wird vom ul-proto (zuerst) aufgebaut: dient zum Lifettime-check, Debug, ul_release ... ---*/ 
    logic_chan_Messages    = 1,
	logic_chan_D_Kanal_TE  = 2,
	logic_chan_D_Kanal_NT  = 3,
#if (CONFIG_FBOX_UBIK2 != 5)
	logic_chan_Manhattan0_Data = 4,/* unbenutzt */
	logic_chan_Manhattan1_Data = 5,/* slic2   */
	logic_chan_Manhattan2_Data = 6,/* slic3   */ 
	logic_chan_Transparent     = 7,
	logic_chan_AnalogAB        = 8, /*---- slic1 ---*/
	logic_chan_AnalogPOTS      = 9, /*---- pots ---*/    
	logic_chan_B1_Kanal_TE     = 10,/*--- nur isdnab ---*/
	logic_chan_B2_Kanal_TE     = 11,/*--- nur isdnab ---*/
	logic_chan_B1_Kanal_NT     = 12,/*--- nur isdnab ---*/
	logic_chan_B2_Kanal_NT     = 13,/*--- nur isdnab ---*/
    logic_chan_MaxChan         = 14 
#else
    logic_chan_delic_Data      = 4,     /*--- Profi-VOIP  ---*/
	logic_chan_MaxChan = logic_chan_delic_Data + 24 
#endif
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _logic_chan_ll {
    logic_chan_ubik2_boot_phase_0 = 0,
    logic_chan_ubik2_boot_phase_1 = 1,
    logic_chan_ubik2_boot_phase_2 = 2,
    logic_chan_ubik2_boot_phase_last = 3,
    logic_chan_ubik2_dsp_loopback    = 7,
    logic_chan_ubik2_through_data = 8,
    logic_chan_ubik2_remote_loopback = 9,
    logic_chan_fpga_voip     = 10,
    logic_no_chan = 30,
    logic_chan_last = 31
};

/*-------------------------------------------------------------------------------------*\
 * UL-Proto für logic_chan_Messages wenn LL-Layer = logic_chan_fpga_voip     
\*-------------------------------------------------------------------------------------*/
#define UL_VOIP_TRANSFERSIZE     128        /*--- die max. Transfersize für ungesplittetes Senden der uVOIP ---*/
#define UL_VOIP_WINDOWSIZE         1        /*--- Ws der uVOIP ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
enum _chan_message_voip_config{
    voip_chanx_off           = 0,
    voip_chanx_8khz          = 1,
    voip_chanx_8khz_only_rcv = 2
};

/*-------------------------------------------------------------------------------------*\
 * derzeitiges Format der Daten im logic_chan_Messages
\*-------------------------------------------------------------------------------------*/
struct _chan_message_voip {
    enum _logic_chan_ul ManhattanChan     :  5;
    enum _chan_message_voip_config Config :  2;
    unsigned int EchoCanceler             :  1;
    unsigned int Reserved                 : 24;
};

/*-------------------------------------------------------------------------------------*\
 * in der 5010/5012/... wird die logic_chan_Messages (DSP -> MIPS) missbraucht, um voip2voip
 * zu synchronisieren (zusätzlich zum TriggerCount werden Status-Infos versendet)
\*-------------------------------------------------------------------------------------*/
struct _chan_dtmf {
    unsigned char Next               : 3;               /*--- 1 = es existiert Nachfolge-Element ---*/
    enum _logic_chan_ul ManhattanChan: 5;
    unsigned char DTMF_Sign;
};

struct _chan_message_dsptomips {
    unsigned int TriggerCount;
    unsigned int DSP_XDU;
    unsigned int DSP_OVR;
    unsigned int MIPS_OVR;
    unsigned int ulRxMask;
    /*--- variable Anzahl _chan_dtmf; ---*/

};

/*-------------------------------------------------------------------------------------*\
 * als Alternative zu _chan_message_dsptomips (Config-Info beim Startup)
 * gueltig bis DSP-Fw-Version 2.115
\*-------------------------------------------------------------------------------------*/
struct _chan_configmessage_dsptomips {
    unsigned int MaxEcChan:4;
    unsigned int MaxDTMFChan:4;
    unsigned int MaxCodecChan:8;
    unsigned int MaxBChan:4;
    unsigned int MaxAnalogChan:4;
    unsigned int FifoSize:8;
    unsigned int svnVersion;       
    unsigned int Reserved1;
    unsigned int Reserved2;
    unsigned int Flag;          /*--- muss -1 sein, zur Erkennung, dass keine struct _chan_message_dsptomips ---*/
};

/*-------------------------------------------------------------------------------------*\
 * als Alternative zu _chan_message_dsptomips (Config-Info beim Startup)
 * gueltig ab DSP-Fw-Version 2.116
\*-------------------------------------------------------------------------------------*/
struct _chan_configmessage_dsptomips_fw116 {
    unsigned int MaxEcChan:4;
    unsigned int MaxDTMFChan:4;
    unsigned int MaxCodecChan:6;
    unsigned int MaxBChan:6;
    unsigned int MaxAnalogChan:4;
    unsigned int FifoSize:8;
    unsigned int svnVersion;       
    unsigned int Reserved1;
    unsigned int Reserved2;
    unsigned int Flag;          /*--- muss -1 sein, zur Erkennung, dass keine struct _chan_message_dsptomips ---*/
};

/*-----------------------------------------------------------------------------*\
	ubik2_ul_rx_getbuffer  muss solange kein Protokollfehler vorliegt
      immer einen Buffer zurückliefern.

	Params: 1. Handle logischen Kanal
		    2. Doppelpointer auf Buffer

	Returnwert ist das Handle, welches bei ubik2_ul_rx_putbuffer angegeben
	wird.
\*-----------------------------------------------------------------------------*/
typedef void *(*ubik2_ul_rx_alloc_buffer_t)(void *, unsigned char **);

/*-----------------------------------------------------------------------------*\
	Ein zuvor mit ubik2_ul_rx_alloc_buffer(_t) allozieerter Buffer
	ist mit Informationen gefüllt und wird als empfangene Daten zurück
	geliefert.

	Params: 1. Handle logischen Kanal
		    2. Handle vom ubik2_ul_rx_alloc_buffer(_t)
		    3. Länge der Daten im Buffer
            4. empfangener Poolflag-Value (max. 3 Bit!)
	Return:  void
\*-----------------------------------------------------------------------------*/
typedef void (*ubik2_ul_rx_put_buffer_t)(void *, void *, int, unsigned int );

/*-----------------------------------------------------------------------------*\
	Fordert einen Buffer mit Sendedaten an. Ein weitere Aufruf dieser
	Funktion gibt den vorigen (Sende) Buffer free.
	Params:   1. Handle für logischen Kanal 
		      2. Doppelpointer auf Buffer
              3. Header/Poolflag-Value  (max. 3 Bit!)
	Return:  Länge
\*-----------------------------------------------------------------------------*/
typedef int (*ubik2_ul_tx_get_buffer_t)(void *, char **, unsigned int *);

/*-----------------------------------------------------------------------------*\
	Registrieren eines logischen Kanals
	Return: Handle fuer logischen Kanal, oder NULL für Fehler
\*-----------------------------------------------------------------------------*/
void *ubik2_ul_register(enum _logic_chan_ll ll_chan,
                        enum _logic_chan_ul chan, 	
                        unsigned int rxfree_buffer_count,  /*--- Anzahl der freien Buffer fuer Empfang ---*/
                        unsigned int txfree_buffer_count,  /*--- Anzahl der freien Buffer fuer Senden (nur für Debug) ---*/
                        unsigned int rxbuffer_size,      /*--- groesse eines rx-Buffers in Bytes ---*/
                        ubik2_ul_rx_alloc_buffer_t,
                        ubik2_ul_rx_put_buffer_t,
                        ubik2_ul_tx_get_buffer_t);

/*-----------------------------------------------------------------------------*\
 * gibt logischen Chan zuück vom logischen hHandle
\*-----------------------------------------------------------------------------*/
enum _logic_chan_ul ubik2_ul_getchanfromhandle(void *ulHandle); 

/*-----------------------------------------------------------------------------*\
	Information, dass ein zuvor verwendetet Rx Buffer, dem Buffer-Pool
	wieder zur Verfügung steht. "Handle" ist das jenige, welches bei register
	geliefert wurde.
\*-----------------------------------------------------------------------------*/
void ubik2_ul_rx_buffer_conf(void *Handle);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if defined(__KERNEL__)
void ubik2_ul_powerdown(enum _logic_chan_ll ll_chan, unsigned int Enable);
#endif/*--- #if defined(__KERNEL__) ---*/

#if defined(_UBIK2_) 
/*-------------------------------------------------------------------------------------*\
 * Reconfiguriert Channel auf Start-Blocknr.
\*-------------------------------------------------------------------------------------*/
void ubik2_ul_reconfig(void *Handle);
#endif/*--- #if defined(_UBIK2_) ---*/ 

/*-----------------------------------------------------------------------------*\
    Sendewunsch signalisieren, hat einen Tx-Callback zur Folge.
	"Handle" ist dasjenige, welches bei register geliefert wurde.
\*-----------------------------------------------------------------------------*/
void ubik2_ul_tx_trigger(void *Handle);

/*-----------------------------------------------------------------------------*\
    Logischen Kanal freigeben. 
	"Handle" ist dasjenige, welches bei register geliefert wurde.
\*-----------------------------------------------------------------------------*/
void ubik2_ul_release(void *Handle);

#if defined(UL_STATISTIC)
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void ubik2_ul_statistic(void *ll_Handle, char *Buffer, unsigned int MaxBufferLen);
#endif/*--- #if defined(UL_STATISTIC) ---*/
#endif /* _UBIK2_UL_H_ */

