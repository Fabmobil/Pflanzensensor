/* Flash Split for 4M chips - Custom: Dual OTA partitions + 1MB LittleFS */
/* sketch @0x40200000 (~1575KB) (1613824B) */
/* ota    @0x40389000 (~1575KB) (1613824B) */
/* spiffs @0x40512000 (~916KB) (937984B) - reduced to expand EEPROM */
/* eeprom @0x405F7000 (16KB) - increased from 4KB to fit all sensor config */
/* rfcal  @0x405FB000 (4KB) */
/* wifi   @0x405FD000 (12KB) */

MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  irom0_0_seg :                         org = 0x40201010, len = 0x187ff0
}

PROVIDE ( _FS_start = 0x40512000 );
PROVIDE ( _FS_end = 0x405F7000 );
PROVIDE ( _FS_page = 0x100 );
PROVIDE ( _FS_block = 0x2000 );
PROVIDE ( _EEPROM_start = 0x405f7000 );
/* The following symbols are DEPRECATED and will be REMOVED in a future release */
PROVIDE ( _SPIFFS_start = 0x40512000 );
PROVIDE ( _SPIFFS_end = 0x405F7000 );
PROVIDE ( _SPIFFS_page = 0x100 );
PROVIDE ( _SPIFFS_block = 0x2000 );

INCLUDE "local.eagle.app.v6.common.ld"
