#include <stdio.h>
#include <string.h>
#include "laststate/latch.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "build-id failed: %s:%d\n", #x, __LINE__);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    CHECK(!ls_build_id_validate(0));
    CHECK(!ls_build_id_validate("short"));
    CHECK(ls_build_id_validate("01234567"));
    CHECK(ls_build_id_validate("v1.2.3:abc_DEF-9"));
    CHECK(!ls_build_id_validate("abcdefgh/"));
    char long_id[LS_BUILD_ID_MAX + 2];
    memset(long_id, 'a', sizeof long_id);
    long_id[sizeof long_id - 1] = 0;
    CHECK(!ls_build_id_validate(long_id));
    ls_identity_t invalid = {.firmware_build_id = "bad"};
    ls_config_t config = {.identity = &invalid};
    CHECK(ls_init(&config) == LS_EINVAL);
    ls_identity_t valid = {.firmware_build_id = "valid-id"};
    config.identity = &valid;
    CHECK(ls_init(&config) == LS_OK);
    CHECK(ls_build_id_validate(ls_build_id()));
    return 0;
}
