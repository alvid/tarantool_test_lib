#include <iostream>

#include "library.h"
#include "lua.hpp"
#include "Dispatcher_fiber.hpp"
#include "RTSIB_server.hpp"

static const char *LIB_NAME = "luartsib";

#ifdef __cplusplus
extern "C" {
#endif

static const struct luaL_Reg rtsib_export[] = {
        { "create_server", create_server },
        { "run_dispatcher_fiber", run_dispatcher_fiber },
        { nullptr, nullptr }
};

int luaopen_rtsib(lua_State *lua)
{
    luaL_newlib(lua, rtsib_export);
    return 1;
}

#ifdef __cplusplus
};
#endif
