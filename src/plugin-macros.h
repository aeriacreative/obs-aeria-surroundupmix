#pragma once

#include <obs-module.h>
#include <obs-frontend-api.h>

#define PLUGIN_NAME    "aeria-surroundupmix"
#define PLUGIN_VERSION "0.0.1"
#define PLUGIN_AUTHOR  "Aeria Creative"
#define PLUGIN_CREDITS "Aeria Sorround Upmix by Aeria Creative linktr.ee/aeriacreative"

#define blog(level, msg, ...) blog(level, "[aeria-surroundupmix] " msg, ##__VA_ARGS__)
