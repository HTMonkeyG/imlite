#ifndef __LITE_LUA_HPP__
#define __LITE_LUA_HPP__

/* Lua is a C library and its public headers have no extern "C" guards. */
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

/* Registry keys shared by the binding and the Lua libraries. */
#define LITE_INSTANCE_KEY "lite.instance"
#define LITE_CORE_KEY     "lite.core"

#endif
