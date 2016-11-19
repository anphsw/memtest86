#ifndef _MAPPING_H_
#define _MAPPING_H_

#include "stdint.h"

#define TESTS 6
#define LASTROWBIT 1<<13 // Last bit for byte-in-row addressing

// Macros for getting and setting relevant part of address
#define GET_RANGE(ADDR, START, END)  (ADDR & (((1 << END-START+1)-1) << START))
#define SET_RANGE(ADDR, START, END, VALUE) ((ADDR & ~(((1 << (END-START+1))-1) << START)) | (VALUE << START))

struct testArray {
    char fieldName[20];
    uint8_t startBit;
    uint8_t endBit;
};

void verify_mapping(int me);
void print_serial_single(int step, double mean, double sigma);
void print_test_status(char *testName, uint8_t begin);

extern struct vars * const v;

#endif /* _MAPPING_H_ */
