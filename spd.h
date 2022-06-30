/*
 * MemTest86+ V5.00 Specific code (GPL V2.0)
 * By Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.canardpc.com - http://www.memtest.org
 */

#define AMD_INDEX_IO_PORT	0xCD6
#define AMD_DATA_IO_PORT	0xCD7
#define AMD_SMBUS_BASE_REG	0x2C
#define AMD_PM_DECODE_EN_REG	0x00
 
void get_spd_spec(void);
int get_ddr2_module_size(int rank_density_byte, int rank_num_byte);
int get_ddr3_module_size(int sdram_capacity, int prim_bus_width, int sdram_width, int ranks);
int get_ddr4_module_size(int b4, int b6, int b12, int b13);
char* convert_hex_to_char(unsigned hex_org);
void sb800_get_smb(void);


