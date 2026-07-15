#include <stdio.h>
#include <stdlib.h>
#include "laststate/latch.h"
static ls_result_t print_tlv(void *context, uint16_t type, const uint8_t *value, uint16_t length) {
    (void)context;
    (void)value;
    printf("  tlv type=%u length=%u\n", (unsigned)type, (unsigned)length);
    return LS_OK;
}
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: latch-dump EVENT.lst\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("open");
        return 2;
    }
    if (fseek(file, 0, SEEK_END) || ftell(file) < 0) {
        fclose(file);
        return 2;
    }
    long size = ftell(file);
    rewind(file);
    uint8_t *data = (uint8_t *)malloc((size_t)size);
    if (!data) {
        fclose(file);
        return 2;
    }
    size_t read = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (read != (size_t)size) {
        free(data);
        return 2;
    }
    ls_envelope_info_t info;
    ls_result_t result = ls_envelope_validate(data, read, &info);
    if (result != LS_OK) {
        fprintf(stderr, "invalid LEP envelope: %d\n", result);
        free(data);
        return 1;
    }
    printf("LEP v%u type=%u arch=%u flags=0x%02x sequence=%u event=%08x payload=%u\n", info.version,
           info.type, info.architecture, info.flags, (unsigned)info.sequence,
           (unsigned)info.event_id, (unsigned)info.payload_length);
    result = ls_envelope_visit(data, read, print_tlv, 0);
    free(data);
    return result == LS_OK ? 0 : 1;
}
