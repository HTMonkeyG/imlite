#ifndef __LITE_API_HPP__
#define __LITE_API_HPP__

struct lua_State;

/* Registers the `system` and `renderer` libraries into a Lua state. */
int LiteOpenSystem(lua_State *L);
int LiteOpenRenderer(lua_State *L);

#endif
