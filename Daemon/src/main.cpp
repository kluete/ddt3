// ddt daemon main start

#include <cassert>
#include <string>
#include <regex>
#include <fstream>

#include <array>
#include <cmath>

#include "ddt/FileSystem.h"

#include "ddt/sharedLog.h"

#include "Daemon.h"
#include "DaemonJIT.h"

#define CATCH_CONFIG_RUNNER
#define CATCH_BREAK_INTO_DEBUGGER() 	LX::xtrap();
#include "catch.hpp"

#define	RUN_UNIT_TESTS		0

using namespace std;
using namespace LX;
using namespace DDT;

const int	DDT_DAEMON_NET_SLEEP_MS	= 0;

//---- File Log ---------------------------------------------------------------

class FileLog : public LogSlot
{
public:
	// ctor
	FileLog(const string &fname, const STAMP_FORMAT &fmt)
		: m_StampFormat(fmt),
		m_OFS {fname, ios_base::trunc}
	{
		assert(m_OFS && m_OFS.is_open());
	}
	virtual ~FileLog()	{}
	
	void	LogAtLevel(const timestamp_t stamp, const LogLevel level, const string &msg) override
	{
		// lock_guard<mutex>	lock(m_Mutex);
		
		const string	s = stamp.str(m_StampFormat) + " " + msg;
		
		if (level == ERROR)
		{
			m_OFS << "*** ERROR ***\n\t" << s << endl << flush;
		}
		else	m_OFS << s << endl;
	}
	
	void	ClearLog(void) override
	{
		const string sep_s(80, '=');
		
		m_OFS << endl << endl << endl;
		m_OFS << sep_s << endl;
		m_OFS << sep_s << endl;
		m_OFS << endl << "\t\t    NEW SESSION" << endl << endl;
		m_OFS << sep_s << endl;
		m_OFS << sep_s << endl;
		m_OFS << endl << endl << endl;
	}
	
private:
	
	const STAMP_FORMAT	m_StampFormat;
	mutable mutex		m_Mutex;
	ofstream		m_OFS;
};

//-----------------------------------------------------------------------------

static
int	lua_panic(lua_State *L)
{
	uLog(FATAL, "lua_panic()");
	
	assert(L);
	
	// called when error is outside any protected environment
	// - could do longjump to my own recovery point?
	throw("lua panic");

	return 0;
}

//---- Lua print OVERLOAD -----------------------------------------------------

static
int	lg_Print(lua_State *L)
{
	const int	nargs = lua_gettop(L);
	
	ostringstream	ss;
	
	for (int i = 1; i <= nargs; i++)
	{
		const int	typ = lua_type(L, i);
		// const char	*typ_s = lua_typename(L, typ);
		
		const string	s = (typ == LUA_TNIL) ? string("nil") : lua_tostring(L, i);
		
		ss << s + " ";
	}
	
	IDaemon::LogLuaMsg(ss.str());
	
	return 0;
}

//---- main -------------------------------------------------------------------

int	main(int argc, char* argv[])
{
	// main log
	rootLog	logImp;

	logImp.EnableLevels({FATAL, ERROR, EXCEPTION, WARNING, MSG});
	logImp.EnableLevels({LUA_ERR});
	// logImp.EnableLevels({STACK});
	// logImp.EnableLevels({RECURSE});
	// logImp.EnableLevels({COLLECT});
	logImp.EnableLevels({INTERCEPT});
	// logImp.EnableLevels({GLUE});
	// logImp.EnableLevels({MSG_IN, MSG_OUT, MSG_UDP});
	logImp.EnableLevels({SESSION});
	logImp.EnableLevels({PROF});
	// logImp.EnableLevels({BREAKPOINT});
	// logImp.EnableLevels({ASIO});
	logImp.EnableLevels({USER_MSG, LUA_USER});
	// logImp.EnableLevels({DTOR});
	
	// path ok on Windows? FIXME
	unique_ptr<LogSlot>	file_log(new FileLog("./daemon.log", STAMP_FORMAT::MICROSEC));
	logImp.Connect(file_log.get());
	
	unique_ptr<LogSlot>	term_log(IDaemon::CreateTermLog(true, STAMP_FORMAT::MILLISEC));
	logImp.Connect(term_log.get());
	
	#ifdef __DDT_IOS__
		iosLog	s_iosLogImp;
		logImp.Connect(&s_iosLogImp);
	#endif // __DDT_IOS__

	#if RUN_UNIT_TESTS
	{	logImp.EnableLevels({UNIT});
		
		const int	err = Catch::Session().run(argc, argv);
		if (err)	LX::xtrap();
		return err;
	}
	#endif
	
	uMsg("roger charlie tango");
	
	// static
	unique_ptr<IDaemon>	daemonPtr(IDaemon::Create(3001/*default port*/, DDT_DAEMON_NET_SLEEP_MS));
	IDaemon			&daemon(*daemonPtr);
	
	daemon.EnableNetLogLevels({FATAL, ERROR, EXCEPTION, WARNING, USER_MSG, RECURSE, LUA_ERR, PROF});
	
	uLog(USER_MSG, "top-knall UDP from daemon!!!");
	uLog(USER_MSG, "2nd UDP msg from daemon");
	
	bool	ok = daemon.WaitForConnection();
	if (!ok)
	{
		uErr("ERROR: couldn't wait on connection");
		return 1;
	}
	
	const char	registry_test = 'k';
	
	while (daemon.IsConnected())
	{
		#ifdef __DDT_IOS__
			daemon.Breathe(5000);
		
			uLog("long breathe done");
		
			daemon.Breathe(1000);
		#endif
		
		daemon.Reset();
		
		LuaStartup	startup;
		
		ok = daemon.SessionSetupLoop(startup/*&*/);
		if (!ok)
		{	// FIXME - shouldn't abort?
			uErr("SessionSetupLoop() aborted");
			break;
		}
		
		logImp.ClearLogAll();
		
		uLog(SESSION, "Daemon setup Lua");
		
	// init Lua
		lua_State *L = luaL_newstate();
		assert(L);

		luaL_openlibs(L);
		lua_atpanic(L, lua_panic);
		lua_settop(L, 0);
		
		lua_pushcfunction(L, lg_Print);
		lua_setglobal(L, "print");
		
		unique_ptr<IJitGlue>	jit_glue(IJitGlue::Create(L));			// must RE-allocate cause lua_State is new ?
		
	// init DDT
		daemon.InitDDT(L, jit_glue.get(), startup.m_LoadBreakFlag);
		
		jit_glue->ToggleJit(L, startup.m_JITFlag);
		
		/*
		lua_pushstring(L, "DDT userdata in registry test");
		char	*test = (char*) lua_newuserdata(L, 3);	
		assert(test);
		test[0] = 0x12;
		test[1] = 0x34;
		test[2] = 0x56;
		lua_settable(L, LUA_REGISTRYINDEX);
		
		// lua_pushlightuserdata(L, (void*)&registry_test);
		lua_pushstring(L, "DDT string in registry test");
		lua_pushstring(L, "val_registry_test");
		lua_settable(L, LUA_REGISTRYINDEX);
		*/
		
		uLog(SESSION, "bf loadfile startup source %S", startup.m_Source);
		
		try
		{
			const VM_ERR	res = daemon.loadfile(L, startup.m_Source);
			if (res != VM_ERR::LUAE_OK)
			{	uErr("error af daemon.loadfile()");
				goto clean_up;
			}
		}
		catch (exception &e)
		{
			uErr("caught exception on daemon.loadfile()from main.cpp");
		}
		
		try
		{
			// parse outer shell
			assert(lua_isfunction(L, -1));
			
			uLog(SESSION, "bf parse outer in main");
			daemon.pcall(L, 0, 0, "parse outer main");
		}
		catch (exception &e)
		{
			uErr("caught exception on daemon.pcall(outer) from main.cpp");
		}
		
		try
		{
			// (check global exists)
			ddt_getglobal(L, startup.m_Function.c_str());
			if (!lua_isfunction(L, -1))
			{	uErr("lua function %S not found", startup.m_Function);
				goto clean_up;					// should report back to client - FIXME
			}
			
			uLog(SESSION, "bf call main()");
			daemon.pcall(L, 0, 0, "startup main()");
		}
		catch (exception &e)
		{
			const char	*what_s = e.what();
		
			uErr("caught exception %S on daemon.pcall(main) from main.cpp", what_s);
		}
		
	clean_up:
		
		uLog(SESSION, "Lua session terminated");
		
		const int	used_kb = lua_gc(L, LUA_GCCOLLECT, 0);
		uLog(SESSION, "Lua VM used %d kb", used_kb);
		
		// notify client the session ended
		ok = daemon.SessionEnded(0);
		
		daemon.ExitDDT(L);
		
		lua_close(L);
		L = nil;
		
		if (!ok)			break;
		if (!daemon.IsConnected())	break;
		
		uLog(SESSION, "daemon reloop!");
	}
	
	return 0;
}

#ifdef DDT_IOS_LUA_TICK

static
lua_State	*s_LastLuaState = nil;

//---- Deferred Lua -----------------------------------------------------------

bool	DeferredLua(void)
{
	assert(g_Daemon);
	lua_State	*L = s_Daemon->GetLastLuaState();
	
	uLog("*** Tick lua ***");
	
	// push function (Lua vanilla)
	ddt_getglobal(L, "Tick");
	if (!lua_isfunction(L, -1))	{uLog("lua function Tick() not found"); return false;}

	try
	{
		// execute with debugger (NON-VANILLA)
		int	err = s_Daemon->pcall(L, 0, 0/*res*/);
		return (err == 0);
	}
	catch (...)
	{
		uLog("caught exception from Lua VM in DeferredLua()");
		
		// lua_gc(L, LUA_GCCOLLECT, 0);
		// lua_close(L);
		
		return false;
	}
}

	g_Daemon = &daemon;
	s_LastLuaState = L;
	
	// loop the loop
	bool	cont_f = true;

	do
	{	// strictly for iOS log update (irrelevant)
		// call async() lambda when tries to get future
		auto	fut = async(launch::deferred, [&] {return DeferredLua();});
		cont_f = fut.get();
	
	} while (cont_f);
	
	uLog("lua chain done");

#endif // DDT_LUA_TICK

// nada mas
