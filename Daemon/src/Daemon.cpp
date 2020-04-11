// ddt main Daemon
		
#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <fstream>

#if DDT_JIT
	#include "luajit.h"
#endif

#include "ddt/sharedLog.h"

#include "DaemonImp.h"

using namespace std;
using namespace LX;
using namespace DDT;

// static
IServerTCP	*Daemon::s_ServerInst = nil;

//---- CTOR -------------------------------------------------------------------

	Daemon::Daemon(const int server_port, const int sleep_ms)
		: m_ServerTCPPtr(IServerTCP::Create(server_port, *this/*breather*/)),
		m_ServerTCP(*m_ServerTCPPtr),
		m_LuaGlue(*this),
		m_AsyncSleepMS(sleep_ms)
{
	uLog(SESSION, "Daemon::CTOR(port %d, sleep_ms %d)", server_port, sleep_ms);
	
	rootLog::Get().Connect(this);
	
	s_ServerInst = m_ServerTCPPtr.get();
}

//---- DTOR -------------------------------------------------------------------

	Daemon::~Daemon()
{
	uLog(SESSION, "Daemon::DTOR");
	
	s_ServerInst = nil;
}

//---- Reset ------------------------------------------------------------------

void	Daemon::Reset(void)
{
	uLog(SESSION, "Daemon::Reset()");
	
	m_LuaGlue.Reset();
}

//---- Init DDT ---------------------------------------------------------------

void	Daemon::InitDDT(lua_State *L, IJitGlue *jglue, const bool break_asap_f)
{
	uLog(SESSION, "Daemon::InitDDT()");
	
	assert(L);
	assert(jglue);
	
	m_LuaGlue.InitLuaHooks(L, m_NetFileList, jglue, break_asap_f);
}

//---- Exit DDT ---------------------------------------------------------------

void	Daemon::ExitDDT(lua_State *L)
{
	assert(L);
	
	m_LuaGlue.ExitLuaHooks(L);
}

//---- Wait For Connection ----------------------------------------------------

bool	Daemon::WaitForConnection(void)
{
	const bool	ok = m_ServerTCP.WaitForConnection();
	
	return ok;
}

//---- Notify Sync & Start Failed ---------------------------------------------

void	Daemon::SyncNStartFailed(const string &err_msg)
{
	uErr("ERROR - SyncNStartFailed()");
	
	SendMessage(DAEMON_MSG::NOTIFY_SYNC_FAILED, err_msg);

	m_NetFileList.clear();
}

//---- Breathe IMP ------------------------------------------------------------

bool	Daemon::Breathe(void)
{
	if (m_AsyncSleepMS <= 0)
		this_thread::yield();
	else	this_thread::sleep_for(chrono::milliseconds{m_AsyncSleepMS});		// bullshit but FASTER than yield ??
	
	return true;    // ok;
}

//---- Session Setup Loop -----------------------------------------------------

bool	Daemon::SessionSetupLoop(LuaStartup &startup)
{
	uLog(SESSION, "SessionSetupLoop() ENTERED");
	
	startup.clear();
	
	/*
	ok = m_ServerTCP.SetOption(TCP_OPTION::RECEIVE_BUFFER_SIZE, 32);
	assert(ok);
	ok = m_ServerTCP.SetOption(TCP_OPTION::SEND_BUFFER_SIZE, 32);
	assert(ok);
	*/
	
	// sync loop
	while (true)
	{	vector<uint8_t>	msg_buff;
		
		bool	ok = ReadMessage(msg_buff/*&*/);
		if (!ok)	return false;
		
		MemDataInputStream	mis(msg_buff);
		assert(mis.IsOk());
		
		CLIENT_MSG	msg_t;
		
		mis >> msg_t;
		
		uLog(SESSION, "  SessionSetup(%s)", MessageName(msg_t));
		
		switch (msg_t)
		{
			case CLIENT_MSG::REQUEST_PLATFORM:
			{
				PlatformInfo	info;
				
				info.m_OS = DDT_PLATFORM_OS_NAME;
				info.m_Architecture = DDT_PLATFORM_OS_ARCH;
				info.m_Hostname = m_ServerTCP.GetHostname();
				info.m_Lua = LUA_RELEASE;
				info.m_LuaNum = LUA_VERSION_NUM;
				
				// (Lua VM not yet initialized here)
				#if DDT_JIT
					info.m_LuaJIT = LUAJIT_VERSION;
					info.m_LuaJITNum = LUAJIT_VERSION_NUM;
				#else
					info.m_LuaJIT = "no jit";
					info.m_LuaJITNum = 0;
				#endif
				
				SendMessage(DAEMON_MSG::REPLY_PLATFORM, info);
			}	break;
			
			case CLIENT_MSG::REQUEST_SYNC_N_START_LUA:
			{
				RequestStartupLua	req_start(mis);
				if (!req_start.IsOk())
				{	SyncNStartFailed("bad startup struct");
					break;
				}
				
				// store in ref'ed vars (why is different struct?)
				startup.m_Source = req_start.m_Source;
				startup.m_Function = req_start.m_Function;
				startup.m_JITFlag = req_start.m_JITFlag;
				startup.m_LoadBreakFlag = req_start.m_LoadBreakFlag;
				
				const uint32_t	num_files = req_start.m_NumSourceFiles;
				uLog(SESSION, "REQUEST_SYNC_N_START_LUA: %d source files", num_files);
				
				// serialized source files
				m_NetFileList.clear();
				
				for (int i = 0; i < num_files; i++)
				{
					const NetSourceFile	nsf(mis);
					
					const string	name = nsf.m_Name;
					
					uLog(SESSION, "    [%d] name %S (%zu bytes, %zu lines)", i, name, nsf.DataSize(), nsf.NumLines());
					
					m_NetFileList.push_back(nsf);
				}
				
				// sends NOTHING on success?

				return true;
			}	break;
			
			case CLIENT_MSG::NOTIFY_BREAKPOINTS:
			{	
				const NotifyBreakpoints	notify_bp(mis);
			
				uLog(SESSION, "received %zu breakpoints bf startup", notify_bp.GetBreakpoints().size());			// is error?
				
				m_LuaGlue.SaveAllBreakpoints(notify_bp);
			}	break;
			
		// for stress-test
			case CLIENT_MSG::REQUEST_ECHO_DATA:
			{
				const size_t	sz = mis.Read32();			// useless ?
				
				uMsg("REQUEST_ECHO_DATA %zu bytes (0x%0X)", sz, sz);
				
				const vector<uint8_t>	buff = mis.ReadSTLBuffer();
				const size_t	read_sz = buff.size();
				assert(sz == read_sz);
				
				MemDataOutputStream	mos;
				
				mos << DAEMON_MSG::REPLY_ECHO_DATA << (uint32_t) sz;
				mos.WriteSTLBuffer(buff);
				
				SendMessage(std::move(mos));			// should send header ID ?
			}	break;
			
			case CLIENT_MSG::REQUEST_ECHO_SIZE:
			{
				/*
				const size_t	sz = mis.Read32();
				
				uMsg("REQUEST_ECHO_SIZE %zu bytes (0x%0X)", sz, sz);
				
				vector<uint8_t>	buff(sz, 0);
				
				const size_t	read_sz = mis.ReadBuffer(&buff[0], sz);
				assert(sz == read_sz);
				
				MemDataOutputStream	mos;
				
				mos << DAEMON_MSG::REPLY_ECHO_SIZE << (uint32_t) sz;
				
				SendMessage(std::move(mos));			// should send header ID ?
				*/
			}	break;
			
			
			case CLIENT_MSG::REQUEST_VM_RESUME:
			case CLIENT_MSG::REQUEST_STACK:
			case CLIENT_MSG::REQUEST_LOCALS:
			case CLIENT_MSG::REQUEST_GLOBALS:
			case CLIENT_MSG::REQUEST_WATCHES:
			case CLIENT_MSG::REQUEST_SOLVE_VARIABLE:
			default:
			{	// error
				uErr("ERROR: unexpected client message in NetHandshakeLoop()");
				
				// error-for-error?
				SendMessage(DAEMON_MSG::UNEXPECTED_CLIENT_MESSAGE_DURING_SYNC, msg_t);
			}	break;
		}
	}
	
	uLog(SESSION, "NetHandshakeLoop() EXITED");
	
	return false;
}

//---- Load (1st) Lua File ----------------------------------------------------

VM_ERR	Daemon::loadfile(lua_State *L, const string &lua_filename)
{
	assert(L);
	assert(!lua_filename.empty());
	
	const VM_ERR	err = m_LuaGlue.loadfile(L, lua_filename);
	
	return err;
}

//---- Lua pcall --------------------------------------------------------------

void	Daemon::pcall(lua_State *L, int nargs, int nresults, const string &debug_s)
{
	assert(L);
	
	m_LuaGlue.pcall(L, nargs, nresults, debug_s);
}

//---- Is Connected -----------------------------------------------------------

bool	Daemon::IsConnected(void) const
{
	return m_ServerTCP.IsConnected();
}

//---- Notify Session Ended ---------------------------------------------------

bool	Daemon::SessionEnded(const int cod)
{
	return SendSessionEndMessage(cod);
}

//---- Send Message to Client -------------------------------------------------

bool	Daemon::SendMessage(const MemDataOutputStream &&mos)
{	
	if (!m_ServerTCP.IsConnected())		return false;
	
	LogOutgoingMessage(mos);
	
	return m_ServerTCP.WriteMessage(mos);
}

//---- Read Message -----------------------------------------------------------

bool	Daemon::ReadMessage(vector<uint8_t> &recv_msg)
{
	if (!m_ServerTCP.IsConnected())	return false;
	
	return m_ServerTCP.ReadMessage(recv_msg/*&*/);
}

//---- Send Daemon Ended Message ----------------------------------------------

bool	Daemon::SendSessionEndMessage(const uint32_t &res)
{
	if (!m_ServerTCP.IsConnected())	return false;
	
	const bool	ok = SendMessage(DAEMON_MSG::NOTIFY_SESSION_ENDED, res);		
				
	return ok;
}

//---- Log Outgoing Message ---------------------------------------------------

bool	Daemon::LogOutgoingMessage(const MemDataOutputStream &mos) const
{
	vector<uint8_t>	header_buff(4, 0);

	const bool	ok = mos.CloneTo(&header_buff[0], header_buff.size());
	assert(ok);
	
	MemDataInputStream	mis(header_buff);
	assert(mis.IsOk());
	
	DAEMON_MSG	msg_t;
	
	mis >> msg_t;
	
	uLog(MSG_OUT, "%s", MessageName(msg_t));
	
	return true;
}

//---- Enable UDP Log Levels --------------------------------------------------

void	Daemon::EnableNetLogLevels(const unordered_set<LogLevel> &levels)
{
	m_NetLogLevelSet.insert(levels.begin(), levels.end());
	
	// enable all root levels too
	rootLog::Get().EnableLevels(levels);
}

//---- LogSlot IMP ------------------------------------------------------------

void	Daemon::LogAtLevel(const timestamp_t stamp, const LogLevel level, const string &msg)
{
	if (!m_NetLogLevelSet.count(level))		return;		// level disabled
	
	const IJitGlue	*jit_glue = m_LuaGlue.GetJitGlue();
	if (jit_glue && jit_glue->IsProfiling())	return;		// mute during PROFILER

	m_ServerTCP.QueueUDPLog(stamp, level, msg);
}

//---- Log Lua Message (no filtering) -----------------------------------------

// static
void	IDaemon::LogLuaMsg(const string &msg)
{
	if (!Daemon::s_ServerInst)	return;		// should be error?
	
	const timestamp_t	stamp{};
	
	Daemon::s_ServerInst->QueueUDPLog(stamp, LUA_USER, msg);
	
	uLog(LUA_USER, msg);
}

//---- INSTANTIATE ------------------------------------------------------------

// static
IDaemon*	IDaemon::Create(const int server_port, const int sleep_ms)
{
	return new Daemon(server_port, sleep_ms);
}

// nada mas