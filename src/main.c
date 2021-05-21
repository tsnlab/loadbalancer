#include <stdio.h>
#include <stdint.h>

#include <sglib.h>
#include <pv/checksum.h>

void do_sglib() {
    int arr[100];
    int size = sizeof(arr) / sizeof(arr[0]);

    for(int i = 0; i < size; i += 1) {
        int value = (i * 826391263816283 + 182616281) & 0xffff;
        arr[i] = value;
    }

    SGLIB_ARRAY_SINGLE_QUICK_SORT(int, arr, size, SGLIB_FAST_NUMERIC_COMPARATOR);

    for(int i = 0; i < size; i += 1) {
        printf("%04d: %04d\n", i, arr[i]);
    }
}

void do_packetvisor() {
    int arr[100];
    uint16_t cksum = checksum((void*) &arr, sizeof(arr));
    printf("Checksum: %04x\n", cksum);
}

int main(int argc, const char *argv[])
{
    do_sglib();
    do_packetvisor();
    return 0;
}
