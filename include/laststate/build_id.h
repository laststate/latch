#ifndef LASTSTATE_BUILD_ID_H
#define LASTSTATE_BUILD_ID_H

#include <stdbool.h>
#include "config.h"
#if defined(LS_HAS_GENERATED_BUILD_ID)
#include "laststate/build_id_generated.h"
#endif
#ifndef LS_BUILD_ID
#define LS_BUILD_ID "development-unknown"
#endif
const char *ls_build_id(void);
bool ls_build_id_validate(const char *build_id);
#endif
