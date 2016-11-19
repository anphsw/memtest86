#ifndef _MAPPING_H_
#define _MAPPING_H_

#include "stdint.h"

#define TESTS 6

// Macros for getting and setting relevant part of address
#define GET_RANGE(ADDR, START, END)  (ADDR & (((1 << END-START+1)-1) << START))
#define SET_RANGE(ADDR, START, END, VALUE) ((ADDR & ~(((1 << (END-START+1))-1) << START)) | (VALUE << START))

struct testArray {
    char fieldName[20];
    uint8_t startBit;
    uint8_t endBit;
};

void verify_mapping();
void print_serial_single(int step, double mean, double sigma);
void print_test_status(char *testName, uint8_t begin);

#endif /* _MAPPING_H_ */
