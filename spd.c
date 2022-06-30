/*
 * MemTest86+ V5 Specific code (GPL V2.0)
 * By Samuel DEMEULEMEESTER, sdemeule@memtest.org
 * http://www.canardpc.com - http://www.memtest.org
 */

#include "stdint.h"
#include "test.h"
#include "io.h"
#include "pci.h"
#include "msr.h"
#include "spd.h"
#include "screen_buffer.h"
#include "jedec_id.h"

#define NULL 0

#define SMBHSTSTS smbusbase
#define SMBHSTCNT smbusbase + 2
#define SMBHSTCMD smbusbase + 3
#define SMBHSTADD smbusbase + 4
#define SMBHSTDAT smbusbase + 5

extern void wait_keyup();

int smbdev, smbfun;
unsigned short smbusbase;
unsigned char spd_raw[256];
char s[] = {'/', 0, '-', 0, '\\', 0, '|', 0};

static void ich5_get_smb(void)
{
    unsigned long x;
    int result;
    result = pci_conf_read(0, smbdev, smbfun, 0x20, 2, &x);
    if (result == 0) smbusbase = (unsigned short) x & 0xFFFE;
}

static void piix4_get_smb(void)
{
    unsigned long x;
    int result;

    result = pci_conf_read(0, smbdev, smbfun, 0x08, 1, &x);

    if(x < 0x40){
 			// SB600/700
 			result = pci_conf_read(0, smbdev, smbfun, 0x90, 2, &x);
  		if (result == 0) smbusbase = (unsigned short) x & 0xFFFE;
  	} else {
  		// SB800
			sb800_get_smb();
  	}
}

void sb800_get_smb(void)
{
	int lbyte, hbyte, result;
	unsigned long x;

	result = pci_conf_read(0, smbdev, smbfun, 0x08, 1, &x);

	/* if processor revision is ML_A0 or ML_A1 use different way for SMBus
	 * IO base calculation */
	if (x == 0x42 || x == 0x41) {
		/* read PMx00+1 to get SmbusAsfIoBase */
		__outb(AMD_PM_DECODE_EN_REG+1, AMD_INDEX_IO_PORT);
		lbyte = __inb(AMD_DATA_IO_PORT);

		/* SMBus IO base is defined as {Smbus0AsfIoBase[7:0], 0x00} */
		smbusbase = (lbyte & 0xF) << 8;
	} else {
		__outb(AMD_SMBUS_BASE_REG + 1, AMD_INDEX_IO_PORT);
		lbyte = __inb(AMD_DATA_IO_PORT);
		__outb(AMD_SMBUS_BASE_REG, AMD_INDEX_IO_PORT);
		hbyte = __inb(AMD_DATA_IO_PORT);

		smbusbase = lbyte;
		smbusbase <<= 8;
		smbusbase += hbyte;
		smbusbase &= 0xFFE0;
	}

	if (smbusbase == 0xFFE0)	{ smbusbase = 0; }
}

unsigned char ich5_smb_read_byte(unsigned char adr, unsigned char cmd)
{
    int l1, h1, l2, h2;
    uint64_t t;
    uint64_t toval = 10 * v->clks_msec;

    __outb(0x1f, SMBHSTSTS);			// reset SMBus Controller
    __outb(0xff, SMBHSTDAT);
    while(__inb(SMBHSTSTS) & 0x01);		// wait until ready
    __outb(cmd, SMBHSTCMD);
    __outb((adr << 1) | 0x01, SMBHSTADD);
    __outb(0x48, SMBHSTCNT);
    rdtsc(l1, h1);
    //cprint(POP2_Y, POP2_X + 16, s + cmd % 8);	// progress bar
    while (!(__inb(SMBHSTSTS) & 0x02)) {	// wait til command finished
			rdtsc(l2, h2);
			t = ((uint64_t)(h2 - h1) * 0xffffffff + (l2 - l1));
			if (t > toval) break;			// break after 10ms
    }
    return __inb(SMBHSTDAT);
}

static int ich5_read_spd(int dimmadr)
{
    int x;
    spd_raw[0] = ich5_smb_read_byte(0x50 + dimmadr, 0);
    if (spd_raw[0] == 0xff)	return -1;		// no spd here
    for (x = 1; x < 256; x++) {
			spd_raw[x] = ich5_smb_read_byte(0x50 + dimmadr, (unsigned char) x);
    }
    return 0;
}

static void us15w_get_smb(void)
{
    unsigned long x;
    int result;
    result = pci_conf_read(0, 0x1f, 0, 0x40, 2, &x);
    if (result == 0) smbusbase = (unsigned short) x & 0xFFC0;
}

unsigned char us15w_smb_read_byte(unsigned char adr, unsigned char cmd)
{
    int l1, h1, l2, h2;
    uint64_t t;
    uint64_t toval = 10 * v->clks_msec;

    //__outb(0x00, smbusbase + 1);			// reset SMBus Controller
    //__outb(0x00, smbusbase + 6);
    //while((__inb(smbusbase + 1) & 0x08) != 0);		// wait until ready
    __outb(0x02, smbusbase + 0);    // Byte read
    __outb(cmd, smbusbase + 5);     // Command
    __outb(0x07, smbusbase + 1);    // Clear status
    __outb((adr << 1) | 0x01, smbusbase + 4);   // DIMM address
    __outb(0x12, smbusbase + 0);    // Start
    //while (((__inb(smbusbase + 1) & 0x08) == 0)) {}	// wait til busy
    rdtsc(l1, h1);
    cprint(POP2_Y, POP2_X + 16, s + cmd % 8);	// progress bar
    while (((__inb(smbusbase + 1) & 0x01) == 0) ||
		((__inb(smbusbase + 1) & 0x08) != 0)) {	// wait til command finished
	rdtsc(l2, h2);
	t = ((uint64_t)(h2 - h1) * 0xffffffff + (l2 - l1));
	if (t > toval) break;			// break after 10ms
    }
    return __inb(smbusbase + 6);
}

static int us15w_read_spd(int dimmadr)
{
    int x;
    spd_raw[0] = us15w_smb_read_byte(0x50 + dimmadr, 0);
    if (spd_raw[0] == 0xff)	return -1;		// no spd here
    for (x = 1; x < 256; x++) {
	spd_raw[x] = us15w_smb_read_byte(0x50 + dimmadr, (unsigned char) x);
    }
    return 0;
}

struct pci_smbus_controller {
    unsigned vendor;
    unsigned device;
    char *name;
    void (*get_adr)(void);
    int (*read_spd)(int dimmadr);
};

static struct pci_smbus_controller smbcontrollers[] = {
     // Intel SMBUS
     {0x8086, 0x2413, "Intel 82801AA (ICH)",    ich5_get_smb, ich5_read_spd},
     {0x8086, 0x2423, "Intel 82801AB (ICH0)",   ich5_get_smb, ich5_read_spd},
     {0x8086, 0x2443, "Intel 82801BA (ICH2)",   ich5_get_smb, ich5_read_spd},
     {0x8086, 0x2483, "Intel 82801CA (ICH3)",   ich5_get_smb, ich5_read_spd},
     {0x8086, 0x24C3, "Intel 82801DB (ICH4)",   ich5_get_smb, ich5_read_spd},
     {0x8086, 0x24D3, "Intel 82801E (ICH5)",    ich5_get_smb, ich5_read_spd},
     {0x8086, 0x25A4, "Intel 6300ESB",          ich5_get_smb, ich5_read_spd},
     {0x8086, 0x266A, "Intel 82801F (ICH6)",    ich5_get_smb, ich5_read_spd},
     {0x8086, 0x269B, "Intel 6310ESB/6320ESB",  ich5_get_smb, ich5_read_spd},
     {0x8086, 0x27DA, "Intel 82801G (ICH7)",    ich5_get_smb, ich5_read_spd},
     {0x8086, 0x283E, "Intel 82801H (ICH8)",    ich5_get_smb, ich5_read_spd},
     {0x8086, 0x2930, "Intel 82801I (ICH9)",    ich5_get_smb, ich5_read_spd},
     {0x8086, 0x5032, "Intel EP80579",          ich5_get_smb, ich5_read_spd},
     {0x8086, 0x3A30, "Intel ICH10R",           ich5_get_smb, ich5_read_spd},
     {0x8086, 0x3A60, "Intel ICH10B",           ich5_get_smb, ich5_read_spd},
     {0x8086, 0x3B30, "Intel P55",              ich5_get_smb, ich5_read_spd},
     {0x8086, 0x1C22, "Intel P67",              ich5_get_smb, ich5_read_spd},
     {0x8086, 0x1D22, "Intel C600/X79",         ich5_get_smb, ich5_read_spd},
     {0x8086, 0x1D70, "Intel C600/X79 B0",      ich5_get_smb, ich5_read_spd},
     {0x8086, 0x1D71, "Intel C600/X79 B1",      ich5_get_smb, ich5_read_spd},
     {0x8086, 0x1D72, "Intel C600/X79 B2",      ich5_get_smb, ich5_read_spd},
     {0x8086, 0x2330, "Intel DH89xxCC",         ich5_get_smb, ich5_read_spd},
     {0x8086, 0x1E22, "Intel Z77",              ich5_get_smb, ich5_read_spd},
     {0x8086, 0x8C22, "Intel HSW",              ich5_get_smb, ich5_read_spd},
     {0x8086, 0x9C22, "Intel HSW-ULT",          ich5_get_smb, ich5_read_spd},
     {0x8086, 0x1F3C, "Intel Atom C2000",       ich5_get_smb, ich5_read_spd},
     {0x8086, 0x8D22, "Intel C610/X99",         ich5_get_smb, ich5_read_spd},
     {0x8086, 0x8D7D, "Intel C610/X99 B0",      ich5_get_smb, ich5_read_spd},
     {0x8086, 0x8D7E, "Intel C610/X99 B1",      ich5_get_smb, ich5_read_spd},
     {0x8086, 0x8D7F, "Intel C610/X99 B2",      ich5_get_smb, ich5_read_spd},
     {0x8086, 0x23B0, "Intel DH895XCC",         ich5_get_smb, ich5_read_spd},
     {0x8086, 0x8CA2, "Intel Wildcat Point",    ich5_get_smb, ich5_read_spd},
     {0x8086, 0x9CA2, "Intel Wildcat Point-LP", ich5_get_smb, ich5_read_spd},
     {0x8086, 0x0F12, "Intel BayTrail",         ich5_get_smb, ich5_read_spd},
     {0x8086, 0x2292, "Intel Braswell",         ich5_get_smb, ich5_read_spd},
     {0x8086, 0xA123, "Intel Sunrise Point-H",  ich5_get_smb, ich5_read_spd},
     {0x8086, 0x9D23, "Intel Sunrise Point-LP", ich5_get_smb, ich5_read_spd},
     {0x8086, 0x19DF, "Intel Atom C3000",       ich5_get_smb, ich5_read_spd},
     {0x8086, 0x1BC9, "Intel Emmitsburg",       ich5_get_smb, ich5_read_spd},
     {0x8086, 0x5AD4, "Intel Broxton",          ich5_get_smb, ich5_read_spd},
     {0x8086, 0xA1A3, "Intel C620",             ich5_get_smb, ich5_read_spd},
     {0x8086, 0xA223, "Intel Lewisburg",        ich5_get_smb, ich5_read_spd},
     {0x8086, 0xA2A3, "Intel 200/Z370",         ich5_get_smb, ich5_read_spd},
     {0x8086, 0x31D4, "Intel Gemini Lake",      ich5_get_smb, ich5_read_spd},
     {0x8086, 0xA323, "Intel Cannon Lake-H",    ich5_get_smb, ich5_read_spd},
     {0x8086, 0x9DA3, "Intel Cannon Lake-LP",   ich5_get_smb, ich5_read_spd},
     {0x8086, 0x18DF, "Intel Cedar Fork",       ich5_get_smb, ich5_read_spd},
     {0x8086, 0x34A3, "Intel Ice Lake-LP",      ich5_get_smb, ich5_read_spd},
     {0x8086, 0x38A3, "Intel Ice Lake-N",       ich5_get_smb, ich5_read_spd},
     {0x8086, 0x02A3, "Intel Comet Lake",       ich5_get_smb, ich5_read_spd},
     {0x8086, 0x06A3, "Intel Comet Lake-H",     ich5_get_smb, ich5_read_spd},
     {0x8086, 0x4B23, "Intel Elkhart Lake",     ich5_get_smb, ich5_read_spd},
     {0x8086, 0xA0A3, "Intel Tiger Lake-LP",    ich5_get_smb, ich5_read_spd},
     {0x8086, 0x43A3, "Intel Tiger Lake-H",     ich5_get_smb, ich5_read_spd},
     {0x8086, 0x4DA3, "Intel Jasper Lake",      ich5_get_smb, ich5_read_spd},
     {0x8086, 0xA3A3, "Intel Comet Lake-V",     ich5_get_smb, ich5_read_spd},
     {0x8086, 0x7AA3, "Intel Alder Lake-S",     ich5_get_smb, ich5_read_spd},
     {0x8086, 0x51A3, "Intel Alder Lake-P",     ich5_get_smb, ich5_read_spd},
     {0x8086, 0x54A3, "Intel Alder Lake-M",     ich5_get_smb, ich5_read_spd},

     {0x8086, 0x8119, "Intel US15W",            us15w_get_smb, us15w_read_spd},

     // AMD SMBUS
     {0x1002, 0x4385, "AMD SB600/700",          piix4_get_smb, ich5_read_spd},
     {0x1022, 0x780B, "AMD SB800/900",          sb800_get_smb, ich5_read_spd},

     {0, 0, "", NULL, NULL}

     // NOTE: Linux PCI IDs list / web site and Linux source code (drivers/i2c/busses/i2c*.c)
     // contain other SMBus controller models from AMD / Hygon, SiS, Nvidia, Intel
     // not supported by this code.
/*
     1002:4353 AMD SB200 SMBus Controller
     1002:4363 AMD SB300 SMBus Controller
     1002:4372 AMD IXP SB4x0 SMBus Controller

     1022:13e7 AMD Ariel SMBus Controller
     1022:746a AMD AMD-8111 SMBus 2.0
     1022:790b AMD FCH SMBus Controller

     1039:0016 Silicon Integrated Systems [SiS] SiS961/2/3 SMBus controller

     10de:0034 Nvidia MCP04 SMBus
     10de:0052 Nvidia CK804 SMBus
     10de:0064 Nvidia nForce2 SMBus (MCP)
     10de:0084 Nvidia MCP2A SMBus
     10de:00d4 Nvidia nForce3 SMBus

     10de:0264 Nvidia MCP51 SMBus
     10de:0368 Nvidia MCP55 SMBus Controller
     10de:03eb Nvidia MCP61 SMBus
     10de:0446 Nvidia MCP65 SMBus
     10de:0542 Nvidia MCP67 SMBus

    (10de:0752 Nvidia MCP78S [GeForce 8200] SMBus - maybe irrelevant)
     10de:07d8 Nvidia MCP73 SMBus
     10de:0aa2 Nvidia MCP79 SMBus
     10de:0d79 Nvidia MCP89 SMBus

     1d94:790b Chengdu Haiguang IC Design Co., Ltd. (Hygon) FCH SMBus Controller

     8086:0c59 Intel Atom Processor S1200 SMBus 2.0 Controller 0 - ?
     8086:0c5a Intel Atom Processor S1200 SMBus 2.0 Controller 1 - ?
     8086:0c5b Intel Atom Processor S1200 SMBus Controller 2 - ?
     8086:0c5c Intel Atom Processor S1200 SMBus Controller 3 - ?
     8086:0c5d Intel Atom Processor S1200 SMBus Controller 4 - ?
     8086:0c5e Intel Atom Processor S1200 SMBus Controller 5 - ?
    (8086:0f13 ValleyView SMBus Controller - unconfirmed)
     8086:19ac Intel Atom Processor C3000 Series SMBus Contoller - Host - ?
     8086:1f15 Intel Atom processor C2000 SMBus 2.0 - ?
    (8086:1f3d Intel Atom Processor C2000 PECI SMBus - maybe irrelevant)
     8086:7603 Intel 82372FB PIIX5 SMBus
*/
};


int find_smb_controller(void)
{
    int i = 0;
    unsigned long valuev, valued;

    for (smbdev = 0; smbdev < 32; smbdev++) {
			for (smbfun = 0; smbfun < 8; smbfun++) {
		    pci_conf_read(0, smbdev, smbfun, 0, 2, &valuev);
		    if (valuev != 0xFFFF) {					// if there is something look what's it..
					for (i = 0; smbcontrollers[i].vendor > 0; i++) {	// check if this is a known smbus controller
			    	if (valuev == smbcontrollers[i].vendor) {
							pci_conf_read(0, smbdev, smbfun, 2, 2, &valued);	// read the device id
							if (valued == smbcontrollers[i].device) {
				    		return i;
							}
			    	}
					}
		    }
			}
    }
    return -1;
}



void get_spd_spec(void)
{
	  int index;
    int h, i, j, z;
    int k = 0;
    int module_size;
    int curcol;
    int temp_nbd;
    int tck;

    index = find_smb_controller();

    if (index == -1)
    {
    	// Unknown SMBUS Controller, exit
			return;
    }

    smbcontrollers[index].get_adr();
		cprint(LINE_SPD-2, 0, "Memory SPD Informations");
		cprint(LINE_SPD-1, 0, "--------------------------");

    for (j = 0; j < 8; j++) {
			if (smbcontrollers[index].read_spd(j) == 0) {
				curcol = 1;
				if(spd_raw[2] == 0x0c){
				  // We are here if DDR4 present

				  // First print slot#, module capacity
					cprint(LINE_SPD+k, curcol, " - Slot   :");
					dprint(LINE_SPD+k, curcol+8, k, 1, 0);

					module_size = get_ddr4_module_size(spd_raw[4], spd_raw[6], spd_raw[12], spd_raw[13]);
					temp_nbd = getnum(module_size); curcol += 12;
					dprint(LINE_SPD+k, curcol, module_size, temp_nbd, 0); curcol += temp_nbd;
					cprint(LINE_SPD+k, curcol, " MB"); curcol += 4;
					tck = spd_raw[18];
					// Then module jedec speed
					switch(tck)
					{
						default:
							cprint(LINE_SPD+k, curcol, "DDR4-????");
							break;
						case 0x0A:
							cprint(LINE_SPD+k, curcol, "DDR4-1600");
							break;
						case 0x09:
							cprint(LINE_SPD+k, curcol, "DDR4-1866");
							break;
						case 0x08:
							cprint(LINE_SPD+k, curcol, "DDR4-2133");
							break;
						case 0x07:
							cprint(LINE_SPD+k, curcol, "DDR4-2400");
							break;
						case 0x06:
							cprint(LINE_SPD+k, curcol, "DDR4-2666");
							break;
						case 0x05:
							cprint(LINE_SPD+k, curcol, "DDR4-3200");
							break;
						}

					curcol += 10;

					if((spd_raw[13] >> 3) == 1) { cprint(LINE_SPD+k, curcol, "ECC"); curcol += 4; }

					// Then print module infos (manufacturer & part number)
					spd_raw[320] &= 0x0F; // Parity odd or even
					for (i = 0; jep106[i].cont_code < 9; i++) {
			    		    if (spd_raw[320] == jep106[i].cont_code && spd_raw[321] == jep106[i].hex_byte) {
			    		    // We are here if a Jedec manufacturer is detected
							cprint(LINE_SPD+k, curcol, "-"); curcol += 2;
							cprint(LINE_SPD+k, curcol, jep106[i].name);
							for(z = 0; jep106[i].name[z] != '\0'; z++) { curcol++; }
							curcol++;
							// Display module serial number
							for (h = 325; h < 328; h++) {
								cprint(LINE_SPD+k, curcol, convert_hex_to_char(spd_raw[h]));
								curcol++;
							}

							// Detect Week and Year of Manufacturing (2004-2037 allowed)
							if(curcol <= 72 && spd_raw[323] > 3 && spd_raw[323] < 38 && spd_raw[324] < 55)
							{
								cprint(LINE_SPD+k, curcol, "(W");
								dprint(LINE_SPD+k, curcol+2, spd_raw[324], 2, 0);
								if(spd_raw[121] < 10) { cprint(LINE_SPD+k, curcol+2, "0"); }
								cprint(LINE_SPD+k, curcol+4, "'");
								dprint(LINE_SPD+k, curcol+5, spd_raw[323], 2, 0);
								if(spd_raw[120] < 10) { cprint(LINE_SPD+k, curcol+5, "0"); }
								cprint(LINE_SPD+k, curcol+7, ")");
								curcol += 9;
							}
					    }
					}
				}
				if(spd_raw[2] == 0x0b){
				  // We are here if DDR3 present

				  // First print slot#, module capacity
					cprint(LINE_SPD+k, curcol, " - Slot   :");
					dprint(LINE_SPD+k, curcol+8, k, 1, 0);

					module_size = get_ddr3_module_size(spd_raw[4] & 0xF, spd_raw[8] & 0x7, spd_raw[7] & 0x7, spd_raw[7] >> 3);
					temp_nbd = getnum(module_size); curcol += 12;
					dprint(LINE_SPD+k, curcol, module_size, temp_nbd, 0); curcol += temp_nbd;
					cprint(LINE_SPD+k, curcol, " MB"); curcol += 4;

					// If XMP is supported, check Tck in XMP reg
					if(spd_raw[176] == 0x0C && spd_raw[177] == 0x4A && spd_raw[12])
						{
							tck = spd_raw[186];
						} else {
							tck = spd_raw[12];
						}

					// Then module jedec speed
					switch(tck)
					{
						default:
							cprint(LINE_SPD+k, curcol, "DDR3-????");
							break;
						case 20:
							cprint(LINE_SPD+k, curcol, "DDR3-800");
							curcol--;
							break;
						case 15:
							cprint(LINE_SPD+k, curcol, "DDR3-1066");
							break;
						case 12:
							cprint(LINE_SPD+k, curcol, "DDR3-1333");
							break;
						case 10:
							cprint(LINE_SPD+k, curcol, "DDR3-1600");
							break;
						case 9:
							cprint(LINE_SPD+k, curcol, "DDR3-1866");
							break;
						case 8:
							cprint(LINE_SPD+k, curcol, "DDR3-2133");
							break;
						case 7:
							cprint(LINE_SPD+k, curcol, "DDR3-2400");
							break;
						case 6:
							cprint(LINE_SPD+k, curcol, "DDR3-2533");
							break;
						case 5:
							cprint(LINE_SPD+k, curcol, "DDR3-2666");
							break;
						}

					curcol += 10;

					if((spd_raw[8] >> 3) == 1) { cprint(LINE_SPD+k, curcol, "ECC"); curcol += 4; }

					// Then print module infos (manufacturer & part number)
					spd_raw[117] &= 0x0F; // Parity odd or even
					for (i = 0; jep106[i].cont_code < 9; i++) {
			    	if (spd_raw[117] == jep106[i].cont_code && spd_raw[118] == jep106[i].hex_byte) {
			    		// We are here if a Jedec manufacturer is detected
							cprint(LINE_SPD+k, curcol, "-"); curcol += 2;
							cprint(LINE_SPD+k, curcol, jep106[i].name);
							for(z = 0; jep106[i].name[z] != '\0'; z++) { curcol++; }
							curcol++;
							// Display module serial number
							for (h = 128; h < 146; h++) {
								cprint(LINE_SPD+k, curcol, convert_hex_to_char(spd_raw[h]));
								curcol++;
							}

							// Detect Week and Year of Manufacturing (2004-2037 allowed)
							if(curcol <= 72 && spd_raw[120] > 3 && spd_raw[120] < 38 && spd_raw[121] < 55)
							{
								cprint(LINE_SPD+k, curcol, "(W");
								dprint(LINE_SPD+k, curcol+2, spd_raw[121], 2, 0);
								if(spd_raw[121] < 10) { cprint(LINE_SPD+k, curcol+2, "0"); }
								cprint(LINE_SPD+k, curcol+4, "'");
								dprint(LINE_SPD+k, curcol+5, spd_raw[120], 2, 0);
								if(spd_raw[120] < 10) { cprint(LINE_SPD+k, curcol+5, "0"); }
								cprint(LINE_SPD+k, curcol+7, ")");
								curcol += 9;
							}

							// Detect XMP Memory
							if(spd_raw[176] == 0x0C && spd_raw[177] == 0x4A)
								{
									cprint(LINE_SPD+k, curcol, "*XMP*");
								}
			    	}
					}
				}
			// We enter this function if DDR2 is detected
			if(spd_raw[2] == 0x08){
					 // First print slot#, module capacity
					cprint(LINE_SPD+k, curcol, " - Slot   :");
					dprint(LINE_SPD+k, curcol+8, k, 1, 0);

					module_size = get_ddr2_module_size(spd_raw[31], spd_raw[5]);
					temp_nbd = getnum(module_size); curcol += 12;
					dprint(LINE_SPD+k, curcol, module_size, temp_nbd, 0); curcol += temp_nbd;
					cprint(LINE_SPD+k, curcol, " MB"); curcol += 4;

					// Then module jedec speed
					float ddr2_speed, byte1, byte2;

					byte1 = (spd_raw[9] >> 4) * 10;
					byte2 = spd_raw[9] & 0xF;

					ddr2_speed = 1 / (byte1 + byte2) * 10000 * 2;

					temp_nbd = getnum(ddr2_speed);
					cprint(LINE_SPD+k, curcol, "DDR2-"); curcol += 5;
					dprint(LINE_SPD+k, curcol, ddr2_speed, temp_nbd, 0); curcol += temp_nbd;

					if((spd_raw[11] >> 1) == 1) { cprint(LINE_SPD+k, curcol+1, "ECC"); curcol += 4; }

					// Then print module infos (manufacturer & part number)
					int ccode = 0;

					for(i = 64; i < 72; i++)
					{
						if(spd_raw[i] == 0x7F) { ccode++; }
					}

					curcol++;

					for (i = 0; jep106[i].cont_code < 9; i++) {
			    	if (ccode == jep106[i].cont_code && spd_raw[64+ccode] == jep106[i].hex_byte) {
			    		// We are here if a Jedec manufacturer is detected
							cprint(LINE_SPD+k, curcol, "-"); curcol += 2;
							cprint(LINE_SPD+k, curcol, jep106[i].name);
							for(z = 0; jep106[i].name[z] != '\0'; z++) { curcol++; }
							curcol++;
							// Display module serial number
							for (h = 73; h < 91; h++) {
								cprint(LINE_SPD+k, curcol, convert_hex_to_char(spd_raw[h]));
								curcol++;
							}

			    	}
					}

				}
			k++;
			}
    }
}


void show_spd(void)
{
    int index;
    int i, j;
    int data = 0;
    popup(POP_SAVE_BUFFER_2);
    index = find_smb_controller();
    if (index == -1) {
	cprint(POP2_Y, POP2_X+1, "SMBus Controller not known");
	goto exit;
    }

    smbcontrollers[index].get_adr();
    wait_keyup();
    for (j = 0; j < 16; j++) {
	if (smbcontrollers[index].read_spd(j) == 0) {
	    popclear(POP_SAVE_BUFFER_2);
	    cprint(POP2_Y, POP2_X + 1, "SPD Data: Slot");
	    dprint(POP2_Y, POP2_X + 15, j, 2, 0);
    	    for (i = 0; i < 256; i++) {
		hprint2(2 + POP2_Y + i / 16, 3 + POP2_X + (i % 16) * 3, spd_raw[i], 2);
	    }
	    data++;
	    while (!get_key());
	    wait_keyup();
	}
    }
    if (data == 0) {
	cprint(POP2_Y, POP2_X+1, "SMBus Controller present, but returned no info");
    } else goto exit_nowait;

    exit:
    while (!get_key());
    wait_keyup();
    exit_nowait:
    popdown(POP_SAVE_BUFFER_2);
}

int get_ddr4_module_size(int b4, int b6, int b12, int b13)
{
    // from i2c-tools
    int sdram_width = 4 << (b12 & 0x07);
    int ranks = ((b12 >> 3) & 0x07) + 1;
    int signal_loading = b6 & 0x03;
    int die_count = ((b6 >> 4) & 0x07) + 1;
    int cap = (256 << (b4 & 0x0f)) / 8;
    cap *= (8 << (b13 & 0x07)) / sdram_width;
    cap *= ranks;

    if (signal_loading == 0x02) cap *= die_count;

    return cap;
}

int get_ddr3_module_size(int sdram_capacity, int prim_bus_width, int sdram_width, int ranks)
{
	int module_size;

	switch(sdram_capacity)
	{
		case 0:
			module_size = 256;
			break;
		case 1:
			module_size = 512;
			break;
		default:
		case 2:
			module_size = 1024;
			break;
		case 3:
			module_size = 2048;
			break;
		case 4:
			module_size = 4096;
			break;
		case 5:
			module_size = 8192;
			break;
		case 6:
			module_size = 16384;
			break;
		}

		module_size /= 8;

	switch(prim_bus_width)
	{
		case 0:
			module_size *= 8;
			break;
		case 1:
			module_size *= 16;
			break;
		case 2:
			module_size *= 32;
			break;
		case 3:
			module_size *= 64;
			break;
		}

		switch(sdram_width)
	{
		case 0:
			module_size /= 4;
			break;
		case 1:
			module_size /= 8;
			break;
		case 2:
			module_size /= 16;
			break;
		case 3:
			module_size /= 32;
			break;

		}

	module_size *= (ranks + 1);

	return module_size;
}


int get_ddr2_module_size(int rank_density_byte, int rank_num_byte)
{
	int module_size;

	switch(rank_density_byte)
	{
		case 1:
			module_size = 1024;
			break;
		case 2:
			module_size = 2048;
			break;
		case 4:
			module_size = 4096;
			break;
		case 8:
			module_size = 8192;
			break;
		case 16:
			module_size = 16384;
			break;
		case 32:
			module_size = 128;
			break;
		case 64:
			module_size = 256;
			break;
		default:
		case 128:
			module_size = 512;
			break;
		}

	module_size *= (rank_num_byte & 7) + 1;

	return module_size;

}


struct ascii_map {
    unsigned hex_code;
    char *name;
};


char* convert_hex_to_char(unsigned hex_org) {
        static char buf[2] = " ";
        if (hex_org >= 0x20 && hex_org < 0x80) {
                buf[0] = hex_org;
        } else {
                //buf[0] = '\0';
                buf[0] = ' ';
        }

        return buf;
}
