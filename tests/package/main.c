#include "laststate/latch.h"

int main(void) {
    return ls_build_id_validate(ls_build_id()) ? 0 : 1;
}
