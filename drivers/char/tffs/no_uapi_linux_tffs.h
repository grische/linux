/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2004 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
\*------------------------------------------------------------------------------------------*/
#ifndef _tffs_h_
#define _tffs_h_

/*-----------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------*/
enum _tffs_id {
    /*-------------------------------------------------------------------------------------*\
        reserved ID's
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_FREE              = 0xFFFF,
    FLASH_FS_ID_SKIP              = 0x0000UL,
    FLASH_FS_ID_SEGMENT           = 0x0001,

    /*-------------------------------------------------------------------------------------*\
     * Minor ID's  (0x0001 - 0x00FF)
    \*-------------------------------------------------------------------------------------*/
        /*--- 0x0050 bis 0x005C ist reserviert ---*/
    FLASH_FS_ID_CRASH2_LOG        = 0x005D,   
    FLASH_FS_ID_PANIC2_LOG        = 0x005E,   
    FLASH_FS_ID_CRASH_LOG         = 0x005F,   
    FLASH_FS_ID_PANIC_LOG         = 0x0060,   
    FLASH_FS_ID_RC_RESERVED_USER  = 0x0061,     /*--- reserved for files needed in rc.user ---*/
    FLASH_FS_ID_RC_USER           = 0x0062,
    FLASH_FS_ID_TEL_DEFAULT       = 0x0063,     /*--- /var/flash/fx_def ---*/

    /*-------------------------------------------------------------------------------------*\
     * Beim Werkseinstellungen werden alle IDs von 0x64 - 0xFF gelöscht
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_TICFG             = 0x0064,
    /*--- FLASH_FS_ID_SHUTDOWN          = 0x0065, ---*/

    FLASH_FS_ID_PROVIDERADD_0     = 0x001D,   /*--- /var/flash/provider-additive/? (konfig des nachladbaren Internetanbieters, nicht durch WE geloescht) ---*/

    FLASH_FS_ID_PROV_CONFIG_0     = 0x001E,   /*--- /var/flash/provider-default/udhcpd.leases ---*/
    FLASH_FS_ID_CONFIG_0          = 0x0070,   /*--- /var/flash/udhcpd.leases ---*/
    FLASH_FS_ID_PROV_CONFIG_1     = 0x001F,   /*--- /var/flash/provider-default/ar7.cfg ---*/
    FLASH_FS_ID_CONFIG_1          = 0x0071,   /*--- /var/flash/ar7.cfg ---*/

    FLASH_FS_ID_CHRONY_DRIFT      = 0x0020,
    FLASH_FS_ID_CHRONY_RTC        = 0x0021,

    FLASH_FS_ID_PROV_CONFIG_2     = 0x0022,   /*--- /var/flash/provider-default/voip.cfg ---*/
    FLASH_FS_ID_CONFIG_2          = 0x0072,   /*--- /var/flash/voip.cfg ---*/
    FLASH_FS_ID_PROV_CONFIG_3     = 0x0023,   /*--- /var/flash/provider-default/wlan.cfg ---*/
    FLASH_FS_ID_CONFIG_3          = 0x0073,   /*--- /var/flash/wlan.cfg ---*/
    FLASH_FS_ID_PROV_CONFIG_4     = 0x0024,   /*--- /var/flash/provider-default/ar7stat.cfg ---*/
    FLASH_FS_ID_CONFIG_4          = 0x0074,   /*--- /var/flash/ar7stat.cfg ---*/
    FLASH_FS_ID_PROV_CONFIG_5     = 0x0025,   /*--- /var/flash/provider-default/net.update ---*/
    FLASH_FS_ID_CONFIG_5          = 0x0075,   /*--- /var/flash/net.update ---*/
    FLASH_FS_ID_PROV_CONFIG_6     = 0x0026,   /*--- /var/flash/provider-default/vpn.cfg ---*/
    FLASH_FS_ID_CONFIG_6          = 0x0076,   /*--- /var/flash/vpn.cfg ---*/
    FLASH_FS_ID_PROV_CONFIG_7     = 0x0027,   /*--- /var/flash/provider-default/tr069.cfg ---*/
    FLASH_FS_ID_CONFIG_7          = 0x0077,   /*--- /var/flash/tr069.cfg ---*/
    FLASH_FS_ID_PROV_CONFIG_8     = 0x0028,   /*--- /var/flash/provider-default/user.cfg ---*/      
    FLASH_FS_ID_CONFIG_8          = 0x0078,   /*--- /var/flash/user.cfg ---*/      
    FLASH_FS_ID_PROV_CONFIG_9     = 0x0029,   /*--- /var/flash/provider-default/userstat.cfg ---*/   
    FLASH_FS_ID_CONFIG_9          = 0x0079,   /*--- /var/flash/userstat.cfg ---*/   
    FLASH_FS_ID_PROV_CONFIG_10    = 0x002A,   /*--- /var/flash/provider-default/voip_call_stat ---*/
    FLASH_FS_ID_CONFIG_10         = 0x007A,   /*--- /var/flash/voip_call_stat ---*/
    FLASH_FS_ID_PROV_CONFIG_11    = 0x002B,   /*--- /var/flash/provider-default/rext.cfg ---*/
    FLASH_FS_ID_CONFIG_11         = 0x007B,   /*--- /var/flash/rext.cfg ---*/
    FLASH_FS_ID_PROV_CONFIG_12    = 0x002C,   /*--- /var/flash/provider-default/nlr.cfg ---*/
    FLASH_FS_ID_CONFIG_12         = 0x007C,   /*--- /var/flash/nlr.cfg ---*/
    FLASH_FS_ID_PROV_CONFIG_13    = 0x002D,   /*--- /var/flash/provider-default/pin ---*/
    FLASH_FS_ID_CONFIG_13         = 0x007D,   /*--- /var/flash/pin ---*/
    FLASH_FS_ID_PROV_CONFIG_14    = 0x002E,   /*--- /var/flash/provider-default/hcid.conf ---*/
    FLASH_FS_ID_CONFIG_14         = 0x007E,   /*--- /var/flash/hcid.conf ---*/
    FLASH_FS_ID_PROV_CONFIG_15    = 0x002F,   /*--- /var/flash/provider-default/link_key ---*/
    FLASH_FS_ID_CONFIG_15         = 0x007F,   /*--- /var/flash/link_key ---*/
    FLASH_FS_ID_PROV_CONFIG_16    = 0x001C,   /*--- /var/flash/provider-default/tr369.cfg ---*/
    FLASH_FS_ID_CONFIG_16         = 0x0092,   /*--- /var/flash/tr369.cfg ---*/

    /*-------------------------------------------------------------------------------------*\
     * Telefon Applikation
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_PROV_MSNLISTE     = 0x0030,   /*--- /var/flash/provider-default/telefon_msns ---*/
    FLASH_FS_ID_MSNLISTE          = 0x0080,   /*--- /var/flash/telefon_msns ---*/
    FLASH_FS_ID_PROV_TEL_CONFIG   = 0x0031,   /*--- /var/flash/provider-default/fx_conf ---*/
    FLASH_FS_ID_TEL_CONFIG        = 0x0081,   /*--- /var/flash/fx_conf ---*/
    FLASH_FS_ID_PROV_TEL_LCR      = 0x0032,   /*--- /var/flash/provider-default/fx_lcr ---*/
    FLASH_FS_ID_TEL_LCR           = 0x0082,   /*--- /var/flash/fx_lcr ---*/
    FLASH_FS_ID_PROV_TEL_MOH      = 0x0033,   /*--- /var/flash/provider-default/ansage_moh1 ---*/
    FLASH_FS_ID_TEL_MOH           = 0x0083,   /*--- /var/flash/ansage_moh1 ---*/
    FLASH_FS_ID_PROV_TEL_CG       = 0x0034,   /*--- /var/flash/provider-default/fx_cg ---*/
    FLASH_FS_ID_TEL_CG            = 0x0084,   /*--- /var/flash/fx_cg ---*/
    FLASH_FS_ID_PROV_TELCFG_MISC  = 0x0035,   /*--- /var/flash/provider-default/telefon_misc ---*/
    FLASH_FS_ID_TELCFG_MISC       = 0x0085,   /*--- /var/flash/telefon_misc ---*/

    FLASH_FS_ID_PROV_TEL_MOH2     = 0x0036,   /*--- /var/flash/provider-default/ansage_moh2 ---*/
    FLASH_FS_ID_TEL_MOH2          = 0x0086,   /*--- /var/flash/ansage_moh2 ---*/
    FLASH_FS_ID_PROV_TEL_NO_SERVICE = 0x0037,   /*--- /var/flash/provider-default/ansage_no_service ---*/
    FLASH_FS_ID_TEL_NO_SERVICE    = 0x0087,   /*--- /var/flash/ansage_no_service ---*/
    FLASH_FS_ID_PROV_TEL_NO_NUMBER = 0x0038,   /*--- /var/flash/provider-default/ansage_no_number ---*/
    FLASH_FS_ID_TEL_NO_NUMBER     = 0x0088,   /*--- /var/flash/ansage_no_number ---*/
    FLASH_FS_ID_PROV_TEL_USER1    = 0x0039,   /*--- /var/flash/provider-default/ansage_user ---*/
    FLASH_FS_ID_TEL_USER1         = 0x0089,   /*--- /var/flash/ansage_user ---*/
    FLASH_FS_ID_PROV_TEL_USER2    = 0x003A,   /*--- /var/flash/provider-default/ansage_user2 ---*/
    FLASH_FS_ID_TEL_USER2         = 0x008A,   /*--- /var/flash/ansage_user2 ---*/
    FLASH_FS_ID_PROV_TEL_USER3    = 0x003B,   /*--- /var/flash/provider-default/ansage_user3 ---*/
    FLASH_FS_ID_TEL_USER3         = 0x008B,   /*--- /var/flash/ansage_user3 ---*/
    FLASH_FS_ID_PROV_TEL_USER4    = 0x003C,   /*--- /var/flash/provider-default/ansage_user4 ---*/
    FLASH_FS_ID_TEL_USER4         = 0x008C,   /*--- /var/flash/ansage_user4 ---*/
    FLASH_FS_ID_PROV_TEL_CALLLOG  = 0x003D,   /*--- /var/flash/provider-default/calllog ---*/
    FLASH_FS_ID_TEL_CALLLOG       = 0x008D,   /*--- /var/flash/calllog ---*/
    FLASH_FS_ID_PROV_TEL_PHONEBOOK = 0x003E,   /*--- /var/flash/provider-default/phonebook ---*/
    FLASH_FS_ID_TEL_PHONEBOOK     = 0x008E,   /*--- /var/flash/phonebook ---*/
    FLASH_FS_ID_PROV_TEL_FONCTRL  = 0x003F,   /*--- /var/flash/provider-default/fonctrl ---*/
    FLASH_FS_ID_TEL_FONCTRL       = 0x008F,   /*--- /var/flash/fonctrl ---*/
    FLASH_FS_ID_TAM_CONFIG        = 0x0091,     /*--- /var/flash/tamconf ---*/

    /*-------------------------------------------------------------------------------------*\
     * Power Applikation
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_PROV_POWERMODE    = 0x0040,   /*--- /var/flash/provider-default/powermode ---*/
    FLASH_FS_ID_POWERMODE         = 0x0090,   /*--- /var/flash/powermode ---*/

    /*-------------------------------------------------------------------------------------*\
     * Audio/Aura/USB Applikation(s)
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_PROV_AURA_USB     = 0x0041,   /*--- /var/flash/provider-default/aura-usb ---*/
    FLASH_FS_ID_AURA_USB          = 0x00A0,   /*--- /var/flash/aura-usb ---*/
    /*-------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_PROV_CONFIGD      = 0x004F,   /* /var/flash/provider-default/configd */
    FLASH_FS_ID_CONFIGD           = 0x00A1,   /* /var/flash/configd */
    /*-------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_EWNWSTATUS        = 0x00A2,   /* /var/flash/ewnwstatus.cfg */
    /*-------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_FWUPDATETRACE     = 0x00A3,   /* /var/flash/fwupdatetrace.cfg */

    /*-------------------------------------------------------------------------------------*\
     * Fritz Mini || DOCSIS (/var/flash/docsis.nvram 0x0042)
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_PROV_BROWSER_DATA = 0x0042,   /*--- /var/flash/provider-default/browser-data ---*/
    FLASH_FS_ID_BROWSER_DATA      = 0x00A8,   /*--- /var/flash/browser-data ---*/
    FLASH_FS_ID_PROV_UPDATE_URL   = 0x0043,   /*--- /var/flash/provider-default/update-url.cfg ---*/
    FLASH_FS_ID_UPDATE_URL        = 0x00A9,   /*--- /var/flash/update-url.cfg ---*/

    /*-------------------------------------------------------------------------------------*\
     * DECT
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_PROV_DECT_MISC         = 0x44,    /*--- /var/flash/provider-default/dect_misc ---*/
    FLASH_FS_ID_DECT_MISC              = 0xB0,    /*--- /var/flash/dect_misc ---*/
    FLASH_FS_ID_PROV_DECT_EEPROM       = 0x45,    /*--- /var/flash/provider-default/dect_eeprom ---*/
    FLASH_FS_ID_DECT_EEPROM            = 0xB1,    /*--- /var/flash/dect_eeprom ---*/
    FLASH_FS_ID_PROV_DECT_HANDSET_USER = 0x46,    /*--- /var/flash/provider-default/dmgr_handset_user ---*/
    FLASH_FS_ID_DECT_HANDSET_USER      = 0xB2,    /*--- /var/flash/dmgr_handset_user ---*/

    /*-------------------------------------------------------------------------------------*\
     * Software Plugins
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_PLUGIN_GLOBAL     = 0xC0,    /*--- /var/flash/plugin-manager.ini ---*/
    FLASH_FS_ID_PLUGIN_1          = 0xC1,    /*--- /var/flash/plugin-.... ---*/
    FLASH_FS_ID_PLUGIN_2          = 0xC2,    /*--- /var/flash/plugin-.... ---*/
    FLASH_FS_ID_PLUGIN_3          = 0xC3,    /*--- /var/flash/plugin-.... ---*/
    FLASH_FS_ID_PLUGIN_4          = 0xC4,    /*--- /var/flash/plugin-.... ---*/
    FLASH_FS_ID_PLUGIN_5          = 0xC5,    /*--- /var/flash/plugin-.... ---*/
    FLASH_FS_ID_PLUGIN_6          = 0xC6,    /*--- /var/flash/plugin-.... ---*/
    FLASH_FS_ID_PLUGIN_7          = 0xC7,    /*--- /var/flash/plugin-.... ---*/
    FLASH_FS_ID_PLUGIN_8          = 0xC8,    /*--- /var/flash/plugin-.... ---*/

    /*-------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_PROV_TR064        = 0x47,    /*--- /var/flash/provider-default/websrv_ssl_key.pem  - private key for HTTPS-Fernzugang and TR-064-SSL ---*/
    FLASH_FS_ID_TR064             = 0xC9,    /*--- /var/flash/websrv_ssl_key.pem  - private key for HTTPS-Fernzugang and TR-064-SSL ---*/
    FLASH_FS_ID_PROV_ZERTIFIKAT   = 0x48,    /*--- /var/flash/provider-default/websrv_ssl_cert.pem - certificate for HTTPS-Fernzugang when using dyn dns name ---*/
    FLASH_FS_ID_ZERTIFIKAT        = 0xCA,    /*--- /var/flash/websrv_ssl_cert.pem - certificate for HTTPS-Fernzugang when using dyn dns name ---*/
    FLASH_FS_ID_LETSENCRYPT_KEY   = 0xCB,    /*--- /var/flash/letsencrypt_key.pem - private key for letsencrypt ---*/
    FLASH_FS_ID_LETSENCRYPT_CERT  = 0xCC,    /*--- /var/flash/letsencrypt_cert.pem - certificate for letsencrypt ---*/
    /*-------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_AVMNEXUS          = 0xCD,    /*--- /var/flash/avmnexus.cfg - config for avmnexusd ---*/
    /*-------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_MESHD             = 0xCE,    /*--- /var/flash/meshd.cfg - config for meshd ---*/
    
    /*-------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------*/
	FLASH_FS_ID_PROV_CERTCFG        = 0x49,   /* /var/flash/provider-default/cert.cfg */
	FLASH_FS_ID_CERTCFG             = 0xD0,   /* /var/flash/cert.cfg */
	FLASH_FS_ID_PROV_USBCFG         = 0x4A,   /* /var/flash/provider-default/usb.cfg */
	FLASH_FS_ID_USBCFG              = 0xD1,   /* /var/flash/usb.cfg */
	FLASH_FS_ID_PROV_XDSLMODE       = 0x4B,   /* /var/flash/provider-default/xdslmode */
	FLASH_FS_ID_XDSLMODE            = 0xD2,   /* /var/flash/xdslmode */
    FLASH_FS_ID_PROV_UMTSCFG        = 0x4C,   /* /var/flash/provider-default/umts.cfg */
    FLASH_FS_ID_UMTSCFG             = 0xD3,   /* /var/flash/umts.cfg */
    FLASH_FS_ID_PROV_MAILDXML       = 0x4D,   /* /var/flash/provider-default/maild.xml */
    FLASH_FS_ID_MAILDXML            = 0xD4,   /* /var/flash/maild.xml */
    FLASH_FS_ID_PROV_TIMEPROFILECFG = 0x4E,   /* /var/flash/provider-default/timeprofile.cfg */
    FLASH_FS_ID_TIMEPROFILECFG      = 0xD5,   /* /var/flash/timeprofile.cfg */
    FLASH_FS_ID_EVENT_LOG           = 0xD6,   /* /var/flash/saved_events */
    FLASH_FS_ID_FEATOVLCFG          = 0xD7,   /* /var/flash/featovl.cfg */
    FLASH_FS_ID_USBGSMCFG           = 0xD8,   /* /var/flash/usbgsm.cfg */
    FLASH_FS_ID_PLCPIB              = 0xD9,   /* /var/flash/plc.pib, Konfig fuer Produkte mit internem Powerline ohne eigenen Flash */
    FLASH_FS_ID_MODULE_MEMORY       = 0xDA,   /* /var/flash/modulemem, Modulspeicherinfo */
    FLASH_FS_ID_DVBCFG              = 0xDB,   /* /var/flash/dvb.cfg, Konfig fuer DVB: Senderliste und Konfig fuer TV */



    /*-------------------------------------------------------------------------------------*\
     * Home automation, 
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_SMART_METER_CFG     = 0xE0,   /* /var/flash/SmartMeterCfg, Konfiguration für EWTEL Projekt */
    /*-------------------------------------------------------------------------------------*\
     * Home automation, AHA 
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_AHACFG              = 0xE1,   /* /var/flash/aha.cfg, Allgemeine Konfig fuer AHA -Basis- */
    FLASH_FS_ID_AHAUSRCFG           = 0xE2,   /* /var/flash/ahausr.cfg, Konfig fuer Google-Kalender, Benutzerdaten -Networking- */
    FLASH_FS_ID_AHASTATCFG          = 0xE3,   /* /var/flash/ahastat.cfg, Konfig fuer Profilic, Statistik und Zeitschaltwerte -Basis- */
    FLASH_FS_ID_AHADECTCFG          = 0xE4,   /* /var/flash/ahadect.cfg, Konfig fuer DECT -Basis- */
    FLASH_FS_ID_AHANETCFG           = 0xE5,   /* /var/flash/ahanet.cfg, Konfig fuer Vernetzung auf AHA-Ebene -Basis- */
    FLASH_FS_ID_AHAGLOBALCFG        = 0xE6,   /* /var/flash/ahaglobal.cfg, Konfig/Speicher fuer globale Daten auf AHA-Ebene -Basis- */
    FLASH_FS_ID_AHAPUSHMAILCFG      = 0xE7,   /* /var/flash/ahapushmail.cfg, Konfiguration fuer Pushmail auf AHA-Ebene -Basis- */
    
    /*-------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------*/

    FLASH_FS_ID_FIRMWARE_CONFIG_LAST = 0x00FF,
#if defined(CONFIG_TFFS_ENV)
    /*-------------------------------------------------------------------------------------*\
     * Adam2 ID's  (0x0100 - 0x01FF)
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_HWREVISION           = 0x0100,
    FLASH_FS_PRODUCTID            = 0x0101,
    FLASH_FS_SERIALNUMBER         = 0x0102,
    FLASH_FS_DMC                  = 0x0103,
    FLASH_FS_HWSUBREVISION        = 0x0104,

    /*--- kleiner erster Buchstaben ---*/
    FLASH_FS_AUTOLOAD             = 0x0181,
    FLASH_FS_BOOTLOADERvERSION    = 0x0182,
    FLASH_FS_BOOTSERPORT          = 0x0183,
    FLASH_FS_MAC_BLUETOOTH        = 0x0184,
    FLASH_FS_CPUFREQUENCY         = 0x0185,
    FLASH_FS_FIRSTFREEADDRESS     = 0x0186,
    FLASH_FS_FLASHSIZE            = 0x0187,
    FLASH_FS_MAC_A                = 0x0188,
    FLASH_FS_MAC_B                = 0x0189,
    FLASH_FS_MAC_WLAN             = 0x018A,
    FLASH_FS_MAC_DSL              = 0x018B,
    FLASH_FS_MEMSIZE              = 0x018C,
    FLASH_FS_MODETTY0             = 0x018D,
    FLASH_FS_MODETTY1             = 0x018E,
    FLASH_FS_MY_IPADDRESS         = 0x018F,
    FLASH_FS_PROMPT               = 0x0190,
    FLASH_FS_MAC_RES              = 0x0191,
    FLASH_FS_REQ_FULLRATE_FREQ    = 0x0192,
    FLASH_FS_SYSFREQUENCY         = 0x0193,
    FLASH_FS_MAC_USB_BOARD        = 0x0194,
    FLASH_FS_MAC_USB_RNDIS        = 0x0195,
    FLASH_FS_MAC_WLAN2            = 0x0196,     /*--- war mal  FLASH_FS_MAC_C = 0x0196, ---*/
    FLASH_FS_ETHADDR              = 0x0197,
    FLASH_FS_LINUX_FS_START       = 0x0198,
    FLASH_FS_LINUXIP              = 0x0199,
    FLASH_FS_MODULATION           = 0x019A,
    FLASH_FS_NFS                  = 0x019B,
    FLASH_FS_NFSROOT              = 0x019C,
    FLASH_FS_OAM_LB_TIMEOUT       = 0x019D,
    FLASH_FS_SYSTYPE              = 0x019E,
    FLASH_FS_KERNEL_ARGS1         = 0x019F,
	FLASH_FS_KERNEL_ARGS          = 0x01A0,
    FLASH_FS_CRASH                = 0x01A1,
    FLASH_FS_USB_DEVICE_ID        = 0x01A2,
    FLASH_FS_USB_REVISION_ID      = 0x01A3,
    FLASH_FS_USB_DEVICE_NAME      = 0x01A4,
    FLASH_FS_USB_MANUFACTURER_NAME = 0x01A5,
    FLASH_FS_FIRMWARE_VERSION     = 0x01A6,
    FLASH_FS_LANGUAGE             = 0x01A7,
    FLASH_FS_COUNTRY              = 0x01A8,
    FLASH_FS_ANNEX                = 0x01A9,
    FLASH_FS_PTEST                = 0x01AA,
    FLASH_FS_WLAN_KEY             = 0x01AB,
    FLASH_FS_BLUETOOTH_KEY        = 0x01AC,
    FLASH_FS_HTTP_KEY             = 0x01AD,
    FLASH_FS_FIRMWARE_INFO        = 0x01AE,
    FLASH_FS_AUTO_MDIX            = 0x01AF,
    FLASH_FS_MTD_0                = 0x01B0,
    FLASH_FS_MTD_1                = 0x01B1,
    FLASH_FS_MTD_2                = 0x01B2,
    FLASH_FS_MTD_3                = 0x01B3,
    FLASH_FS_MTD_4                = 0x01B4,
    FLASH_FS_MTD_5                = 0x01B5,
    FLASH_FS_MTD_6                = 0x01B6,
    FLASH_FS_MTD_7                = 0x01B7,
    FLASH_FS_WLAN_CAL             = 0x01B8,
    FLASH_FS_JFFS2_SIZE           = 0x01B9,
    FLASH_FS_MTD_8                = 0x01BA,
    FLASH_FS_MTD_9                = 0x01BB,
    FLASH_FS_MTD_10               = 0x01BC,
    FLASH_FS_MTD_11               = 0x01BD,
    FLASH_FS_MTD_12               = 0x01BE,
    FLASH_FS_MTD_13               = 0x01BF,
    FLASH_FS_TR069_SERIAL         = 0x01C0,
    FLASH_FS_TR069_PASSPHRASE     = 0x01C1,  
    FLASH_FS_WEBGUI_PASS          = 0x01C2,
    FLASH_FS_PROVIDER_DEFAULT     = 0x01C3,
    FLASH_FS_MODULE_MEMORY        = 0x01C4,
    FLASH_FS_PLC_DAK_NMK          = 0x01C5,
    FLASH_FS_MTD_14               = 0x01C6,
    FLASH_FS_MTD_15               = 0x01C7,
    FLASH_FS_WLAN_SSID            = 0x01C8,

    FLASH_FS_URLADER_VERSION      = 0x01FD,
    FLASH_FS_TABLE_VERSION        = 0x01FE, /*--- eintrag in wirklichkeit nicht vorhanden, nur id reserviert ---*/
    FLASH_FS_NAME_TABLE           = 0x01FF,

    FLASH_FS_BB0                  = 0x0200,
    FLASH_FS_BB1                  = 0x0201,
    FLASH_FS_BB2                  = 0x0202,
    FLASH_FS_BB3                  = 0x0203,
    FLASH_FS_BB4                  = 0x0204,
    FLASH_FS_BB5                  = 0x0205,
    FLASH_FS_BB6                  = 0x0206,
    FLASH_FS_BB7                  = 0x0207,
    FLASH_FS_BB8                  = 0x0208,
    FLASH_FS_BB9                  = 0x0209,
    FLASH_FS_BB10                 = 0x020a,
    FLASH_FS_BB11                 = 0x020b,
    FLASH_FS_BB12                 = 0x020c,
    FLASH_FS_BB13                 = 0x020d,
    FLASH_FS_BB14                 = 0x020e,
    FLASH_FS_BB15                 = 0x020f,
    FLASH_FS_BB16                 = 0x0210,
    FLASH_FS_BB17                 = 0x0211,
    FLASH_FS_BB18                 = 0x0212,
    FLASH_FS_BB19                 = 0x0213,
    FLASH_FS_BB20                 = 0x0214,
    FLASH_FS_BB21                 = 0x0215,
    FLASH_FS_BB22                 = 0x0216,
    FLASH_FS_BB23                 = 0x0217,
    FLASH_FS_BB24                 = 0x0218,
    FLASH_FS_BB25                 = 0x0219,
    FLASH_FS_BB26                 = 0x021a,
    FLASH_FS_BB27                 = 0x021b,
    FLASH_FS_BB28                 = 0x021c,
    FLASH_FS_BB29                 = 0x021d,
    FLASH_FS_BB30                 = 0x021e,
    FLASH_FS_BB31                 = 0x021f,
#endif /*--- #if defined(CONFIG_TFFS_ENV) ---*/

    /*-------------------------------------------------------------------------------------*\
     * Zaehler
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_REBOOT_MAJOR         = 0x0400,
    FLASH_FS_REBOOT_MINOR         = 0x0401,
    FLASH_FS_HOUR_COUNTER         = 0x0402,
    FLASH_FS_DAY_COUNTER          = 0x0403,
    FLASH_FS_MOUNTH_COUNTER       = 0x0404,
    FLASH_FS_YEAR_COUNTER         = 0x0405,
    FLASH_FS_RESERVED_COUNTER     = 0x0406,
    FLASH_FS_VERSION_COUNTER      = 0x0407,

    /*-------------------------------------------------------------------------------------*\
     * AVM dropable data 
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_DROPABLE_DATA       = 0x4000,

    FLASH_FS_ID_ASSERT           = 0x4001,
    FLASH_FS_ID_ATM_JOURNAL      = 0x4002,

    /*-------------------------------------------------------------------------------------*\
    \*-------------------------------------------------------------------------------------*/
    FLASH_FS_ID_LAST   
};

/*------------------------------------------------------------------------------------------*\
 * kernel unterstuetzt provider defaults
\*------------------------------------------------------------------------------------------*/
#define TFFS_WITH_PROV_CONFIG
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline int provider_default_mapping(int id) {
    switch(id) {
        case FLASH_FS_ID_CONFIG_0: return FLASH_FS_ID_PROV_CONFIG_0;
        case FLASH_FS_ID_CONFIG_1: return FLASH_FS_ID_PROV_CONFIG_1;
        case FLASH_FS_ID_CONFIG_2: return FLASH_FS_ID_PROV_CONFIG_2;
        case FLASH_FS_ID_CONFIG_3: return FLASH_FS_ID_PROV_CONFIG_3;
        case FLASH_FS_ID_CONFIG_4: return FLASH_FS_ID_PROV_CONFIG_4;
        case FLASH_FS_ID_CONFIG_5: return FLASH_FS_ID_PROV_CONFIG_5;
        case FLASH_FS_ID_CONFIG_6: return FLASH_FS_ID_PROV_CONFIG_6;
        case FLASH_FS_ID_CONFIG_7: return FLASH_FS_ID_PROV_CONFIG_7;
        case FLASH_FS_ID_CONFIG_8: return FLASH_FS_ID_PROV_CONFIG_8;
        case FLASH_FS_ID_CONFIG_9: return FLASH_FS_ID_PROV_CONFIG_9;
        case FLASH_FS_ID_CONFIG_10: return FLASH_FS_ID_PROV_CONFIG_10;
        case FLASH_FS_ID_CONFIG_11: return FLASH_FS_ID_PROV_CONFIG_11;
        case FLASH_FS_ID_CONFIG_12: return FLASH_FS_ID_PROV_CONFIG_12;
        case FLASH_FS_ID_CONFIG_13: return FLASH_FS_ID_PROV_CONFIG_13;
        case FLASH_FS_ID_CONFIG_14: return FLASH_FS_ID_PROV_CONFIG_14;
        case FLASH_FS_ID_CONFIG_15: return FLASH_FS_ID_PROV_CONFIG_15;
        case FLASH_FS_ID_MSNLISTE: return FLASH_FS_ID_PROV_MSNLISTE;
        case FLASH_FS_ID_TEL_CONFIG: return FLASH_FS_ID_PROV_TEL_CONFIG;
        case FLASH_FS_ID_TEL_LCR: return FLASH_FS_ID_PROV_TEL_LCR;
        case FLASH_FS_ID_TEL_MOH: return FLASH_FS_ID_PROV_TEL_MOH;
        case FLASH_FS_ID_TEL_CG: return FLASH_FS_ID_PROV_TEL_CG;
        case FLASH_FS_ID_TELCFG_MISC: return FLASH_FS_ID_PROV_TELCFG_MISC;
        case FLASH_FS_ID_TEL_MOH2: return FLASH_FS_ID_PROV_TEL_MOH2;
        case FLASH_FS_ID_TEL_NO_SERVICE: return FLASH_FS_ID_PROV_TEL_NO_SERVICE;
        case FLASH_FS_ID_TEL_NO_NUMBER: return FLASH_FS_ID_PROV_TEL_NO_NUMBER;
        case FLASH_FS_ID_TEL_USER1: return FLASH_FS_ID_PROV_TEL_USER1;
        case FLASH_FS_ID_TEL_USER2: return FLASH_FS_ID_PROV_TEL_USER2;
        case FLASH_FS_ID_TEL_USER3: return FLASH_FS_ID_PROV_TEL_USER3;
        case FLASH_FS_ID_TEL_USER4: return FLASH_FS_ID_PROV_TEL_USER4;
        case FLASH_FS_ID_TEL_CALLLOG: return FLASH_FS_ID_PROV_TEL_CALLLOG;
        case FLASH_FS_ID_TEL_PHONEBOOK: return FLASH_FS_ID_PROV_TEL_PHONEBOOK;
        case FLASH_FS_ID_TEL_FONCTRL: return FLASH_FS_ID_PROV_TEL_FONCTRL;
        case FLASH_FS_ID_POWERMODE: return FLASH_FS_ID_PROV_POWERMODE;
        case FLASH_FS_ID_AURA_USB: return FLASH_FS_ID_PROV_AURA_USB;
        case FLASH_FS_ID_BROWSER_DATA: return FLASH_FS_ID_PROV_BROWSER_DATA;
        case FLASH_FS_ID_UPDATE_URL: return FLASH_FS_ID_PROV_UPDATE_URL;
        case FLASH_FS_ID_DECT_MISC: return FLASH_FS_ID_PROV_DECT_MISC;
        case FLASH_FS_ID_DECT_EEPROM: return FLASH_FS_ID_PROV_DECT_EEPROM;
        case FLASH_FS_ID_DECT_HANDSET_USER: return FLASH_FS_ID_PROV_DECT_HANDSET_USER;
        case FLASH_FS_ID_TR064: return FLASH_FS_ID_PROV_TR064;
        case FLASH_FS_ID_ZERTIFIKAT: return FLASH_FS_ID_PROV_ZERTIFIKAT;
        case FLASH_FS_ID_CERTCFG: return FLASH_FS_ID_PROV_CERTCFG;
        case FLASH_FS_ID_USBCFG: return FLASH_FS_ID_PROV_USBCFG;
        case FLASH_FS_ID_XDSLMODE: return FLASH_FS_ID_PROV_XDSLMODE;
        case FLASH_FS_ID_UMTSCFG: return FLASH_FS_ID_PROV_UMTSCFG;
        case FLASH_FS_ID_MAILDXML: return FLASH_FS_ID_PROV_MAILDXML;
        case FLASH_FS_ID_TIMEPROFILECFG: return FLASH_FS_ID_PROV_TIMEPROFILECFG;
        default: return 0;
    }
}
struct _TFFS_Name_Table {
    unsigned int id;
    char Name[64];
};

#if defined(TFFS_NAME_TABLE)
#define MAX_ENV_ENTRY           256

struct _TFFS_Name_Table TFFS_Name_Table[MAX_ENV_ENTRY];

#if defined(URLADER)
struct _TFFS_Name_Table_Init {
    unsigned int id;
    char *Name;
};
#endif /*--- #if defined(URLADER) ---*/

#if defined(URLADER)
struct _TFFS_Name_Table_Init T_Init[] =
#else /*--- #if defined(URLADER) ---*/
struct _TFFS_Name_Table T_Init[] =
#endif /*--- #else ---*/ /*--- #if defined(URLADER) ---*/
/*--- !!! alphabetisch sortiert -> !!! ---*/
{
    /*--- neuer Versionseintrag ---*/
    { FLASH_FS_TABLE_VERSION,     "@L" },

    /*--- grosser erster Buchstaben ---*/
    { FLASH_FS_AUTO_MDIX,         "AutoMDIX" },
    { FLASH_FS_DMC,               "DMC" },
    { FLASH_FS_HWREVISION,        "HWRevision" },
    { FLASH_FS_HWSUBREVISION,     "HWSubRevision" },
    { FLASH_FS_PRODUCTID,         "ProductID" },
    { FLASH_FS_SERIALNUMBER,      "SerialNumber" },

    /*--- kleiner erster Buchstaben ---*/
    { FLASH_FS_ANNEX,             "annex" },
    { FLASH_FS_AUTOLOAD,          "autoload" },
    { FLASH_FS_BB0,               "bb0" },
    { FLASH_FS_BB1,               "bb1" },
    { FLASH_FS_BB2,               "bb2" },
    { FLASH_FS_BB3,               "bb3" },
    { FLASH_FS_BB4,               "bb4" },
    { FLASH_FS_BB5,               "bb5" },
    { FLASH_FS_BB6,               "bb6" },
    { FLASH_FS_BB7,               "bb7" },
    { FLASH_FS_BB8,               "bb8" },
    { FLASH_FS_BB9,               "bb9" },
    { FLASH_FS_BOOTLOADERvERSION, "bootloaderVersion" },
    { FLASH_FS_BOOTSERPORT,       "bootserport" },
    { FLASH_FS_BLUETOOTH_KEY,     "bluetooth_key" },
    { FLASH_FS_MAC_BLUETOOTH,     "bluetooth" },
    { FLASH_FS_COUNTRY,           "country" },
    { FLASH_FS_CPUFREQUENCY,      "cpufrequency" },
    { FLASH_FS_CRASH,             "crash" },
    /*--- { FLASH_FS_ETHADDR,           "ethaddr" }, ---*/
    { FLASH_FS_FIRSTFREEADDRESS,  "firstfreeaddress" },
    { FLASH_FS_FIRMWARE_INFO,     "firmware_info" },
    { FLASH_FS_FIRMWARE_VERSION,  "firmware_version" },
    { FLASH_FS_FLASHSIZE,         "flashsize" },
    /*--- { FLASH_FS_HTTP_KEY,          "http_key" }, ---*/
    { FLASH_FS_JFFS2_SIZE,        "jffs2_size" },
    { FLASH_FS_KERNEL_ARGS,       "kernel_args" },
    { FLASH_FS_KERNEL_ARGS1,      "kernel_args1" },
    { FLASH_FS_LANGUAGE,          "language" },
    { FLASH_FS_LINUX_FS_START,    "linux_fs_start" },
    /*--- { FLASH_FS_LINUXIP,           "linuxip" }, ---*/
    { FLASH_FS_MAC_A,             "maca" },
    { FLASH_FS_MAC_B,             "macb" },
    /*--- { FLASH_FS_MAC_C,             "macc" }, ---*/
    { FLASH_FS_MAC_WLAN,          "macwlan" },
    { FLASH_FS_MAC_WLAN2,         "macwlan2" },
    { FLASH_FS_MAC_DSL,           "macdsl" },
    { FLASH_FS_MEMSIZE,           "memsize" },
    { FLASH_FS_MODETTY0,          "modetty0" },
    { FLASH_FS_MODETTY1,          "modetty1" },
    /*--- { FLASH_FS_MODULATION,        "modulation" }, ---*/
    { FLASH_FS_MODULE_MEMORY,     "modulemem" },
    { FLASH_FS_MTD_0,             "mtd0" },
    { FLASH_FS_MTD_1,             "mtd1" },
    { FLASH_FS_MTD_2,             "mtd2" },
    { FLASH_FS_MTD_3,             "mtd3" },
    { FLASH_FS_MTD_4,             "mtd4" },
    { FLASH_FS_MTD_5,             "mtd5" },
    { FLASH_FS_MTD_6,             "mtd6" },
    { FLASH_FS_MTD_7,             "mtd7" },
    { FLASH_FS_MTD_8,             "mtd8" },
    { FLASH_FS_MTD_9,             "mtd9" },
    { FLASH_FS_MTD_10,            "mtd10" },
    { FLASH_FS_MTD_11,            "mtd11" },
    { FLASH_FS_MTD_12,            "mtd12" },
    { FLASH_FS_MTD_13,            "mtd13" },
    { FLASH_FS_MTD_14,            "mtd14" },
    { FLASH_FS_MTD_15,            "mtd15" },
    { FLASH_FS_MY_IPADDRESS,      "my_ipaddress" },
    /*--- { FLASH_FS_NFS,               "nfs" }, ---*/
    /*--- { FLASH_FS_NFSROOT,           "nfsroot" }, ---*/
    /*--- { FLASH_FS_OAM_LB_TIMEOUT,    "oam_lb_timeout" }, ---*/
    { FLASH_FS_PLC_DAK_NMK,       "plc_dak_nmk" },
    { FLASH_FS_PROMPT,            "prompt" },
    { FLASH_FS_PROVIDER_DEFAULT,  "provider" },
    { FLASH_FS_PTEST,             "ptest" },
    { FLASH_FS_MAC_RES,           "reserved" },
    { FLASH_FS_REQ_FULLRATE_FREQ, "req_fullrate_freq" },
    { FLASH_FS_SYSFREQUENCY,      "sysfrequency" },
    /*--- { FLASH_FS_SYSTYPE,           "systype" }, ---*/
    { FLASH_FS_TR069_PASSPHRASE,  "tr069_passphrase" },
    { FLASH_FS_TR069_SERIAL,      "tr069_serial" },
    { FLASH_FS_URLADER_VERSION,   "urlader-version" },
    { FLASH_FS_MAC_USB_BOARD,     "usb_board_mac" },
    { FLASH_FS_USB_DEVICE_ID,     "usb_device_id" },
    { FLASH_FS_USB_DEVICE_NAME,   "usb_device_name" },
    { FLASH_FS_USB_MANUFACTURER_NAME,   "usb_manufacturer_name" },
    { FLASH_FS_USB_REVISION_ID,   "usb_revision_id" },
    { FLASH_FS_MAC_USB_RNDIS,     "usb_rndis_mac" },
    { FLASH_FS_WEBGUI_PASS,       "webgui_pass" },
    { FLASH_FS_WLAN_CAL,          "wlan_cal" },
    { FLASH_FS_WLAN_KEY,          "wlan_key" },
    { FLASH_FS_WLAN_SSID,         "wlan_ssid" },

    /*--- ende ---*/

    { 0, "zuende" }
};
/*--- !!! <- alphabetisch sortiert !!!!! ---*/
#if !defined(URLADER)
static struct _TFFS_Name_Table *current_nametable = T_Init;
#endif // !defined(URLADER)
#endif // TFFS_NAME_TABLE


/*-----------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------*/
struct _tffs_cmd {
    enum _tffs_id id;
    unsigned int size;
    unsigned int status;
    void *buffer;
};

/*-----------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------*/
#if defined(__KERNEL__) || defined(URLADER) || defined(FLASH_TFFS_IMAGE)
struct _TFFS_Entry {
    unsigned short ID;
    unsigned short Length;
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
union _tffs_segment_entry {
    struct _TFFS_Entry Entry;
    unsigned char Buffer[sizeof(struct _TFFS_Entry) + sizeof(unsigned int)];
};

#define TFFS_GET_SEGMENT_VALUE(E)  ~( \
                                        ((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 3]) <<  0) | \
                                        ((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 2]) <<  8) | \
                                        ((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 1]) << 16) | \
                                        ((unsigned int)((E)->Buffer[sizeof(struct _TFFS_Entry) + 0]) << 24)   \
                                    )

#define TFFS_SET_SEGMENT_VALUE(E, value)   ( \
                                         (E)->Buffer[sizeof(struct _TFFS_Entry) + 3] = ((~(value) >>  0) & 0xFF), \
                                         (E)->Buffer[sizeof(struct _TFFS_Entry) + 2] = ((~(value) >>  8) & 0xFF), \
                                         (E)->Buffer[sizeof(struct _TFFS_Entry) + 1] = ((~(value) >> 16) & 0xFF), \
                                         (E)->Buffer[sizeof(struct _TFFS_Entry) + 0] = ((~(value) >> 24) & 0xFF)  \
                                    )

#endif /*--- #if defined(__KERNEL__) || defined(URLADER) ---*/


/*-----------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------*/
#if !defined(URLADER)
#define _TFFS_READ_ID           0x01
#define _TFFS_WRITE_ID          0x02
#define _TFFS_FORMAT            0x03
#define _TFFS_CLEAR_ID          0x04
#define _TFFS_REINDEX           0x05
#define _TFFS_CLEANUP           0x06
#define _TFFS_INFO              0x07
#define _TFFS_WERKSEINSTELLUNG  0x08

#define _TFFS_TYPE_RAW          0x00

#include <asm/ioctl.h>

#define TFFS_READ_ID            _IOC(_IOC_READ | _IOC_WRITE, _TFFS_TYPE_RAW,         _TFFS_READ_ID, sizeof(struct _tffs_cmd))
#define TFFS_WRITE_ID           _IOC(_IOC_READ | _IOC_WRITE, _TFFS_TYPE_RAW,         _TFFS_WRITE_ID, sizeof(struct _tffs_cmd))
#define TFFS_FORMAT             _IOC(_IOC_READ | _IOC_WRITE, _TFFS_TYPE_RAW,         _TFFS_FORMAT, sizeof(struct _tffs_cmd))
#define TFFS_CLEAR_ID           _IOC(_IOC_READ | _IOC_WRITE, _TFFS_TYPE_RAW,         _TFFS_CLEAR_ID, sizeof(struct _tffs_cmd))
#define TFFS_REINDEX            _IOC(_IOC_READ | _IOC_WRITE, _TFFS_TYPE_RAW,         _TFFS_REINDEX, sizeof(struct _tffs_cmd))
#define TFFS_CLEANUP            _IOC(_IOC_READ | _IOC_WRITE, _TFFS_TYPE_RAW,         _TFFS_CLEANUP, sizeof(struct _tffs_cmd))
#define TFFS_INFO               _IOC(_IOC_READ | _IOC_WRITE, _TFFS_TYPE_RAW,         _TFFS_INFO, sizeof(struct _tffs_cmd))
#define TFFS_WERKSEINSTELLUNG   _IOC(_IOC_READ | _IOC_WRITE, _TFFS_WERKSEINSTELLUNG, _TFFS_INFO, sizeof(struct _tffs_cmd))

#define MAX_MINOR_100_BLOCK_SIZE        (32 * 1024)

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#if defined(__KERNEL__)

#include <linux/types.h>

#if defined(CONFIG_TFFS)

struct mtd_info;
extern unsigned int TFFS_Init(unsigned int, unsigned int);
extern void TFFS_Deinit(void);
extern void *TFFS_Open(void);
extern void TFFS_Close(void *);
extern unsigned int TFFS_Werkseinstellungen(void *);
extern unsigned int TFFS_Minor(void *handle);
extern unsigned int TFFS_Clear(void *, enum _tffs_id);
extern unsigned int TFFS_Write(void *, enum _tffs_id, unsigned char *, unsigned int, unsigned int);
extern unsigned int TFFS_Read(void *, enum _tffs_id, unsigned char *, unsigned int *);
extern unsigned int TFFS_Create_Index(void);
extern unsigned int TFFS_Cleanup(void *);
extern unsigned int TFFS_Info(void *, unsigned int *);
extern unsigned char *TFFS_Read_Buffer(void *, unsigned int **);
extern unsigned char *TFFS_Write_Buffer(void *, unsigned int **);
extern unsigned int tffs_panic_log_suppress;
void tffs_panic_log_register_spi(void);
extern void tffs_send_event(unsigned int event);

struct mtd_info;
struct mtd_oob_ops;

enum tffs3_handle_mode {
    tffs3_mode_read,
    tffs3_mode_write,
    tffs3_mode_panic,
};

enum tffs3_types {
    tffs3_type_legacy,
    tffs3_type_mtdnor,
    tffs3_type_bdev,
    tffs3_type_mtdnand,
    tffs3_type_cache,
    tffs3_type_remote,
    tffs3_type_last,
};

struct tffs_core_handle {
    void *core_priv;
    unsigned int id;
    enum tffs3_handle_mode mode;
    size_t max_segment_size;
};

typedef struct mtd_info *(*panic_setup_cb)(struct mtd_info *mtd);

extern void TFFS3_Backend_Ready(void);
extern int TFFS3_Register_NAND(struct mtd_info *mtd);
extern int TFFS3_Register_REMOTE(unsigned int node_id);
extern int TFFS3_Register_SERVER(unsigned int node_id);
extern int TFFS3_Register_Panic_CB(struct mtd_info *mtd, panic_setup_cb panic_setup_fn);
extern struct tffs_core_handle *TFFS3_Open(enum _tffs_id, enum tffs3_handle_mode);
extern int TFFS3_Close(struct tffs_core_handle *);
extern int TFFS3_Read(struct tffs_core_handle *, unsigned char *, unsigned int *);
extern int TFFS3_Write(struct tffs_core_handle *, unsigned char *, unsigned int, unsigned int);
#endif /*--- #if defined(CONFIG_TFFS) ---*/

#define TFFS_EVENT_BIT_TRIGGER  (BITS_PER_LONG - 1)
#define TFFS_EVENT_BIT_CLEANUP  0
#define TFFS_EVENT_BIT_PANIC    1
#define TFFS_EVENT_TRIGGER      (1 << TFFS_EVENT_BIT_TRIGGER)
#define TFFS_EVENT_CLEANUP      (1 << TFFS_EVENT_BIT_CLEANUP)
#define TFFS_EVENT_PANIC        (1 << TFFS_EVENT_BIT_PANIC)

#define TFFS_VERSION(major, minor) ((((major) & 0xffff) << 16) + ((minor) & 0xffff))
#define TFFS3_TYPE_MTDNOR       0x1
#define TFFS3_TYPE_MTDNAND      0x2
#define TFFS3_TYPE_BDEV         0x3
#define TFFS3_TYPE_CACHE        0x4
#define TFFS3_TYPE_REMOTE       0x5
#define TFFS3_TYPE_LEGACY       0x6

#endif /*--- #if defined(__KERNEL__) ---*/
#endif /*--- #if !defined(URLADER) ---*/

#endif /*--- #ifndef _tffs_h_ ---*/
