// Lua DDT glue

#include <memory>
#include <cassert>
#include <regex>

#include <setjmp.h>				// portable???

#ifndef LUA_51
	#error "LUA_51 wasn't defined"
#endif

#ifndef DDT_JIT
	#error "DDT_JIT wasn't defined"
#endif

#ifndef LUA_CXX
	// #error "LUA_CXX wasn't defined"
	#define	LUA_CXX		0
#endif

#include "ddt/sharedDefs.h"

#include "lx/stream/MemDataInputStream.h"
#include "lx/stream/MemDataOutputStream.h"

#include "ddt/FileSystem.h"			// wasn't included before 2014-04-16

#include "lx/xutils.h"

#include "lx/misc/autolock.h"		// bullshit

#include "ddt/sharedLog.h"

#include "DaemonImp.h"

#include "LuaGlue.h"

using namespace std;
using namespace LX;
using namespace DDT;
using namespace LX::FileName;

#define		LOG_BREAKPOINT_FILE_LUTS	0

const int	MAX_STEP_OVER_FILE_PATH_LEN = 256;

#define	def_ent(arg)	{ENTRY_TYPE::arg, #arg}

static const
unordered_map<ENTRY_TYPE, string, EnumClassHash>	s_EntryTypeNameMap
{
	def_ent(UNDEFINED_ENTRY_TYPE),
	def_ent(HOOK),
	def_ent(INLINE_BREAKPOINT_CALL),
	def_ent(BREAKPOINT),
	def_ent(VM_ERROR_CALL),
	def_ent(VM_ASSERT_CALL),
	def_ent(VM_ERROR_HANDLER),
	def_ent(LOAD_FILE_ERROR)
};

#undef def_ent

static const
unordered_set<ENTRY_TYPE, EnumClassHash>	s_EntryLuaErrSet
{
	ENTRY_TYPE::LOAD_FILE_ERROR,
	ENTRY_TYPE::VM_ERROR_HANDLER,
	ENTRY_TYPE::VM_ERROR_CALL,
	ENTRY_TYPE::VM_ASSERT_CALL
};

std::string	LuaErrorTypeName(const VM_ERR &lua_err_t)
{
	switch (lua_err_t)
	{	
		case VM_ERR::LUAE_OK:			return "LUA_OK";			// 0
		case VM_ERR::LUAE_YIELD:		return "LUA_YIELD";			// 1
		case VM_ERR::LUAE_ERRRUN:		return "LUA_ERRRUN";			// 2
		case VM_ERR::LUAE_ERRSYNTAX:		return "LUA_ERRSYNTAX";			// 3
		case VM_ERR::LUAE_ERRMEM:		return "LUA_ERRMEM";			// 4
		case VM_ERR::LUAE_ERRGCMM:		return "LUA_ERRGCMM";			// 5
		case VM_ERR::LUAE_ERRERR:		return "LUA_ERRERR";			// 6
		case VM_ERR::LUAE_ERRFILE:		return "LUA_ERRFILE";			// 7
		case VM_ERR::LUAE_ERRILLEGAL:		return "LUA_ERRILLEGAL";		// -1
		// default:				assert(false);
	}
	
	return "LUA_ERRUNDEFINED";
}

// won't work on iOS (no idea why - makes no sense)
// const char*	DDT_REGISTRY_STRING_KEY = "DDT_REGISTRY_KEY";

static
LuaGlue		*s_LuaGlue = nil;

// faster AND MORE RELIABLE
static
const uint8_t	*s_BreakpointTab;
static
size_t		s_BreakpointTabLen;
static
int		s_StepOverLineStart, s_StepOverLineEnd;
static
char		s_StepOverFileNameBuff[MAX_STEP_OVER_FILE_PATH_LEN];
static
const char	*s_StepOverFileName;
static
int		s_StepDepth;

static
StringSet	s_Debug_BreakpointFileLUTs;

static const
unordered_map<CLIENT_MSG, DAEMON_MSG, EnumClassHash>	s_RequestReplyMap
{
	// {CLIENT_MSG::REQUEST_STACK,		DAEMON_MSG::REPLY_STACK},
	{CLIENT_MSG::REQUEST_LOCALS,		DAEMON_MSG::REPLY_LOCALS},
	{CLIENT_MSG::REQUEST_GLOBALS,		DAEMON_MSG::REPLY_GLOBALS},
	{CLIENT_MSG::REQUEST_WATCHES,		DAEMON_MSG::REPLY_WATCHES},
	{CLIENT_MSG::REQUEST_SOLVE_VARIABLE,	DAEMON_MSG::REPLY_SOLVED_VARIABLE},		// redundant
};

//---- CTOR -------------------------------------------------------------------

	LuaGlue::LuaGlue(Daemon &daemon)
		: m_Daemon(daemon),
		m_LastLuaError(ENTRY_TYPE::UNDEFINED_ENTRY_TYPE)			// kindof bullshit?
{
	Reset();
}

//---- DTOR -------------------------------------------------------------------

	LuaGlue::~LuaGlue()
{
	s_LuaGlue = nil;
	m_JitGlue = nil;
}

void	LuaGlue::Reset(void)
{
	m_JitGlue = nil;
	s_LuaGlue = this;
	
	m_EntryRecurseLock = false;
	m_StepOverLineStart = -1;
	m_StepOverLineEnd = -1;
	s_StepOverFileName = nil;
	s_Debug_BreakpointFileLUTs.clear();
	s_StepOverLineStart = s_StepOverLineEnd = 0;

	m_ResumeCommand = CLIENT_CMD::IDLE;
	m_AbortLevel = DAEMON_ABORT::NOT_ABORT;
	m_LastLuaError = ENTRY_TYPE::UNDEFINED_ENTRY_TYPE;
	
	m_FileNotificationQueue.clear();
}

//---- Get Handle to LuaGlue --------------------------------------------------

extern
LuaGlue*	GetLuaGlue(lua_State *L)
{
	assert(L);
	assert(s_LuaGlue);
	
	#if 1
		return s_LuaGlue;
	#else
		// doesn't work on iOS!
		const int	sp0 = lua_gettop(L);
		
		lua_getfield(L, LUA_REGISTRYINDEX, DDT_REGISTRY_STRING_KEY);
		const string	typ_s = lua_typename(L, lua_type(L, -1));
		assert(LUA_TLIGHTUSERDATA == lua_type(L, -1));
		
		LuaGlue	*luaGlue = static_cast<LuaGlue*> (lua_touserdata(L, -1));
		lua_pop(L, 1);
		
		const int	sp1 = lua_gettop(L);
		assert(sp0 == sp1);
		
		return luaGlue;
	#endif
}
	
//---- Breakpoint -------------------------------------------------------------

static
int	lg_BreakPoint(lua_State *L)
{
	assert(L);
	
	LuaGlue	*lglue = GetLuaGlue(L);
	assert(lglue);
	
	// DON'T check nargs - go to debugger anyway
	lglue->OnDebuggerEntry(L, ENTRY_TYPE::INLINE_BREAKPOINT_CALL, nil/*no activation record*/);
	
	return 0;
}

//---- error() LUA OVERLOAD ---------------------------------------------------

static
int	lg_error(lua_State *L)
{
	assert(L);
	LuaGlue	*lglue = GetLuaGlue(L);
	assert(lglue);
	
	uLog(LUA_ERR, "LuaGlue::lg_error()");
	
	// const int	nargs = lua_gettop(L);
	// const char	*typ_s = lua_typename(L, lua_type(L, -1));
		
	// go to lua debugger
	const bool	continue_f = lglue->OnDebuggerEntry(L, ENTRY_TYPE::VM_ERROR_CALL, nil/*no activation record*/);
	(void)continue_f;
		
	return 0;
}

//---- assert() Lua OVERLOAD --------------------------------------------------

static
int	lg_assert(lua_State *L)
{
	assert(L);
	
	const int	nargs = lua_gettop(L);
	if ((nargs > 0) && lua_toboolean(L, 1))		return nargs;		// passed
	
	uLog(LUA_ERR, "LuaGlue::lg_assert() FAILED");
	
	// failed assert
	LuaGlue	*lglue = GetLuaGlue(L);
	assert(lglue);
	
	// go to lua debugger
	const bool	continue_f = lglue->OnDebuggerEntry(L, ENTRY_TYPE::VM_ASSERT_CALL, nil/*no activation record*/);
	(void)continue_f;
		
	return 0;
}

//---- DDT Profiler on/off ----------------------------------------------------

static
int	lg_ddtProfiler(lua_State *L)
{
	assert(L);
	
	LuaGlue	*lglue = GetLuaGlue(L);
	assert(lglue);
	
	const int	nargs = lua_gettop(L);
	assert(nargs >= 1);
	
	#if (DDT_JIT == 1)
		const bool	f = (lua_toboolean(L, 1) != 0);
		const int	interval_ms = luaL_optinteger(L, 2, DDT_DAEMON_DEFAULT_PROFILER_INTERVAL_MS);
	
		lglue->OnSetProfiler(L, f, interval_ms);
	#endif
	
	return 0;
}

//---- Init Lua Hooks ---------------------------------------------------------

bool	LuaGlue::InitLuaHooks(lua_State *L, const vector<NetSourceFile> &net_file_list, IJitGlue *jglue, const bool break_asap_f)
{
	uLog(GLUE, "LuaGlue::InitLuaHooks(), %zu net_files, break_asap: %c", net_file_list.size(), break_asap_f);	
	
	assert(L);
	assert(jglue);
	
	m_JitGlue = jglue;
	m_BreakASAPFlag = break_asap_f;
	m_EntryRecurseLock = false;
	m_pcallRecurseLock = false;
	
	m_NetFileMap.clear();
	m_FileInstantiatedSet.clear();
	
	m_ResumeCommand = CLIENT_CMD::IDLE;
	m_AbortLevel = DAEMON_ABORT::NOT_ABORT;
	m_LastLuaError = ENTRY_TYPE::UNDEFINED_ENTRY_TYPE;
	
	// register self - crashes on iOS!
	// lua_pushlightuserdata(L, (void*) this);
	// lua_setfield(L, LUA_REGISTRYINDEX, DDT_REGISTRY_STRING_KEY);
	
	lua_pushcfunction(L, lg_BreakPoint);
	lua_setglobal(L, "breakpoint");
	
	lua_pushcfunction(L, lg_error);
	lua_setglobal(L, "error");
	
	lua_pushcfunction(L, lg_assert);
	lua_setglobal(L, "assert");
	
	lua_pushcfunction(L, lg_ddtProfiler);
	lua_setglobal(L, "ddtProfiler");
	
	InsertOverriderHook(L);
	
	PatchDebianSearchPaths(L);
	
	PreloadFiles(L, net_file_list);
	
	return true;
}

//---- Exit Lua Hooks ---------------------------------------------------------

bool	LuaGlue::ExitLuaHooks(lua_State *L)
{
	uLog(GLUE, "LuaGlue::ExitLuaHooks()");	
	
	assert(L);
	
	m_EntryRecurseLock = false;
	m_pcallRecurseLock = false;
	
	// remove debug functions
	lua_pushnil(L);
	lua_setglobal(L, "breakpoint");
	
	lua_pushnil(L);
	lua_setglobal(L, "ddtProfiler");
	
	// need RemoveOverriderHook()				// FIXME?
	
	// restore original error function
	// lua_pushcfunction(L, lua_error);
	// lua_setglobal(L, "error");				// incorrect?
	
	// lua_pushcfunction(L, lua_assert);
	// lua_setglobal(L, "assert");
	
	m_NetFileMap.clear();
	m_FileInstantiatedSet.clear();
	m_AllSavedBreakpoints.clear();
	
	s_LuaGlue = nil;
	m_JitGlue = nil;
	
	return true;
}

//---- Is Internal Abort ? ----------------------------------------------------

bool	LuaGlue::IsInternalAbort(void) const
{
	const bool	f = (m_AbortLevel != DAEMON_ABORT::NOT_ABORT);
	if (f)		uErr("IsInternalAbort()");
	
	return f;
}

//---- Do Internal Abort ------------------------------------------------------

int	LuaGlue::DoInternalAbort(lua_State *L, const DAEMON_ABORT &abort_code)
{
	uLog(GLUE, "LuaGlue::DoInternalAbort(code %d)", (int) abort_code);
	uLog(STACK, "  DoInternalAbort() stack @ %d", lua_gettop(L));
	assert(L);
	
	m_AbortLevel = abort_code;
	
	// remove hook
	lua_sethook(L, nil, 0, 0);
	
	// push abort code
	lua_pushinteger(L, static_cast<int>(abort_code));
	
	// prevent lua_errror() on load file error -- would PANIC
	if (m_LastLuaError == ENTRY_TYPE::LOAD_FILE_ERROR)				// NOT RE-ENTRANT!!!
	{	uErr("ABORT as m_LastLuaError == ENTRY_TYPE::LOAD_FILE_ERROR");
		return 1;
	}
	
	// clear RAII before lua_error() !!
	if (m_pcallRecurseLock)
	{	uLog(RECURSE, "RECURSIVE DoInternalAbort() lock cleared");
		m_pcallRecurseLock = false;
	}
	
	return lua_error(L);			// Lua 5.2 dos: THROWS an error, never returns
}

//---- On Set Profiler on/off -------------------------------------------------

void	LuaGlue::OnSetProfiler(lua_State *L, const bool f, const int interval_ms)
{
	uLog(GLUE, "LuaGlue::OnSetProfiler()");
	assert(L);
	assert(m_JitGlue);
	
	const bool	ok = m_JitGlue->ToggleProfiler(L, f, interval_ms, DDT_DAEMON_DEFAULT_PROFILER_MAX_DEPTH);
	if (!ok)	return;		// wasn't set
	
	if (f)
	{	// enable profiler / disable hook
		lua_sethook(L, nil, 0, 0);
	}
	else
	{	// disable profiler / RESTORE hook
		SetLuaHook(L, m_SavedHookType);
	}
}

//---- Queue Client message locally (IMPLEMENTATION) --------------------------

void	LuaGlue::QueueMessage(const vector<uint8_t> &buff)
{
	Message		msg(buff);
	
	// blocking
	m_DaemonQueue.Put(msg);
}

//---- Process Custom Queue ---------------------------------------------------

bool	LuaGlue::ProcessQueue(lua_State *L)
{
	if (!m_DaemonQueue.HasPending())		return false;
	
	Message	msg;
	
	if (!m_DaemonQueue.Get(msg/*&*/))		return false;
	
	bool	ok = DispatchMessage(L, msg);
	assert(ok);
	
	return ok;
}

//---- Dispatch Client Message ------------------------------------------------

bool	LuaGlue::DispatchMessage(lua_State *L, const Message &msg)
{
	const size_t	sz = msg.GetSize();
	const uint8_t	*p = msg.GetPtr();
	
	MemDataInputStream	mis(p, sz);
	
	CLIENT_MSG	msg_t;
	
	mis >> msg_t;
	
	uLog(MSG_IN, "dispatching %s", MessageName(msg_t));
	
	switch (msg_t)
	{
		case CLIENT_MSG::REQUEST_VM_RESUME:
		{
			const ResumeVM	resume(mis);
			
			m_ResumeCommand = resume.GetClientCmd();
			m_AbortLevel = DAEMON_ABORT::NOT_ABORT;		// (clear)
			
			// do NOT let abort -- would longjump() without prior setjmp(), i.e. would PANIC
			if (m_LastLuaError == ENTRY_TYPE::LOAD_FILE_ERROR)
			{
				return true;
			}
			
			// pre-empt
			if (resume.GetClientCmd() == CLIENT_CMD::ABORT)		return DoInternalAbort(L, DAEMON_ABORT::CLIENT_REQUEST_ABORT);
			if (resume.GetClientCmd() == CLIENT_CMD::END_SESSION)	return DoInternalAbort(L, DAEMON_ABORT::CLIENT_REQUEST_END_SESSION);
			
			if (resume.GetClientCmd() == CLIENT_CMD::STEP_INTO_INSTRUCTION)
			{
				uLog(GLUE, "WARNING: about to collect Lua GC");
				lua_gc(L, LUA_GCCOLLECT, 0);
			}
			
		}	break;
		
		case CLIENT_MSG::REQUEST_STACK:
		{
			m_Daemon.SendMessage(DAEMON_MSG::REPLY_STACK, m_DaemonStack.ToClientStack(*this));
		}	break;
			
		case CLIENT_MSG::REQUEST_LOCALS:
		case CLIENT_MSG::REQUEST_GLOBALS:
		case CLIENT_MSG::REQUEST_WATCHES:
		case CLIENT_MSG::REQUEST_SOLVE_VARIABLE:
		{
			VariableRequest	req(mis);
			
			RequestVariables(L, msg_t, req);
		}	break;
		
		case CLIENT_MSG::NOTIFY_BREAKPOINTS:
		{	
			NotifyBreakpoints	notify_bp(mis);
			
			SaveAllBreakpoints(notify_bp);
		}	break;
		
		default:
		{	// Error
			uErr("ERROR - unexpected client message in debug loop");
			
			m_Daemon.SendMessage(DAEMON_MSG::UNEXPECTED_CLIENT_MESSAGE_DURING_DEBUG, msg_t);
		}	return false;
	}

	return true;	// ok
}

//---- Request Variable List --------------------------------------------------

void	LuaGlue::RequestVariables(lua_State *L, const CLIENT_MSG &msg_t, const VariableRequest &req)
{
	// brutal stack deep-copy? - FIXME
	m_Collectors.CopyStack(m_DaemonStack, req.ExpandedTables(), req.HideMask());
	
	const int	stack_depth = req.GetStackLevel();
	
	const int	top = lua_gettop(L);
	
	// prevent var collection if had load error (since stack is fake)
	const bool	nop_f = (ENTRY_TYPE::LOAD_FILE_ERROR == m_LastLuaError);
	if (nop_f)
	{
		uErr("error - LuaGlue::RequestVariables() with previous error");
	}
	
	MemDataOutputStream	mos;
	
	assert(s_RequestReplyMap.count(msg_t));
	const DAEMON_MSG	reply_msg_t = s_RequestReplyMap.at(msg_t);
	
	mos << reply_msg_t << req.RequesterID();
	
	switch (msg_t)
	{	
		case CLIENT_MSG::REQUEST_LOCALS:
		{
			if (nop_f)	m_Collectors.CollectNOP(mos/*&*/);
			else		m_Collectors.CollectLuaLocals(L, stack_depth, req.HideMask(), mos/*&*/);
			
			assert(top == lua_gettop(L));
		}	break;
		
		case CLIENT_MSG::REQUEST_GLOBALS:							// incl. registry
		{	
			if (nop_f)	m_Collectors.CollectNOP(mos/*&*/);
			else		m_Collectors.CollectLuaGlobals(L, stack_depth, req.HideMask(), mos/*&*/);
		}	break;
		
		case CLIENT_MSG::REQUEST_WATCHES:
		{	
			const StringList	watches {req.WatchNames()};
			if (nop_f)	m_Collectors.CollectNOPWatches(L, watches, mos/*&*/);
			else		m_Collectors.CollectLuaWatches(L, watches, mos/*&*/);
		}	break;
		
		case CLIENT_MSG::REQUEST_SOLVE_VARIABLE:
		{
			assert(req.WatchNames().size() > 0);
			
			const StringList	var_list {req.WatchNames().front()};			// gets only ONE ?
			
			if (nop_f)	m_Collectors.CollectNOPWatches(L, var_list, mos/*&*/);
			else		m_Collectors.CollectLuaWatches(L, var_list, mos/*&*/);
		}	break;
			
		default:
			
			assert(false);
			break;
	}
	
	assert(top == lua_gettop(L));
	
	m_Daemon.SendMessage(std::move(mos));
}

//---- Load File --------------------------------------------------------------

VM_ERR	LuaGlue::loadfile(lua_State *L, const string &lua_filename)
{
	assert(L);
	assert(!lua_filename.empty());
	
	uLog(INTERCEPT, "LuaGlue::loadfile(%S)", lua_filename);
	uLog(STACK, "  stack @ %d", lua_gettop(L));
	
	const SplitPath	split = SplitUnixPath(lua_filename);
	assert(split);
	
	// make sure file definition exists
	assert(m_NetFileMap.count(split.Name()));
	
	const VM_ERR	res = InstantiateSource_LL(L, split.Name());
	if (res != VM_ERR::LUAE_OK)
	{	// error
		uErr("Error in LuaGlue::loadfile(%S)", lua_filename);
		return res;
	}
	
	if (!lua_isfunction(L, -1))
	{	// error
		uErr("Error in LuaGlue::loadfile(%S) no function returned", lua_filename);
		return VM_ERR::LUAE_ERRILLEGAL;
	}
	
	return VM_ERR::LUAE_OK;
}

//---- Lua Error Callback -----------------------------------------------------

	// called before stack unwinds (raw error string will be on top of stack)
	
static
int	LuaErrorHandler(lua_State *L)
{
	assert(L);
	uLog(GLUE, "LuaGlue::LuaErrorHandler()");
	uLog(STACK, "  stack @ %d", lua_gettop(L));
	
	LuaGlue	*lglue = GetLuaGlue(L);
	assert(lglue);
	
	// const int	nargs = lua_gettop(L);
	// const char	*typ1_s = lua_typename(L, lua_type(L, -1));
	
	if (lua_isnumber(L, -1))
	{
		const int	code = luaL_checknumber(L, -1);
		switch (code)
		{
			case DAEMON_ABORT::CLIENT_REQUEST_ABORT:
			case DAEMON_ABORT::CLIENT_REQUEST_END_SESSION:
			case DAEMON_ABORT::DAEMON_NETWORK_ERROR:
			
				// don't re-enter debugger
				// should RE-PUSH code for pcall() below ?
				return 0;
				break;
				
			default:
			
				break;
		}
	}
	
	try
	{
		const bool	continue_f = lglue->OnDebuggerEntry(L, ENTRY_TYPE::VM_ERROR_HANDLER, nil/*no activation record*/);
		(void)continue_f;
		
		// if continuing return the error string (or will never get here?)
		// return continue_f ? 1 : 0;
		return 0;
	}
	catch (...)
	{
		// EXCEPTION IN ERROR FUNCTION!!!
		uFatal("LuaGlue::LuaErrorHandler() exception!");
		return 0;
	}
}

//---- Protected, DDT-Enabled Lua call ----------------------------------------

	// a protected call should never return an error?

void	LuaGlue::pcall(struct lua_State *L, int nargs, int nresults, const string &debug_s)
{
	assert(L);
	
	if (m_pcallRecurseLock)
		uLog(RECURSE, "RECURSIVE LuaGlue::pcall(%d, %d), debug %S", nargs, nresults, debug_s);		// bullshit?
	else
	{
		if (debug_s.empty())
			uLog(GLUE, "LuaGlue::pcall(%d, %d)", nargs, nresults);
		else	uLog(GLUE, "LuaGlue::pcall(%d, %d)(%S)", nargs, nresults, debug_s);
	}
	
	uLog(STACK, "  stack @ %d", lua_gettop(L));
	
	AutoLock	lock(m_pcallRecurseLock);							// RAII won't work with longjumps!!!
	
	SetLuaHook(L, m_BreakASAPFlag ? BREAK_TYPE::INSTRUCTION : BREAK_TYPE::BREAKPOINTS);
	
	const int	top = lua_gettop(L);
	const int	base = top - nargs;	// function index
	lua_pushcfunction(L, LuaErrorHandler);	// push error handling function
	lua_insert(L, base);			// put it under chunk and args

	const int	err = lua_pcall(L, nargs, nresults, base/*error function index*/);

	lua_remove(L, base);			// remove error handling function
	
	if (!err)		return;		// no error
	
	// doesn't receive value passed from InternalAbort()
	if (err == LUA_ERRMEM)
	{
		uErr("lua_pcall() returned LUA_ERRMEM");
		uLog(STACK, "  stack @ %d", lua_gettop(L));
		return;
	}
	
	assert(err == LUA_ERRRUN);
	
	uErr("lua_pcall() returned error %d (ignored)", err);
	uLog(STACK, "  stack @ %d", lua_gettop(L));
}

// clang bug workaround
inline
bool	IsLuaMain(lua_Debug *hook_ar)
{
	if (!hook_ar)		return false;
	
	bool	f = (string{"main"} == hook_ar->what);
	
	return f;
}

//---- On Entry from Lua ------------------------------------------------------

bool	LuaGlue::OnDebuggerEntry(lua_State *L, const ENTRY_TYPE entry_t, lua_Debug *hook_ar)
{	
	assert(L);
	assert(s_EntryTypeNameMap.count(entry_t));
	
	uLog(GLUE, "LuaGlue::OnDebuggerEntry(%s)", s_EntryTypeNameMap.at(entry_t));
	if (IsInternalAbort())
	{	// PREVENT DEBUGGER RE-ENTRY
		uLog(RECURSE, "  averted debugger re-entry");				// bullshit?
		return false;
	}
	
	assert(!m_EntryRecurseLock);
	LX::AutoLock	recurse_lock(m_EntryRecurseLock);
	
	// const bool	main_f = hook_ar && (string{"main"} == hook_ar->what);		// clang doesn't stop at nil hook ?
	const bool	main_f = IsLuaMain(hook_ar);
	(void)main_f;
	
	if (entry_t != ENTRY_TYPE::LOAD_FILE_ERROR)
	{
		const string	source = hook_ar ? hook_ar->source : "no hook ptr";
		
		// fill stack levels
		m_DaemonStack.Fill(L, m_Collectors);					// collector class should be static?
		assert(!m_DaemonStack.IsEmpty());
	}
	else
	{	// load file error
		// - has built a FAKE stack upstream
	}
	
	const int	first_level = m_DaemonStack.FirstLuaLevel();
	assert(first_level >= 0);
	
	// const string	fn = m_DaemonStack.m_Levels[first_level].m_OrgFileName;
	// const string	v_module_fn = m_DaemonStack.m_Levels[first_level].m_CleanFileName;
	// assert(m_NetFileMap.count(v_module_fn));
	// const string	fn = m_NetFileMap.at(v_module_fn).ShortFilename();
	const string	fn = m_DaemonStack.m_Levels[first_level].m_CleanFileName;
	const int	ln = m_DaemonStack.m_Levels[first_level].m_Line;
	
	// flush any user logs
	// m_Daemon.FlushUserLogs();
	
	string	err_s;
	
	if (s_EntryLuaErrSet.count(entry_t))
	{	// lua VM error
		m_LastLuaError = entry_t;
		
		// const int	sp = lua_gettop(L);
		stringstream	ss;
		
		switch (entry_t)
		{
			case ENTRY_TYPE::LOAD_FILE_ERROR:
			case ENTRY_TYPE::VM_ERROR_HANDLER:
			
				ss << lua_tostring(L, -1);
				break;
				
			case ENTRY_TYPE::VM_ERROR_CALL:
			{
				ss << "error()";
			}	break;
			
			case ENTRY_TYPE::VM_ASSERT_CALL:
			{	
				ss << "assert() failed";
				
				// serialize any explicit args (after 1st arg)
				// for (int i = 2; i <= sp; i++)	ss << lua_tostring(L, i) << ((i != sp) ? ", " : "");
			}	break;
				
			default:
			
				assert(0);
				break;
		}
		
		err_s = ss.str();
		
		uLog(LUA_ERR, "Lua VM error %S", err_s);
	}
	
	{	// send stack
		m_Daemon.SendMessage(DAEMON_MSG::REPLY_STACK, m_DaemonStack.ToClientStack(*this));
	}	
	
	{	if (m_FileNotificationQueue.size() > 0)
		{
			assert(m_FileNotificationQueue.size() > 0);
		
			m_Daemon.SendMessage(DAEMON_MSG::NOTIFY_SCRIPT_LOADED, m_FileNotificationQueue);
			
			m_FileNotificationQueue.clear();
		}

	// send vm suspended command
	
		// remove arobas prefix
		const size_t	arobas_skip = (fn[0] == '@') ? 1 : 0;
		
		SuspendState		suspended(string(fn, arobas_skip), ln, err_s);
		
		m_Daemon.SendMessage(DAEMON_MSG::NOTIFY_VM_SUSPENDED, suspended);
		
		uLog(GLUE, "VM suspended at %s:%d", suspended.m_Source, ln);
	}

	/*	
	// if is raw string, not a filename
	if (hook_ar && (hook_ar->source[0] != '@'))							// was for string-embedded Lua code
	{	// set next (real) hook - NOT RIGHT HOOK?
		SetLuaHook(L, m_BreakASAPFlag ? BREAK_TYPE::INSTRUCTION : BREAK_TYPE::BREAKPOINTS);
		
		return true;		// continue
	}
	*/	

	uLog(GLUE, "Daemon PAUSING");

	m_ResumeCommand = CLIENT_CMD::IDLE;

	// process requests while isn't resuming
	while (CLIENT_CMD::IDLE == m_ResumeCommand)
	{	
		vector<uint8_t>	buff;
		
		bool	ok = m_Daemon.ReadMessage(buff/*&*/);
		if (!ok)
		{	if (m_Daemon.IsConnected())		uErr("FAILURE in daemon listen loop, aborting");
			// else client disconnected
			return DoInternalAbort(L, DAEMON_ABORT::DAEMON_NETWORK_ERROR);
		}
		
		// post message to self
		QueueMessage(buff);
		
		// process queue
		ok = ProcessQueue(L);
		assert(ok);
		
		// assert(!IsInternalAbort());			// (will never make it this far)
	}
	
	uLog(GLUE, "Daemon RESUMED with %s", CommandName(m_ResumeCommand));
	
	// if got next command, set it up
	ResumeDaemon(L, m_ResumeCommand);
	
	// ...and continue
	return true;
}

//---- Hook Function ----------------------------------------------------------

	// line / count hook

static
void	lg_HookFunction(lua_State *L, lua_Debug *ar)
{
	assert(L);
	assert(ar);
	
	// fill actual debug info (source filename and line#) -- CAPITAL S
	bool	ok = lua_getinfo(L, "Sl", ar);
	assert(ok);
	
	// ignore for now if source is string-embedded
	if (!ar->source || ('@' != ar->source[0]))		return;
	
	// don't break on C call (or could still want to inspect the call stack?)
	if (::strcmp("C", ar->what) == 0)	return;
	
	LuaGlue	*lglue = GetLuaGlue(L);
	
	lglue->OnDebuggerEntry(L, ENTRY_TYPE::HOOK, ar);
}

//---- StepOut Hook Function --------------------------------------------------

static
void	lg_StepOutHookFunction(lua_State *L, lua_Debug *ar)
{
	assert(L);
	assert(ar);
	
	#if DDT_JIT
		if (!curr_funcisL(L))		return;		// don't count non-Lua functions
	#endif
	
	// could be LUA_HOOKCALL (0), LUA_HOOKRET (1) or LUA_HOOKTAILCALL (4)
	//   currentline will be -1
	const int	step_sign = (LUA_HOOKCALL == ar->event) ? 1 : -1;
	s_StepDepth += step_sign;
	if (s_StepDepth >= 0)			return;		// didn't step out yet
	
	// fill actual debug info (source filename and line#) -- CAPITAL S
	bool	ok = lua_getinfo(L, "Sl", ar);
	assert(ok);

	// ignore for now if source is string-embedded
	if (!ar->source || ('@' != ar->source[0]))		return;		// mute
	// don't break on C call (or could still want to inspect the call stack?)
	assert('C' != ar->what[0]);
	
	LuaGlue	*lglue = GetLuaGlue(L);
	assert(lglue);
	
	// wait for ONE MORE hook before breaking
	lglue->SetLuaHook(L, BREAK_TYPE::INSTRUCTION);
	
	lglue->OnDebuggerEntry(L, ENTRY_TYPE::HOOK, ar);
}

//---- Breakpoint Hook Function -----------------------------------------------

static
void	lg_BreakpointHookFunction(lua_State *L, lua_Debug *ar)
{
	assert(L);
	assert(ar);
	
	// fast look-up (only currentline is valid now)
	const size_t	ln = ar->currentline;
	if ((ln >= s_BreakpointTabLen) || (!s_BreakpointTab[ln]))	return;
	
// matches line#, do more costly test
	
	// fill actual debug info (source filename and line#) -- CAPITAL S
	bool	ok = lua_getinfo(L, "Sl", ar);
	assert(ok);

	// ignore for now if source is string-embedded
	if (!ar->source || ('@' != ar->source[0]))		return;		// mute
	// make sure isn't in C call since breakpoints are only in Lua
	assert('C' != ar->what[0]);
	
	const char	*name = ar->source + 1;
	
	#if LOG_BREAKPOINT_FILE_LUTS
	
		// log breakpoint-tested filename
		const string	sname(name);
		assert(!sname.empty());
		
		if (s_Debug_BreakpointFileLUTs.count(sname) == 0)
		{
			uLog(BREAKPOINT, "lg_BreakpointHookFunction(name %S)", sname);
			
			s_Debug_BreakpointFileLUTs.insert(sname);
		}
	
	#endif
	
	LuaGlue	*lglue = GetLuaGlue(L);
	assert(lglue);
	
	if (!lglue->FastBreakpointExists(name, ln))	return;
	
// both line and filename match target breakpoint

	ok = lglue->OnDebuggerEntry(L, ENTRY_TYPE::BREAKPOINT, ar);
	// assert(ok);
}

//---- Step Over and Step Out Hook Function -----------------------------------

static
void	lg_StepOverAndOutHookFunction(lua_State *L, lua_Debug *ar)
{
	if (LUA_HOOKLINE == ar->event)
	{	// at this point only currentline is valid, fast range compare
		if ((ar->currentline < s_StepOverLineStart) || (ar->currentline > s_StepOverLineEnd))		return;
	}
	else
	{
		// could be LUA_HOOKCALL (0), LUA_HOOKRET (1) or LUA_HOOKTAILCALL (4)
		//    currentline will be -1 here
		#if DDT_JIT
			if (!curr_funcisL(L))				return;		// don't count non-Lua functions
			if (LUA_HOOKTAILRET == ar->event)		return;		// ignore tail call (is neither recurse-in nor out)
		#else
			if (LUA_HOOKTAILCALL == ar->event)		return;		// ignore tail call (is neither recurse-in nor out)
		#endif
		
		const int	step_sign = (LUA_HOOKCALL == ar->event) ? 1 : -1;
		int		&step_depth(s_StepDepth);
		
		step_depth += step_sign;
		if (step_depth >= 0)					return;		// didn't step out yet
		
	// one more step
		
		// fill actual debug info (source filename and line#)
		bool	ok = lua_getinfo(L, "Sl", ar);
		assert(ok);

		LuaGlue	*lglue = GetLuaGlue(L);
		assert(lglue);
		
		// wait for ONE MORE hook before breaking ???
		lglue->SetLuaHook(L, BREAK_TYPE::INSTRUCTION);
	
		return;
	}
	
// is within target line range
	
	// fill actual debug info (source filename and line#)
	bool	ok = lua_getinfo(L, "Sl", ar);
	assert(ok);

	// ignore for now if source is string-embedded
	if (!ar->source || ('@' != ar->source[0]))			return;		// mute
	// make sure isn't in C call since breakpoints are only in Lua
	assert(::strcmp("C", ar->what) != 0);
	
	// can compare without cleaning up name
	if (::strcmp(s_StepOverFileName, ar->source) != 0)		return;		// not our source
	
// both line range and source name match the step-over target

	LuaGlue	*lglue = GetLuaGlue(L);
	assert(lglue);
	
	lglue->OnDebuggerEntry(L, ENTRY_TYPE::HOOK, ar);
}

//---- Set Lua Hook -----------------------------------------------------------

void	LuaGlue::SetLuaHook(lua_State *L, const BREAK_TYPE &hook_type)
{
	assert(L);
	
	m_SavedHookType = hook_type;
	
	if (m_JitGlue && (m_JitGlue->IsProfiling()))
	{	// disable hooks during luajit PROFILER
		lua_sethook(L, nil, 0, 0);
		return;
	}
	
	switch (hook_type)
	{	
		case BREAK_TYPE::NONE:
			
			// remove hook
			lua_sethook(L, nil, 0, 0);
			break;
			
		case BREAK_TYPE::INSTRUCTION:
			
			// step-by-step (can't hook into class member function?)
			lua_sethook(L, lg_HookFunction, LUA_MASKCOUNT, 1);			// # of instructions til next hook callback
			break;
		
		case BREAK_TYPE::LINE:
			
			// step-by-step (can't hook into class member function?)
			lua_sethook(L, lg_HookFunction, LUA_MASKLINE, 0/*ignored*/);
			break;
		
		case BREAK_TYPE::FUNCTION_STEP_OUT:		
			
			// on function return
			lua_sethook(L, lg_StepOutHookFunction, LUA_MASKCALL | LUA_MASKRET, 0/*ignored*/);
			s_StepDepth = 0;
			break;
		
		case BREAK_TYPE::BREAKPOINTS:
			
			// on breakpoints, get fast breakpoint LUT and copy to static vars
			s_BreakpointTab = GetBreakpointLinesTab(s_BreakpointTabLen/*&*/);
			
			if (s_BreakpointTab)
			{	lua_sethook(L, lg_BreakpointHookFunction, LUA_MASKLINE, 0/*ignored*/);
			}
			else
			{	// has no breakpoints
				lua_sethook(L, nil, 0, 0);
			}
			break;
		
		case BREAK_TYPE::STEP_OVER:
			
			// on step-over
			lua_sethook(L, lg_StepOverAndOutHookFunction, LUA_MASKLINE | LUA_MASKCALL | LUA_MASKRET, 0/*ignored*/);		// also check on step-out
			s_StepDepth = 0;
			break;
			
		default:
			
			// ERROR
			assert(false);
			break;
	}
}

//---- Resume Daemon ----------------------------------------------------------

void	LuaGlue::ResumeDaemon(lua_State *L, CLIENT_CMD cmd)
{
	assert(L);
	
	uLog(GLUE, "LuaGlue::ResumeDaemon(%s)", CommandName(cmd));
	
	// assert(!IsInternalAbort());
	// assert(cmd != CLIENT_CMD::ABORT);
	// assert(cmd != CLIENT_CMD::END_SESSION);
	
	switch (cmd)
	{	case CLIENT_CMD::ABORT:
		case CLIENT_CMD::END_SESSION:
		
			SetLuaHook(L, BREAK_TYPE::NONE);
			break;
		
		case CLIENT_CMD::RUN:
		
			if (m_BreakpointSet.size() > 0)
				SetLuaHook(L, BREAK_TYPE::BREAKPOINTS);
			else	SetLuaHook(L, BREAK_TYPE::NONE);
			break;
		
		case CLIENT_CMD::STEP_INTO_LINE:
			
			// step line
			SetLuaHook(L, BREAK_TYPE::LINE);
			break;
			
		case CLIENT_CMD::STEP_INTO_INSTRUCTION:
			
			// step instruction
			SetLuaHook(L, BREAK_TYPE::INSTRUCTION);
			break;
			
		case CLIENT_CMD::STEP_OVER:
			
		{	// get top callstack level
			const DaemonStack::Level	&csl = m_DaemonStack.GetLevel(m_DaemonStack.FirstLuaLevel());
			
			// make sure current line is within valid line range
			assert(csl.m_ValidLines.size() > 0);
			assert(csl.m_Line >= csl.m_ValidLines.front());
			assert(csl.m_Line <= csl.m_ValidLines.back());
			
			// if is last line then there is no next valid line
			if (csl.m_Line == csl.m_ValidLines.back())
			{	// -> just step a normal line (not a step over)
				s_StepOverFileName = nil;
				s_StepOverLineStart = -1;
				s_StepOverLineEnd = -1;
				
				SetLuaHook(L, BREAK_TYPE::LINE);
			}
			else
			{	// setup step over hook
			
				// use org file name so can do a straight compare in the hook function -- was ILLEGAL use of TEMP variable!!!
				assert(csl.m_OrgFileName.length() < MAX_STEP_OVER_FILE_PATH_LEN);
				assert('@' == csl.m_OrgFileName[0]);
				strcpy(&s_StepOverFileNameBuff[0], csl.m_OrgFileName.c_str());
				s_StepOverFileName = &s_StepOverFileNameBuff[0];
				// line range to break on
				s_StepOverLineStart = csl.m_Line;				// starts at CURRENT line (is correct?)
				s_StepOverLineEnd = csl.m_ValidLines.back();
				
				SetLuaHook(L, BREAK_TYPE::STEP_OVER);
			}
		}	break;
		
		case CLIENT_CMD::STEP_OUT:
		{	
			// get top callstack level
			const DaemonStack::Level	&csl = m_DaemonStack.GetLevel(m_DaemonStack.FirstLuaLevel());
			
			// make sure current line is within valid line range
			assert(csl.m_ValidLines.size() > 0);
			assert(csl.m_Line >= csl.m_ValidLines.front());
			assert(csl.m_Line <= csl.m_ValidLines.back());
			
			// set a hook on function call IN and functio call OUT, then break when negative
			//   doesn't count non-Lua functions because luajit doesn't trigger return hook for native function 
			SetLuaHook(L, BREAK_TYPE::FUNCTION_STEP_OUT);
			// Lua has only one hook function - BUT can set multiple bits (f.ex. on a "superhook" function)
			//   will be called MULTIPLE times, at least under LuaJIT
		}	break;
			
		default:
			
			assert("unsupported CLIENT_CMD");
			break;
	}
}

//---- Save All Breakpoints (including for non-loaded sources) ----------------

void	LuaGlue::SaveAllBreakpoints(const NotifyBreakpoints &notify_bp)
{
	uLog(BREAKPOINT, "SaveAllBreakpoints(%d)", (int) notify_bp.GetBreakpoints().size());
	
	m_AllSavedBreakpoints = notify_bp.GetBreakpoints();
	
	#if LOG_BREAKPOINT_FILE_LUTS
	
		for (const auto bp : m_AllSavedBreakpoints)	uLog(BREAKPOINT, " bp(all) %S", bp);
		
	#endif
	
	// process now (no harm done)
	SetValidBreakpoints();
}

//---- Set Valid Breakpoints (for loaded sources) -----------------------------

void	LuaGlue::SetValidBreakpoints(void)
{
	m_BreakpointSet.clear();
	
	#if LOG_BREAKPOINT_FILE_LUTS
		s_Debug_BreakpointFileLUTs.clear();
	#endif
	
	uLog(BREAKPOINT, "LuaGlue::SetValidBreakpoints()");
	
	// sub-match unnecessary ?
	static const regex	re("^((.*)\\.lua):(\\d+)$", regex::ECMAScript | regex::optimize);
	
	for (const string bp_key : m_AllSavedBreakpoints)
	{	
		smatch	matches;
		
		const bool	ok = regex_match(bp_key, matches/*&*/, re);
		assert(ok);
		assert(matches.size() == 4);
		
		const string	filename = matches[1];
		if (!m_FileInstantiatedSet.count(filename))			continue;		// source not instantiated yet
		
		const string	ln_s = matches[3];
		const string	daemon_bp_key = filename + ":" + ln_s;
		
		uLog(BREAKPOINT, "  bp %s", daemon_bp_key);
		
		m_BreakpointSet.insert(daemon_bp_key);
	}
	
	uLog(BREAKPOINT, " set %d / %d breakpoints", m_BreakpointSet.size(), m_AllSavedBreakpoints.size());
	
	#if LOG_BREAKPOINT_FILE_LUTS
	
		for (const auto bp : m_BreakpointSet)	uLog(BREAKPOINT, " bp(live) %S", bp);
		
	#endif

	UpdateFastBreakpoints();
}

//---- Fast Breakpoint Exists ? -----------------------------------------------
	
bool	LuaGlue::FastBreakpointExists(const char *source_name, const int &ln) const
{
	// expects the name ONLY (no @, /, or \ )
	const string	hash_key = string(source_name) + ":" + to_string(ln);
	
	const bool	f = (m_BreakpointSet.count(hash_key) > 0);
	
	return f;
}

//---- Update Fast Breakpoints ------------------------------------------------

void	LuaGlue::UpdateFastBreakpoints(void)
{
	// find highest breakpoint line#
	long	max_ln = -1;
	
	#if LOG_BREAKPOINT_FILE_LUTS
	
		for (const auto bp : m_BreakpointSet)		uLog(GLUE, " bp(fast) %S", bp);
		
	#endif
	
	for (const auto it : m_BreakpointSet)
	{
		const int	ln  = stoi(string(it, it.find(':') + 1));
		if (ln > max_ln)	max_ln = ln;
	}
	
	if (max_ln == -1)
		m_LineLUT.resize(1, 0);			// none found but need at least one entry to return a ptr
	else	m_LineLUT.resize(max_ln + 1, 0);
	
	for (const auto it : m_BreakpointSet)
	{
		const int	ln  = stoi(string(it, it.find(':') + 1));
		
		// insert in hashset
		m_LineLUT[ln] = true;
	}
	
	s_BreakpointTab = GetBreakpointLinesTab(s_BreakpointTabLen/*&*/);
}

//---- Get Breakpoint Lines Table ---------------------------------------------

	// (for fastest LUT)

const uint8_t*	LuaGlue::GetBreakpointLinesTab(size_t &len/*&*/) const
{	
	len = m_LineLUT.size();
	if (len == 0)	return nil;
	
	return &(m_LineLUT[0]);
}

// nada mas
