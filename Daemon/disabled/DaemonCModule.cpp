// DDT Daemon Lua C Module binder

#if 0

#include "config.hpp"

#include "lua.hpp"
#include "lauxlib.h"

#include "Daemon.h"

#ifndef nil
	#define nil	nullptr
#endif

// mark required parameters for signature matching

#ifdef __GNUC__
	#define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
	#define UNUSED(x) UNUSED_ xx x
#endif


static const char *MODULE_NAME = "ddt";

struct LuaDDTContext
{
	lua_State	*L;
	class Daemon	*daemon;
	class wxLog	*log;
	int		ref;
	
} *s_ctx;

//---- Init -------------------------------------------------------------------

static
int	lua_ddt_init(lua_State *L)
{
	// create a new Lua DDT context
	LuaDDTContext	*lctx = lua_newuserdata(L, sizeof(LuaDDTContext));
	s_ctx = lctx;
	
	wxInitializer	initializer;
	if (!initializer)	exit(1);
	
	lctx->daemon = new Daemon(L);
	
	lctx->log = new	miniLog("daemon.log");
	
	// store reference to context so it doesn't get GCed
	// lua_pushvalue(L, -1);
	// lctx->ref = luaL_ref(L, LUA_REGISTRYINDEX);

	// store Lua state for use in callbacks
	lctx->L = L;
	
	return 0;
}

//---- Exit -------------------------------------------------------------------

static
int	lua_ddt_exit(lua_State *L)
{
	LuaDDTContext	*lctx = s_ctx;			// luaL_checkudata(L, 1, CONTEXT_METATABLE);
	
	delete lctx->daemon;
	
	// luaL_unref(L, LUA_REGISTRYINDEX, lctx->ref);
	// lua_gc(L, LUA_GCCOLLECT, 0);
	
	delete (lctx->log);
	
	return 0;
}

static
int	lua_ddt_wait_connection(lua_State *L)
{
	// wait for client connection before starting Lua
	bool	ok = s_Daemon->WaitClientConnection();
	assert(ok);
	
	
	
	
}

static
const luaL_reg moduleFunctions[] =
{
	{ "init",		lua_ddt_init		},
	{ "exit",		lua_ddt_exit		},
	{ "wait_connection",	lua_ddt_wait_connection	},
	{ nil, nil }
};

int
#ifdef __WIN32__
__declspec(dllexport)
#endif
luaopen_ddt(lua_State *L)
{
	luaL_register(L, MODULE_NAME, moduleFunctions);
	lua_pushvalue(L, -1);
	lua_setglobal(L, MODULE_NAME);

	return 1;
}

#endif

// nada mas