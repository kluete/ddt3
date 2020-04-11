// Lua require() load overrider

#include <cassert>
#include <memory>

#include "ddt/sharedDefs.h"

#include "lx/stream/MemoryInputStream.h"
#include "lx/stream/MemoryOutputStream.h"

#include "ddt/FileSystem.h"

#include "ddt/sharedLog.h"

#include "Daemon.h"

#include "LuaGlue.h"

using namespace std;
using namespace LX;
using namespace DDT;
using namespace LX::FileName_std;

template<typename _V>
void	SetPackage(lua_State *L, const char *sub_t, const string &module, _V v);

void	SetPackage(lua_State *L, const char *sub_t, const string &module, const lua_CFunction v)
{
	assert(L);
	assert(sub_t);
	assert(!module.empty());
	ddt_getglobal(L, "package");
	assert(lua_istable(L, -1));
	ddt_getfield(L, -1, sub_t);
	assert(lua_istable(L, -1));
	lua_pushcfunction(L, v);
	ddt_setfield(L, -2, module.c_str());
	lua_pop(L, 2);		// pop both tables	
}

void	SetPackage(lua_State *L, const char *sub_t, const string &module, const bool v)
{
	assert(L);
	assert(sub_t);
	assert(!module.empty());
	ddt_getglobal(L, "package");
	assert(lua_istable(L, -1));
	ddt_getfield(L, -1, sub_t);
	assert(lua_istable(L, -1));
	lua_pushboolean(L, v);
	ddt_setfield(L, -2, module.c_str());
	lua_pop(L, 2);		// pop both tables	
}

void	SetPackage(lua_State *L, const char *sub_t, const string &module, const nullptr_t v)
{
	assert(L);
	assert(sub_t);
	assert(!module.empty());
	ddt_getglobal(L, "package");
	assert(lua_istable(L, -1));
	ddt_getfield(L, -1, sub_t);
	assert(lua_istable(L, -1));
	lua_pushnil(L);
	ddt_setfield(L, -2, module.c_str());
	lua_pop(L, 2);		// pop both tables	
}

//---- On Intercept Load Lua File ---------------------------------------------

int	LuaGlue::OnInterceptLoadLuaFile(lua_State *L)
{
	const int	base_sp = lua_gettop(L);
	
	// name of required() file, minus ".lua" extension
	const string	module_name = luaL_checkstring(L, 1);
	uLog(INTERCEPT, "LuaGlue::OnInterceptLoadLuaFile(%S)", module_name);
	
	#if (LUA_51 == 1)
		// Lua 5.1 doesn't have accessible filename resolver, but this is NO SOLUTION
		uWarn("  WARNING: intercepted off-project source %S", module_name);
		
		// use package.searchpath(name, path) ?
		//   NO, filesystem shouldn't be used AT ALL
		
		return 0;		// report not found
		
	#else
		// emulate "package.searchpath(shortname, package.path)"
		lua_getfield(L, lua_upvalueindex(1), "searchpath");
		assert(lua_isfunction(L, -1));
		// re-push module name
		lua_pushvalue(L, -2);
		assert(lua_isstring(L, -1));
		// get package.path search template
		lua_getfield(L, lua_upvalueindex(1), "path");
		assert(lua_isstring(L, -1));
		// get package.path
		const string	package_path_pattern = luaL_checkstring(L, -1);
		// resolve fullpathname					// UNPROTECTED call, but shouldn't throw any error (SURE?)
		lua_call(L, 2, LUA_MULTRET);
		
		// if has two results is "file not found" error
		// const int	n_res = lua_gettop(L) - base_sp;
		
		if (lua_isnil(L, base_sp + 1))
		{	// FILE NOT FOUND
			const string	error_msg = luaL_checkstring(L, base_sp + 2);
			uWarn("WARNING: interception FAILED: %S", error_msg);
			// return 2;		// pass nil + error up
			
			// continue in case is C module
			return 0;
		}
	#endif
	
	// file FOUND
	string	daemon_path = luaL_checkstring(L, base_sp + 1);
	
	const SplitPath	split = SplitUnixPath(daemon_path);
	assert(split && !split.Ext().empty());
	
	const string	client_name = module_name + "." + split.Ext();
	
	// add to file notifications stack
	m_FileNotificationQueue.push_back(client_name);
	
	// balance
	const int	top = lua_gettop(L);
	assert((base_sp + 1) == top);

	// returns filename string
	return 1;
}

//---- Custom Searcher (on new Lua file) --------------------------------------

static
int	s_CustomSearcher(lua_State *L)
{
	assert(L);
	
	LuaGlue	*lglue = GetLuaGlue(L);
	assert(lglue);
	
	int	res = lglue->OnInterceptLoadLuaFile(L);
	return res;
}

//---- Insert Overrider Hook --------------------------------------------------

void	LuaGlue::InsertOverriderHook(lua_State *L)
{
	uLog(INTERCEPT, "LuaGlue::InsertOverriderHook()");
	assert(L);
	
	const int	base_sp = lua_gettop(L);

	// get table.insert() function
	ddt_getglobal(L, "table");
	assert(lua_istable(L, -1));
	ddt_getfield(L, -1, "insert");
	assert(lua_isfunction(L, -1));
	lua_remove(L, -2);				// (pop temp 'table[]')
	
	ddt_getglobal(L, "package");
	assert(lua_istable(L, -1));
	
	#if LUA_51
		// insert package.loaders[]
		lua_pushstring(L, "loaders");			// lua 5.1 calls them LOADERS
		lua_rawget(L, -2);
	#else
		// insert package.searchers[]
		luaL_getsubtable(L, -1, "searchers");		// lua 5.2 calls them SEARCHERS
	#endif
	
	assert(lua_istable(L, -1));
	// (keep 'package[]' around for searcher upvalue)
	// push index 2
	lua_pushinteger(L, 2);
	// push searcher fn
	lua_pushvalue(L, -3);				// set 'package[]' as upvalue
	lua_pushcclosure(L, s_CustomSearcher, 1/*upvalues cnt*/);
	lua_remove(L, -4);				// (pop packages[])
	// call
	lua_call(L, 3/*nargs*/, 0/*results*/);
	
	assert(base_sp == lua_gettop(L));
	
	uLog(GLUE, "LuaGlue::InsertOverriderHook() succeeded");
}

//---- Custom Loader/Instantiator ---------------------------------------------

static
int	s_CustomModuleLoader(lua_State *L)
{
	assert(L);
	
	LuaGlue	*lglue = GetLuaGlue(L);
	assert(lglue);
	
	const int	res = lglue->OnRequireLoader(L);
	return res;
}

//---- On Instantiate Lua source File -----------------------------------------

	// RECURSIVE!

int	LuaGlue::OnRequireLoader(lua_State *L)
{
	assert(L);
	
	const int	nargs = lua_gettop(L);
	assert(nargs >= 1);
	
	const string	module = luaL_checkstring(L, 1);
	const string	module_filename = luaL_optstring(L, 2, "");			// Lua 5.2 doc: "if loader came from a file"
	
	uLog(INTERCEPT, "OnRequireLoader(%S)", module);
	uLog(STACK, "  stack @ %d", lua_gettop(L));
	
	const VM_ERR	res = InstantiateSource_LL(L, module);
	assert(res == VM_ERR::LUAE_OK);
	
	// check function is on top of stack
	assert(lua_isfunction(L, -1));
	// push module name
	lua_pushstring(L, module.c_str());
	
	// call outer shell / register its variables & functions
	const string	dbg_s = xsprintf("outer shell for module %S", module);
	LuaGlue::pcall(L, 1, 1, dbg_s);
	
	// assign result from require (or nil) to _G[module_name]
	// lua_setglobal(L, module.c_str());
	
	// pass on whatever module returned
	lua_pushvalue(L, -1);
	
	return 1;
}

//---- Instantiate Source (low-level) -----------------------------------------

	// RECURSIVE !

VM_ERR	LuaGlue::InstantiateSource_LL(lua_State *L, const string &v_module_name)
{	
	assert(L);
	
	uLog(INTERCEPT, "InstantiateSource_LL(%S)", v_module_name);
	uLog(STACK, "  stack @ %d", lua_gettop(L));
	
	assert(m_NetFileMap.count(v_module_name));
	const NetSourceFile	&nsf = m_NetFileMap.at(v_module_name);
	
	const string	short_filename = nsf.ShortFilename();
	assert(!m_FileInstantiatedSet.count(short_filename));
	
	const string	arobas_filename = string("@") + short_filename;
	const string	&buff = nsf.m_BinData;
	
	const int	err = luaL_loadbufferx(L, buff.data(), buff.size(), arobas_filename.c_str(), "t");
	if (err)
	{	// const string	err_type_s = LuaErrorTypeName(static_cast<DAEMON_ERR>(err));
		assert(err == LUA_ERRSYNTAX);
		
		// get retarded, flattened error message, which must then be reverse-engineered
		assert(LUA_TSTRING == lua_type(L, -1));
		const string	flat_err_s = luaL_checkstring(L, -1);
		uErr("  err msg %S", flat_err_s);
		
		// error format is "filename.lua:1234:whatever_err_msg"
		size_t	ind1 = flat_err_s.find(short_filename + ":");
		assert(ind1 == 0);
		ind1 += short_filename.length() + 1;
		size_t	ind2 = flat_err_s.find(":", ind1);
		assert(ind2 != string::npos);
		
		// chop-chop
		const string	chopsuey { flat_err_s, ind1, ind2 - ind1};
		assert(!chopsuey.empty());
		// aaaaaaand... we're back to pre-Lua error message mangling
		const int	ln = stoi(chopsuey);					// can THROW
		assert(ln > 0);
		
		// set FAKE stack
		m_DaemonStack.SetFakeLevel(short_filename, ln);
		
		// dirty hack will prevent a PANIC on exit
		const bool	ok = OnDebuggerEntry(L, ENTRY_TYPE::LOAD_FILE_ERROR, nil/*no hook*/);		// call a NOP lua function to trigger immediate hook instead???
		(void) ok;
		
		return VM_ERR::LUAE_ERRFILE;
	}
	
	// check function is on top of stack)
	assert(lua_isfunction(L, -1));
	
// unregister PRELOAD function -- not strictly needed as long as loaded flag is set below ?
	// const string	v_module_name{m_NetFileMap.at(module).VirtModuleName()};
	// uLog(INTERCEPT, "unregister package.preload[%S]", module);
	SetPackage(L, "preload", v_module_name, nil);
	
// register provisional LOADED flag
	uLog(INTERCEPT, "register PROVISIONAL package.loaded[%S] = true", v_module_name);
	SetPackage(L, "loaded", v_module_name, true);
	
	m_FileInstantiatedSet.insert(short_filename);
	// set any pending breakpoints
	SetValidBreakpoints();
	// add to file notifications stack
	m_FileNotificationQueue.push_back(short_filename);

	// RE-check function is on top of stack
	assert(lua_isfunction(L, -1));
	
	return VM_ERR::LUAE_OK;
}

//---- Patch Debian Search Paths ----------------------------------------------

void	LuaGlue::PatchDebianSearchPaths(lua_State *L)
{
	assert(L);
	
	const int	top0 = lua_gettop(L);
	
	ddt_getglobal(L, "package");
	assert(lua_istable(L, -1));

	// package.path
	ddt_getfield(L, -1, "path");
	assert(lua_isstring(L, -1));
	string	path_pattern = luaL_checkstring(L, -1);
	lua_pop(L, 1);
	#if (LUA_VERSION_NUM >= 502)
		path_pattern.append(";/usr/share/lua/5.2/?.lua");
	#else
		path_pattern.append(";/usr/share/lua/5.1/?.lua");
	#endif
	lua_pushstring(L, path_pattern.c_str());
	ddt_setfield(L, -2, "path");
	
	// package.cpath
	ddt_getfield(L, -1, "cpath");
	assert(lua_isstring(L, -1));
	string	cpath_pattern = luaL_checkstring(L, -1);
	lua_pop(L, 1);
	#if (LUA_VERSION_NUM >= 502)
		cpath_pattern.append(";/usr/lib/x86_64-linux-gnu/lua/5.2/?.so");
	#else
		cpath_pattern.append(";/usr/lib/x86_64-linux-gnu/lua/5.1/?.so");
	#endif
	lua_pushstring(L, cpath_pattern.c_str());
	ddt_setfield(L, -2, "cpath");

	lua_pop(L, 1);
	
	const int	top1 = lua_gettop(L);
	assert(top0 == top1);
}

//---- Preload (known/project) Lua Files --------------------------------------

	// see PIL book big/orange p.330 : "registering libraries on demand"

void	LuaGlue::PreloadFiles(lua_State *L, const vector<NetSourceFile> &net_file_list)
{
	uLog(INTERCEPT, "PreloadFiles(%zu netfiles)", net_file_list.size());
	assert(L);
	
	m_NetFileMap.clear();
	m_FileInstantiatedSet.clear();
	
	// for (const auto &nsf : net_file_list)
	for (int i = 0; i < net_file_list.size(); i++)
	{
		const auto &nsf = net_file_list[i];
		
		const string	module_name{nsf.ModuleName()};
		
		assert(!m_NetFileMap.count(module_name));
		
		// copy buffer to internals
		m_NetFileMap.emplace(module_name, nsf);
		
		uLog(INTERCEPT, "  [%2d] register loader for %S, %d lines", i, module_name, nsf.NumLines());
		
		SetPackage(L, "preload", module_name, s_CustomModuleLoader);
	}
}

// nada mas
