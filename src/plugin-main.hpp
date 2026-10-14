#pragma once

#include <obs-module.h>

#ifdef __cplusplus
extern "C" {
#endif

bool obs_module_load(void);
void obs_module_unload(void);
const char* obs_module_name(void);
const char* obs_module_description(void);

#ifdef __cplusplus
}
#endif
