#include <stdio.h>
#include "laststate/latch.h"

int main(void) {
    printf("Latch %s linked with FetchContent\n", LS_VERSION_STRING);
    return ls_build_id_validate(ls_build_id()) ? 0 : 1;
}
