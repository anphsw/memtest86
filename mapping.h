#ifndef _MAPPING_H_
#define _MAPPING_H_

#include "stdint.h"

#define TESTS 7

// Macros for getting and setting relevant part of address
#define GET_RANGE(ADDR, START, END)  (ADDR & ((1 << END-START+1) - 1 << START))
// [TODO] Finish this macros
//#define SET_RANGE(ADDR, START, END, VALUE) ((ADDR & ~((1<<END+1)-1)) |
//                                            (VALUE << START))

typedef struct{
    char fieldName[20];
    uint8_t startBit;
    uint8_t endBit;
} TestArray;

//TestArray sandyTest = {"Byte", 0, 5};

void verify_mapping();

#endif /* _MAPPING_H_ */
