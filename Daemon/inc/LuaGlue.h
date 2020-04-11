// Lua DDT glue header

#pragma once

#ifndef nil
	#define nil	nullptr
#endif

#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>

#if LUA_CXX
	#include "lua.h"
	#include "lualib.h"
	#include "lauxlib.h"
#else
	#include "lua.hpp"
#endif

#if DDT_JIT
	extern "C"
	{
		#include "lj_debug.h"
	};
	
	using ddt_Debug = lj_Debug;
	
	inline
	int	ddt_getstack(lua_State *L, int stack_depth, ddt_Debug *ar_p)
	{	return lua_getstack(L, stack_depth, (lua_Debug*) ar_p);}
	
	inline
	const char*	ddt_getlocal(lua_State *L, ddt_Debug *ar_p, int index)
	{	return lua_getlocal(L, (lua_Debug*) ar_p, index);}
	
	inline
	void	ddt_getglobal(lua_State *L, const char *name)
	{	lua_pushstring(L, name);
		lua_rawget(L, LUA_GLOBALSINDEX/*table index*/);		// auto-pops string key
	}
	
	inline
	void	ddt_getfield(lua_State *L, const int &tab_index, const char *name)
	{	assert((tab_index > LUA_REGISTRYINDEX) && (tab_index < 0));
		assert(lua_istable(L, tab_index));
		assert(name);
		lua_pushstring(L, name);
		lua_rawget(L, tab_index - 1);				// auto-pops string key
	}
	
	inline
	void	ddt_setfield(lua_State *L, const int &tab_index, const char *name)
	{	assert((tab_index > LUA_REGISTRYINDEX) && (tab_index < 0));
		assert(lua_istable(L, tab_index));
		assert(name);
		lua_pushstring(L, name);				// push key
		lua_insert(L, -2);					// swap key/value
		lua_rawset(L, tab_index - 1);				// auto-pops key AND value
	}
	
	inline
	int	ddt_getinfo(lua_State *L, const char *what, ddt_Debug *ar_p)
	{	return lj_debug_getinfo(L, what, ar_p, 1/*extended*/);}

#else
	using ddt_Debug = lua_Debug;

	inline
	int	ddt_getstack(lua_State *L, int stack_depth, ddt_Debug *ar)
	{	return lua_getstack(L, stack_depth, ar);
	}
	
	inline
	const char*	ddt_getlocal(lua_State *L, ddt_Debug *ar, int index)
	{	return lua_getlocal(L, ar, index);
	}
	
	inline
	void	ddt_getglobal(lua_State *L, const char *name)
	{	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
		assert(lua_istable(L, -1));
		lua_pushstring(L, name);
		lua_rawget(L, -2/*table index*/);			// auto-pops string key
		lua_remove(L, -2);					// pop globals table
	}
	
	inline
	void	ddt_getfield(lua_State *L, const int &tab_index, const char *name)
	{	assert((tab_index > LUA_REGISTRYINDEX) && (tab_index < 0));
		assert(lua_istable(L, tab_index));
		assert(name);
		lua_pushstring(L, name);
		lua_rawget(L, tab_index - 1);				// auto-pops string key
	}
	
	inline
	void	ddt_setfield(lua_State *L, const int &tab_index, const char *name)
	{	assert((tab_index > LUA_REGISTRYINDEX) && (tab_index < 0));
		assert(lua_istable(L, tab_index));
		assert(name);
		lua_pushstring(L, name);				// push key
		lua_insert(L, -2);					// swap key/value
		lua_rawset(L, tab_index - 1);				// auto-pops key AND value
	}
	
	inline
	int	ddt_getinfo(lua_State *L, const char *what, ddt_Debug *ar_p)
	{	return lua_getinfo(L, what, ar_p);
	}

#endif

#include "ddt/MessageQueue.h"
#include "ddt/sharedDefs.h"
#include "ddt/CommonClient.h"

#include "VarCollectors.h"

namespace DDT
{

class Daemon;
class IJitGlue;

using std::string;
using std::vector;
using std::unordered_map;
using std::unordered_set;

enum class BREAK_TYPE : int
{
	NONE = 0,
	INSTRUCTION,
	LINE,
	FUNCTION_STEP_OUT,
	BREAKPOINTS,
	STEP_OVER
};

enum class ENTRY_TYPE : int
{
	UNDEFINED_ENTRY_TYPE = 0,
	HOOK,
	INLINE_BREAKPOINT_CALL,
	BREAKPOINT,
	
	// error types
	VM_ERROR_CALL,
	VM_ASSERT_CALL,
	VM_ERROR_HANDLER,
	LOAD_FILE_ERROR
};

// redefine because Lua headers use MACROS
enum class VM_ERR : int32_t
{
	LUAE_OK		= 0,
	LUAE_YIELD	= 1,
	LUAE_ERRRUN	= 2,
	LUAE_ERRSYNTAX	= 3,
	LUAE_ERRMEM	= 4,
	LUAE_ERRGCMM	= 5,
	LUAE_ERRERR	= 6,
	LUAE_ERRFILE	= 7,
	LUAE_ERRILLEGAL	= -1
};

string	LuaErrorTypeName(const VM_ERR &lua_err_t);

enum DAEMON_ABORT : int
{
	NOT_ABORT = 0,
	
	CLIENT_REQUEST_ABORT = 1000,		// beyond Lua errors
	CLIENT_REQUEST_END_SESSION,
	DAEMON_NETWORK_ERROR
};


using NetFileMap = unordered_map<string, NetSourceFile>;
using BreakpointSet = unordered_set<string>;

//---- Lua Glue ---------------------------------------------------------------

class LuaGlue
{
public:
	// ctor
	LuaGlue(Daemon &daemon);
	// dtor
	virtual ~LuaGlue();
	
	void	Reset(void);
	
	bool	InitLuaHooks(lua_State *L, const vector<NetSourceFile> &net_file_l, IJitGlue *jglue, const bool break_asap_f);
	bool	ExitLuaHooks(lua_State *L);
	
	VM_ERR	loadfile(lua_State *L, const string &lua_filename);
	void	pcall(lua_State *L, int nargs, int nresults, const string &debug_s);
	
	void	QueueMessage(const vector<uint8_t> &buff);
	
	void	ResumeDaemon(lua_State *L, CLIENT_CMD cmd);
	bool	OnDebuggerEntry(lua_State *L, const ENTRY_TYPE entry_type, lua_Debug *ar);		// returns [continue]
	void	OnSetProfiler(lua_State *L, const bool f, const int interval_ms);
	
	const uint8_t*	GetBreakpointLinesTab(size_t &len/*&*/) const;		// breakpoints: runtime setup function
	void	UpdateFastBreakpoints(void);						// breakpoints: fastest runtime test function
	bool	FastBreakpointExists(const char *source_name, const int &ln) const;	// breakpoints: 2nd fastest runtime test function
	
	void	SaveAllBreakpoints(const NotifyBreakpoints &notify_bp);
	
	void	SetLuaHook(lua_State *L, const BREAK_TYPE &hook_type);
	
	int	DoInternalAbort(lua_State *L, const DAEMON_ABORT &lvl);
	bool	IsInternalAbort(void) const;
	
	IJitGlue*	GetJitGlue(void) const		{return m_JitGlue;}
	
	// called from LoadOverrider (must be public)
	int	OnInterceptLoadLuaFile(lua_State *L);
	int	OnRequireLoader(lua_State *L);
	
private:

	void	SetValidBreakpoints(void);
	void	RequestVariables(lua_State *L, const CLIENT_MSG &msg_t, const VariableRequest &req);
	
	void	OnIncomingMessage(void);
	bool	ProcessQueue(lua_State *L);
	bool	DispatchMessage(lua_State *L, const LX::Message &msg);
	
	// LoadOverrider functions
	void	PatchDebianSearchPaths(lua_State *L);
	VM_ERR	InstantiateSource_LL(lua_State *L, const string &module);
	void	InsertOverriderHook(lua_State *L);
	void	PreloadFiles(lua_State *L, const vector<NetSourceFile> &net_file_list);
	
	Daemon			&m_Daemon;
	IJitGlue		*m_JitGlue;
	bool			m_BreakASAPFlag;
	DaemonStack		m_DaemonStack;
	CLIENT_CMD		m_ResumeCommand;
	DAEMON_ABORT		m_AbortLevel;
	
	LX::MessageQueue	m_DaemonQueue;
	VarCollectors		m_Collectors;
	
	int			m_StepOverLineStart, m_StepOverLineEnd;
	string			m_StepOverFileName;
	
	StringList		m_AllSavedBreakpoints;
	BreakpointSet		m_BreakpointSet;
	vector<uint8_t>		m_LineLUT;			// don't use vector<bool> ?
	
	NetFileMap		m_NetFileMap;
	unordered_set<string>	m_FileInstantiatedSet;
	
	StringList		m_FileNotificationQueue;
	
	bool			m_EntryRecurseLock, m_pcallRecurseLock;
	ENTRY_TYPE		m_LastLuaError;
	// lua_Debug		*m_HookActivationRecord;	// (useless/unused)
	
	BREAK_TYPE		m_SavedHookType;		// during profiler runs
};

} // namespace DDT


extern
DDT::LuaGlue*	GetLuaGlue(lua_State *L);

// nada mas