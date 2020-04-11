// ddt main Daemon

#pragma once

#include <thread>
#include <memory>

#include "LuaGlue.h"

#include "ServerTCP.h"

#include "ddt/sharedDefs.h"

#include "lx/xutils.h"
#include "lx/ulog.h"

#include "lx/stream/MemDataOutputStream.h"

// for iOS UI refresh (debugging the debugger)
// #define		DDT_IOS_LUA_TICK

// forward declarations
struct lua_State;

namespace DDT
{
const int	DDT_DAEMON_DEFAULT_PROFILER_INTERVAL_MS		= 10;
const size_t	DDT_DAEMON_DEFAULT_PROFILER_MAX_DEPTH		= 5;

// import into namespace
using std::string;
using std::uint32_t;
using std::thread;
using std::unordered_set;
using std::unordered_map;
using std::unique_ptr;

using LX::timestamp_t;
using LX::STAMP_FORMAT;
using LX::LogSlot;
using LX::MemDataOutputStream;

class IServerTCP;
class JitGlue;

//---- Lua Startup ------------------------------------------------------------

class LuaStartup
{
public:

	// ctor
	LuaStartup()
	{
		clear();
	}
	
	string	m_Source, m_Function;
	bool	m_JITFlag, m_LoadBreakFlag;
	
	void	clear(void)
	{	m_Source = m_Function = "";
		m_LoadBreakFlag = false;
	}
};

//---- Daemon interface -------------------------------------------------------

class IDaemon
{
public:
	virtual ~IDaemon() = default;
	
	virtual bool	WaitForConnection(void) = 0;
	virtual bool	SessionSetupLoop(LuaStartup &startup) = 0;
	virtual void	Reset(void) = 0;
	virtual void	InitDDT(lua_State *L, IJitGlue *jglue, const bool break_asap_f) = 0;
	virtual void	ExitDDT(lua_State *L) = 0;
	virtual bool	SessionEnded(const int cod) = 0;
	
	virtual VM_ERR	loadfile(lua_State *L, const string &lua_filename) = 0;
	virtual void	pcall(lua_State *L, int nargs, int nresults, const string &debug_s) = 0;
	
	virtual bool	IsConnected(void) const = 0;
	
	virtual void	EnableNetLogLevels(const unordered_set<LogLevel> &levels) = 0;
	
	static
	void		LogLuaMsg(const string &msg);
	
	static LogSlot*	CreateTermLog(const bool console_f, const STAMP_FORMAT &fmt = STAMP_FORMAT::MILLISEC);
	static LogSlot*	CreateIosLog(const bool console_f, const STAMP_FORMAT &fmt = STAMP_FORMAT::MILLISEC);
	
	static
	IDaemon*	Create(const int server_port, const int sleep_ms);
};

} // namespace DDT

// nada mas
