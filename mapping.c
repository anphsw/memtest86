#include "mapping.h"
#include "test.h"
#include "config.h"

// [TODO] Consider Bank XORing with lower 3 bits of row number
struct testArray sandyTest[TESTS] = {{"Byte", 0, 5},
                                     {"Row", 18, 23}, // This is 18-32
                                     {"Bank", 14, 16},
                                     {"Rank", 17, 17},
                                     {"Module", 33, 34},
                                     {"Channel", 6, 6}};

void verify_mapping(int me) {

    uintptr_t base = (uintptr_t) v->map[0].start;
    struct sample s;

    // Assigsned memory segments are:
    // 1048576 = 1 << 20
    // 2147483648 = 1 << 31, this 4 times

    // Start with an aligned address at least to byte 13
    uintptr_t offset = base%LASTROWBIT;
    base += offset;

    for(int i=0; i<TESTS; i++) {
        char *name = sandyTest[i].fieldName;
        int start = sandyTest[i].startBit;
        int end = sandyTest[i].endBit;
        int size = 1 << (end-start+1);

        print_test_status(name, 1);

        for(int j=0; j < size; j++) {
            uint64_t target = SET_RANGE(base, start, end, j);
            s = measure(target);
            print_serial_single(j, s.mean, s.variance);
        }
        print_test_status(name, 0);
        do_tick(me);
    }
}

void print_serial_single(int step, double mean, double variance) {
    serial_echo_print("\nBegin sample data");
    serial_echo_print("\nstep:");
    serial_echo_printd(step, 5);
    serial_echo_print("\nmean:");
    serial_echo_printd(mean, 5);
    serial_echo_print("\nvariance:");
    serial_echo_printd(variance, 5);
}

void print_test_status(char *testName, uint8_t begin) {
    if(begin) {
        serial_echo_print("\n\nBegin test ");
        serial_echo_print(testName);
        serial_echo_print(":\n");
    } else {
        serial_echo_print("\n\nFinished test ");
        serial_echo_print(testName);
        serial_echo_print(":\n");
    }
}
