// Lua DDT common daemon message classes


#include <cassert>
#include <string>
#include <unordered_map>

#include "lx/stream/DataInputStream.h"
#include "lx/stream/DataOutputStream.h"

#include "ddt/CommonDaemon.h"
#include "ddt/sharedLog.h"

#include "lx/xutils.h"

using namespace std;
using namespace LX;
using namespace DDT;

#define SYMB(t)		{t,	#t}

static const
unordered_map<DAEMON_MSG, const string, EnumClassHash > s_DaemonMsgNameMap
{
	SYMB(DAEMON_MSG::ILLEGAL_DAEMON_MSG),
	SYMB(DAEMON_MSG::REPLY_PLATFORM),
	
	SYMB(DAEMON_MSG::NOTIFY_SCRIPT_LOADED),
	SYMB(DAEMON_MSG::NOTIFY_VM_SUSPENDED),
	SYMB(DAEMON_MSG::REPLY_STACK),
	SYMB(DAEMON_MSG::REPLY_LOCALS),
	SYMB(DAEMON_MSG::REPLY_GLOBALS),
	SYMB(DAEMON_MSG::REPLY_WATCHES),
	SYMB(DAEMON_MSG::REPLY_SOLVED_VARIABLE),
	
	SYMB(DAEMON_MSG::NOTIFY_USER_LOG_BATCH),
	SYMB(DAEMON_MSG::NOTIFY_USER_LOG_SINGLE),
	SYMB(DAEMON_MSG::NOTIFY_SESSION_ENDED),				// (not an error)
	
	SYMB(DAEMON_MSG::NOTIFY_SYNC_FAILED),
	
	SYMB(DAEMON_MSG::UNEXPECTED_CLIENT_MESSAGE_DURING_SYNC),
	SYMB(DAEMON_MSG::UNEXPECTED_CLIENT_MESSAGE_DURING_DEBUG),
	
	SYMB(DAEMON_MSG::REPLY_ECHO_DATA),
	SYMB(DAEMON_MSG::REPLY_ECHO_SIZE),
};

string	DDT::MessageName(const DAEMON_MSG &t)
{
	if (!s_DaemonMsgNameMap.count(t))
	{	uErr("ILLEGAL daemon message lookup 0x%08x", static_cast<int>(t));
		return "<ILLEGAL/MISSING>";
	}
	
	return s_DaemonMsgNameMap.at(t);
}

static const
unordered_map<CLIENT_CMD, const string, EnumClassHash > s_ClientCmdNameMap
{
	SYMB(CLIENT_CMD::ABORT),
	SYMB(CLIENT_CMD::ILLEGAL),
	SYMB(CLIENT_CMD::IDLE),
	SYMB(CLIENT_CMD::RUN),
	SYMB(CLIENT_CMD::STEP_INTO_LINE),
	SYMB(CLIENT_CMD::STEP_INTO_INSTRUCTION),
	SYMB(CLIENT_CMD::STEP_OVER),
	SYMB(CLIENT_CMD::STEP_OUT),
	
	SYMB(CLIENT_CMD::END_SESSION)			// not an error
};

string	DDT::CommandName(const CLIENT_CMD &t)
{
	if (!s_ClientCmdNameMap.count(t))
	{	uErr("ILLEGAL CLIENT_CMD message lookup 0x%08x", static_cast<int>(t));
		return "<ILLEGAL/MISSING>";
	}
	
	return s_ClientCmdNameMap.at(t);
}
#undef SYMB

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const DAEMON_MSG &msg_t)
{
	return dos << static_cast<uint32_t>(msg_t);
}
DataInputStream& DDT::operator>>(DataInputStream &dis, DAEMON_MSG &msg_t)
{
	const uint32_t	tmp = dis.Read32();
	
	msg_t = static_cast<DAEMON_MSG>(tmp);
	
	return dis;
}

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const timestamp_t &stamp)
{
	return dos << static_cast<uint64_t>(stamp.GetUSecs());
}

DataInputStream& DDT::operator>>(DataInputStream &dis, timestamp_t &stamp)
{
	const uint64_t	tmp = dis.Read64();
	
	stamp = timestamp_t::FromUS(static_cast<int64_t>(tmp));
	
	return dis;
}

/*
std::ostream& operator<<(std::ostream &os, const DAEMON_MSG &msg_t)
{
	const uint32_t	u32 = static_cast<uint32_t>(msg_t);
	
	os.put((u32 >> 24) & 0xff);
	os.put((u32 >> 16) & 0xff);
	os.put((u32 >>  8) & 0xff);
	os.put((u32 >>  0) & 0xff);
	
	return os;
}
*/

//==== Platform Info ==========================================================

	PlatformInfo::PlatformInfo()
{
	Defaults();
}

void	PlatformInfo::Defaults(void)
{
	m_OS = m_Hostname = m_Lua = m_LuaJIT = "";
	m_Architecture = m_LuaNum = m_LuaJITNum = 0;
}

	PlatformInfo::PlatformInfo(DataInputStream &dis)
		: m_OS{dis.ReadString()}, m_Architecture{dis.Read32()}, m_Hostname{dis.ReadString()},
		m_Lua{dis.ReadString()}, m_LuaNum{dis.Read32()},
		m_LuaJIT{dis.ReadString()}, m_LuaJITNum{dis.Read32()}
{
}

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const PlatformInfo &info)
{
	dos << info.OS() << info.Architecture() << info.m_Hostname;
	dos << info.Lua() << info.LuaNum();
	dos << info.LuaJIT() << info.LuaJITNum();
	return dos;
}

//==== Suspend State ==========================================================

	SuspendState::SuspendState()
		: m_Source{""}, m_Line{-1}, m_ErrorString{""}
{
}

	SuspendState::SuspendState(const string &fn, const int &ln, const string &err_s)
		: m_Source{fn}, m_Line{ln}, m_ErrorString{err_s}
{
}

	SuspendState::SuspendState(DataInputStream &dis)
		: m_Source{dis.ReadString()}, m_Line{dis.Read32s()}, m_ErrorString{dis.ReadString()}
{
}

bool	SuspendState::HasError(void) const
{
	return !m_ErrorString.empty();
}	

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const SuspendState &suspendState)
{
	dos << suspendState.m_Source << suspendState.m_Line << suspendState.m_ErrorString;
	
	return dos;
}

//=== Notify STACK to CLIENT ==================================================

//---- vanilla CTOR -----------------------------------------------------------

	ClientStack::ClientStack()
{
	Clear();
}

//---- CTOR & deserializer ----------------------------------------------------

	ClientStack::ClientStack(DataInputStream &dis)
{
	Clear();
	
	uint32_t	n_levels;
	
	dis >> n_levels;
	
	for (int i = 0; i < n_levels; i++)	{ m_Levels.push_back(Level(dis)); }
}

size_t	ClientStack::NumLevels(void) const
{
	return m_Levels.size();
}

//----- ClientStack Level (ctor & deserializer) -------------------------------

	ClientStack::Level::Level(DataInputStream &dis)
{
	dis >> m_CFlag >> m_SourceFileName >> m_FunctionName >> m_Line >> m_nFixedArguments >> m_nUpValues;
	
	m_FunctionArguments = StringList(dis);
	
	dis >> m_AltFunctionName;
}
DataOutputStream&	ClientStack::Level::Serialize(DataOutputStream &dos) const
{
	dos << m_CFlag << m_SourceFileName << m_FunctionName << m_Line << m_nFixedArguments << m_nUpValues << m_FunctionArguments << m_AltFunctionName;
	
	return dos;
}

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const ClientStack::Level &lvl)
{
	return lvl.Serialize(dos/*&*/);
}

void	ClientStack::Clear(void)
{
	m_Levels.clear();
}

const ClientStack::Level&	ClientStack::GetLevel(const size_t &ind) const
{
	assert(ind < m_Levels.size());
	
	return m_Levels[ind];
}

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const ClientStack &clientStack)
{
	const uint32_t	n_levels = clientStack.NumLevels();
	
	dos << n_levels;
	
	for (int i = 0; i < n_levels; i++)	{ dos << clientStack.GetLevel(i); }
	
	return dos;
}

// nada mas
