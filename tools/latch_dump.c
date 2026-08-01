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

static int read_binary(FILE *file, uint8_t *data, size_t capacity, size_t *length) {
    for (;;) {
        size_t remaining = capacity - *length;
        if (remaining == 0u) {
            int character = fgetc(file);
            if (character != EOF) {
                fprintf(stderr, "binary input exceeds LS_MAX_EVENT_SIZE (%u bytes)\n",
                        (unsigned)LS_MAX_EVENT_SIZE);
                return 1;
            }
            if (ferror(file)) {
                perror("read");
                return 1;
            }
            return 0;
        }
        size_t read = fread(data + *length, 1u, remaining, file);
        *length += read;
        if (read < remaining) {
            if (ferror(file)) {
                perror("read");
                return 1;
            }
            return 0;
        }
    }
}

static const char *event_type_name(uint8_t type) {
    switch (type) {
    case LS_EVENT_CRASH:
        return "crash";
    case LS_EVENT_ERROR:
        return "error";
    case LS_EVENT_MESSAGE:
        return "message";
    case LS_EVENT_HEALTH:
        return "health";
    case LS_EVENT_RESET:
        return "reset";
    case LS_EVENT_LOG:
        return "log";
    case LS_EVENT_PERIPHERAL:
        return "peripheral";
    case LS_EVENT_COREDUMP:
        return "coredump";
    default:
        return "unknown";
    }
}

static const char *architecture_name(uint8_t architecture) {
    switch (architecture) {
    case LS_ARCH_UNKNOWN:
        return "unknown";
    case LS_ARCH_CORTEX_M:
        return "cortex-m";
    case LS_ARCH_RISCV:
        return "riscv";
    case LS_ARCH_XTENSA:
        return "xtensa";
    case LS_ARCH_LINUX:
        return "linux";
    case LS_ARCH_RISCV64:
        return "riscv64";
    default:
        return "unknown";
    }
}

static const char *tlv_name(uint16_t type) {
    switch (type) {
    case LS_TLV_IDENTITY:
        return "identity";
    case LS_TLV_RESET:
        return "reset";
    case LS_TLV_EVENT:
        return "event";
    case LS_TLV_CPU:
        return "cpu";
    case LS_TLV_FAULT:
        return "fault";
    case LS_TLV_BREADCRUMB:
        return "breadcrumb";
    case LS_TLV_METRIC:
        return "metric";
    case LS_TLV_POWER:
        return "power";
    case LS_TLV_HEALTH:
        return "health";
    case LS_TLV_ASSERT:
        return "assert";
    case LS_TLV_PERIPHERAL:
        return "peripheral";
    case LS_TLV_LOG:
        return "log";
    case LS_TLV_MEMORY:
        return "memory";
    case LS_TLV_STACK:
        return "stack";
    case LS_TLV_HEAP:
        return "heap";
    case LS_TLV_CPU64:
        return "cpu64";
    default:
        return "unknown";
    }
}

static void print_hex(const uint8_t *value, uint16_t length) {
    for (uint16_t index = 0; index < length; ++index) {
        printf("%02x", (unsigned)value[index]);
    }
}

static ls_result_t print_tlv(void *context, uint16_t type, const uint8_t *value, uint16_t length) {
    (void)context;
    printf("  tlv type=%u (%s) length=%u value_hex=", (unsigned)type, tlv_name(type),
           (unsigned)length);
    print_hex(value, length);
    putchar('\n');
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
    printf("%s{\"type\":%u,\"name\":\"%s\",\"length\":%u,\"value_hex\":\"",
           context->count++ ? "," : "", (unsigned)type, tlv_name(type), (unsigned)length);
    print_hex(value, length);
    fputs("\"}", stdout);
    return LS_OK;
}

static void print_json_header(const ls_envelope_info_t *info) {
    printf("{\"version\":%u,\"event_type\":%u,\"event_type_name\":\"%s\","
           "\"architecture\":%u,\"architecture_name\":\"%s\",\"flags\":%u,"
           "\"sequence\":%u,\"event_id\":%u,\"event_id_hex\":\"%08x\","
           "\"payload_length\":%u",
           info->version, info->type, event_type_name(info->type), info->architecture,
           architecture_name(info->architecture), info->flags, (unsigned)info->sequence,
           (unsigned)info->event_id, (unsigned)info->event_id, (unsigned)info->payload_length);
}

static const char *integrity_status(const ls_envelope_info_t *info) {
    return (info->flags & LS_ENVELOPE_AUTHENTICATED) ? "not_verified" : "not_present";
}

static int dump_envelope(const uint8_t *data, size_t length, int json) {
    ls_envelope_info_t info;
    ls_result_t result = ls_envelope_validate(data, length, &info);
    if (result != LS_OK) {
        fprintf(stderr, "invalid LEP envelope: %d\n", result);
        return 1;
    }
    if (!json) {
        printf("LEP v%u type=%u (%s) arch=%u (%s) flags=0x%02x sequence=%u event=%08x "
               "payload=%u\n",
               info.version, info.type, event_type_name(info.type), info.architecture,
               architecture_name(info.architecture), info.flags, (unsigned)info.sequence,
               (unsigned)info.event_id, (unsigned)info.payload_length);
        if (info.flags & LS_ENVELOPE_ENCRYPTED) {
            puts("  payload: encrypted (structural metadata only; no decryption key was supplied)");
            return 0;
        }
        result = ls_envelope_visit(data, length, print_tlv, 0);
        if (result != LS_OK) {
            fprintf(stderr, "LEP payload cannot be represented as TLV text: %d\n", result);
            return 1;
        }
        return 0;
    }

    if (info.flags & LS_ENVELOPE_ENCRYPTED) {
        print_json_header(&info);
        printf(",\"payload_encoding\":\"encrypted\",\"integrity\":\"%s\"}\n",
               integrity_status(&info));
        return 0;
    }

    /* Validate the complete TLV walk before emitting JSON, so malformed input
     * never leaves a partial document. */
    result = ls_envelope_visit(data, length, validate_tlv, 0);
    if (result != LS_OK) {
        fprintf(stderr, "LEP payload cannot be represented as TLV JSON: %d\n", result);
        return 1;
    }
    print_json_header(&info);
    printf(",\"payload_encoding\":\"tlv\",\"integrity\":\"%s\",\"tlvs\":[",
           integrity_status(&info));
    json_context_t context = {0u};
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
        } else if (strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0) {
            printf(
                "usage: latch-dump [--hex] [--json] EVENT\n"
                "  EVENT is a binary LEP file, a whitespace-tolerant hexadecimal file with --hex,\n"
                "  or - to read either format from standard input.\n");
            return 0;
        } else if (argv[index][0] == '-' && argv[index][1] != '\0' &&
                   strcmp(argv[index], "-") != 0) {
            fprintf(stderr, "unknown option: %s\n", argv[index]);
            return 2;
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
    FILE *file = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
    if (!file) {
        perror("open");
        return 2;
    }
    uint8_t *data = (uint8_t *)malloc(LS_MAX_EVENT_SIZE);
    if (!data) {
        if (file != stdin) {
            fclose(file);
        }
        return 2;
    }
    size_t length = 0u;
    int result = hex_input ? read_hex(file, data, LS_MAX_EVENT_SIZE, &length)
                           : read_binary(file, data, LS_MAX_EVENT_SIZE, &length);
    if (file != stdin && fclose(file) != 0 && result == 0) {
        perror("close");
        result = 1;
    }
    if (result != 0) {
        free(data);
        return 1;
    }
    result = dump_envelope(data, length, json);
    free(data);
    return result;
}
