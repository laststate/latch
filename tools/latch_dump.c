#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "laststate/latch.h"

static int is_ascii_whitespace(int character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r' ||
           character == '\f' || character == '\v';
}

static int hex_digit(int character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

static int read_hex(FILE *file, uint8_t *data, size_t capacity, size_t *length) {
    int high = -1;
    int character;
    while ((character = fgetc(file)) != EOF) {
        if (is_ascii_whitespace(character)) {
            continue;
        }
        int digit = hex_digit(character);
        if (digit < 0) {
            fprintf(stderr, "invalid hex input: non-hex character 0x%02x\n",
                    (unsigned)(unsigned char)character);
            return 1;
        }
        if (high < 0) {
            high = digit;
            continue;
        }
        if (*length >= capacity) {
            fprintf(stderr, "hex input exceeds LS_MAX_EVENT_SIZE (%u bytes)\n",
                    (unsigned)LS_MAX_EVENT_SIZE);
            return 1;
        }
        data[*length] = (uint8_t)((high << 4) | digit);
        ++*length;
        high = -1;
    }
    if (ferror(file)) {
        perror("read");
        return 1;
    }
    if (high >= 0) {
        fprintf(stderr, "invalid hex input: odd number of digits\n");
        return 1;
    }
    return 0;
}

static ls_result_t print_tlv(void *context, uint16_t type, const uint8_t *value, uint16_t length) {
    (void)context;
    (void)value;
    printf("  tlv type=%u length=%u\n", (unsigned)type, (unsigned)length);
    return LS_OK;
}

static ls_result_t validate_tlv(void *context, uint16_t type, const uint8_t *value,
                                uint16_t length) {
    (void)context;
    (void)type;
    (void)value;
    (void)length;
    return LS_OK;
}

typedef struct {
    unsigned count;
} json_context_t;

static ls_result_t print_json_tlv(void *opaque, uint16_t type, const uint8_t *value,
                                  uint16_t length) {
    json_context_t *context = (json_context_t *)opaque;
    printf("%s{\"type\":%u,\"length\":%u,\"value_hex\":\"", context->count++ ? "," : "",
           (unsigned)type, (unsigned)length);
    for (uint16_t index = 0; index < length; ++index) {
        printf("%02x", (unsigned)value[index]);
    }
    fputs("\"}", stdout);
    return LS_OK;
}

static int dump_envelope(const uint8_t *data, size_t length, int json) {
    ls_envelope_info_t info;
    ls_result_t result = ls_envelope_validate(data, length, &info);
    if (result != LS_OK) {
        fprintf(stderr, "invalid LEP envelope: %d\n", result);
        return 1;
    }
    if (!json) {
        printf("LEP v%u type=%u arch=%u flags=0x%02x sequence=%u event=%08x payload=%u\n",
               info.version, info.type, info.architecture, info.flags, (unsigned)info.sequence,
               (unsigned)info.event_id, (unsigned)info.payload_length);
        result = ls_envelope_visit(data, length, print_tlv, 0);
        return result == LS_OK ? 0 : 1;
    }

    /* Validate the complete TLV walk before emitting any JSON. A malformed or
       encrypted
     * payload therefore never leaves a partial JSON document. */
    result = ls_envelope_visit(data, length, validate_tlv, 0);
    if (result != LS_OK) {
        fprintf(stderr, "LEP payload cannot be represented as TLV JSON: %d\n", result);
        return 1;
    }
    printf("{\"version\":%u,\"event_type\":%u,\"architecture\":%u,\"flags\":%u,"
           "\"sequence\":%u,\"event_id\":%u,\"event_id_hex\":\"%08x\","
           "\"payload_length\":%u,\"tlvs\":[",
           info.version, info.type, info.architecture, info.flags, (unsigned)info.sequence,
           (unsigned)info.event_id, (unsigned)info.event_id, (unsigned)info.payload_length);
    json_context_t context = {0};
    result = ls_envelope_visit(data, length, print_json_tlv, &context);
    fputs("]}\n", stdout);
    return result == LS_OK ? 0 : 1;
}

int main(int argc, char **argv) {
    int hex_input = 0;
    int json = 0;
    const char *path = 0;
    for (int index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--hex") == 0) {
            hex_input = 1;
        } else if (strcmp(argv[index], "--json") == 0) {
            json = 1;
        } else if (!path) {
            path = argv[index];
        } else {
            path = 0;
            break;
        }
    }
    if (!path) {
        fprintf(stderr, "usage: latch-dump [--hex] [--json] EVENT\n");
        return 2;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror("open");
        return 2;
    }
    if (hex_input) {
        uint8_t *data = (uint8_t *)malloc(LS_MAX_EVENT_SIZE);
        if (!data) {
            fclose(file);
            return 2;
        }
        size_t length = 0;
        int result = read_hex(file, data, LS_MAX_EVENT_SIZE, &length);
        fclose(file);
        if (result) {
            free(data);
            return 1;
        }
        result = dump_envelope(data, length, json);
        free(data);
        return result;
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
    int result = dump_envelope(data, read, json);
    free(data);
    return result;
}
