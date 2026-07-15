#include "internal.h"
const char *ls_build_id(void) {
    return LS_BUILD_ID;
}
bool ls_build_id_validate(const char *build_id) {
    size_t length = ls_string_length(build_id);
    if (length < 8 || length > LS_BUILD_ID_MAX)
        return false;
    for (size_t i = 0; i < length; i++) {
        char c = build_id[i];
        bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                     c == '.' || c == '_' || c == '-' || c == ':';
        if (!valid)
            return false;
    }
    return true;
}
