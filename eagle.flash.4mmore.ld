/* Flash Split for 4M chips - Dual LittleFS Partitions for OTA Protection */
/* 
 * Flash Memory Map (4MB ESP8266):
 * ================================
 * Sketch (Firmware)  : 0x40200000 - 0x40389000 (~1575KB) (1613824B)
 * OTA (Firmware)     : 0x40389000 - 0x40512000 (~1575KB) (1613824B)
 * CONFIG Partition   : 0x40510000 - 0x40520000 (64KB)     - Preferences storage (survives OTA)
 * MAIN_FS Partition  : 0x40520000 - 0x405F3000 (~844KB)   - Web assets/logs (gets updated via OTA)
 * EEPROM             : 0x405F3000 - 0x405F4000 (4KB)      - Traditional EEPROM (reduced)
 * RF Calibration     : 0x405FB000 - 0x405FC000 (4KB)      - RF calibration data
 * WiFi Config        : 0x405FD000 - 0x40600000 (12KB)     - WiFi configuration
 * 
 * Key Design:
 * - CONFIG partition stores Preferences data and survives filesystem OTA updates
 * - MAIN_FS partition contains web interface files and can be safely updated via OTA
 * - Preferences library is configured to use CONFIG partition instead of MAIN_FS
 * - EEPROM reduced to 4KB (from 16KB) to make room for CONFIG partition
 */

MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  irom0_0_seg :                         org = 0x40201010, len = 0x187ff0
}

/* CONFIG partition - 64KB for Preferences (survives OTA filesystem updates) */
PROVIDE ( _CONFIG_start = 0x40510000 );
PROVIDE ( _CONFIG_end = 0x40520000 );
PROVIDE ( _CONFIG_page = 0x100 );
PROVIDE ( _CONFIG_block = 0x2000 );

/* MAIN_FS partition - ~844KB for web assets and logs (gets updated via OTA) */
PROVIDE ( _FS_start = 0x40520000 );
PROVIDE ( _FS_end = 0x405F3000 );
PROVIDE ( _FS_page = 0x100 );
PROVIDE ( _FS_block = 0x2000 );

/* EEPROM - reduced to 4KB (Preferences now use CONFIG partition) */
PROVIDE ( _EEPROM_start = 0x405F3000 );

/* The following symbols are DEPRECATED and will be REMOVED in a future release */
PROVIDE ( _SPIFFS_start = 0x40520000 );
PROVIDE ( _SPIFFS_end = 0x405F3000 );
PROVIDE ( _SPIFFS_page = 0x100 );
PROVIDE ( _SPIFFS_block = 0x2000 );

INCLUDE "local.eagle.app.v6.common.ld"
