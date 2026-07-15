#include "laststate/latch.hpp"

int main() {
    return ls_build_id_validate(ls_build_id()) ? 0 : 1;
}
