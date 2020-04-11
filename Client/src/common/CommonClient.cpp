// Lua DDT common CLIENT message classes

#include <cassert>
#include <algorithm>

#include "lx/stream/DataInputStream.h"
#include "lx/stream/DataOutputStream.h"

#include "ddt/CommonClient.h"
#include "ddt/sharedLog.h"

#include "lx/xutils.h"

using namespace std;
using namespace LX;
using namespace DDT;

#define SYMB(t)		{t,	#t}

static const
unordered_map<CLIENT_MSG, const string, EnumClassHash>	s_ClientMsgNameMap
{
		SYMB(CLIENT_MSG::ILLEGAL),
		
		SYMB(CLIENT_MSG::REQUEST_PLATFORM),
		
		SYMB(CLIENT_MSG::REQUEST_SYNC_N_START_LUA),
		
		SYMB(CLIENT_MSG::REQUEST_STACK),
		SYMB(CLIENT_MSG::REQUEST_LOCALS),
		SYMB(CLIENT_MSG::REQUEST_GLOBALS),
		SYMB(CLIENT_MSG::REQUEST_WATCHES),
		SYMB(CLIENT_MSG::REQUEST_SOLVE_VARIABLE),
		
		SYMB(CLIENT_MSG::NOTIFY_BREAKPOINTS),
		SYMB(CLIENT_MSG::REQUEST_VM_RESUME),
		
		SYMB(CLIENT_MSG::REQUEST_DAEMON_RELOOP),
		SYMB(CLIENT_MSG::REQUEST_DAEMON_LOG_OFF),
		
		// stress test
		SYMB(CLIENT_MSG::REQUEST_ECHO_DATA),
		SYMB(CLIENT_MSG::REQUEST_ECHO_SIZE)
};

#undef SYMB

string	DDT::MessageName(const CLIENT_MSG &t)
{
	if (!s_ClientMsgNameMap.count(t))
	{	uErr("ILLEGAL client message lookup 0x%08x", static_cast<int>(t));
		return "<ILLEGAL/MISSING>";
	}
	
	return s_ClientMsgNameMap.at(t);
}

//===== Client Message TYPE ===================================================

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const CLIENT_MSG &msg_t)
{
	return dos << static_cast<uint32_t>(msg_t);
}
DataInputStream& DDT::operator>>(DataInputStream &dis, CLIENT_MSG &msg_t)
{
	uint32_t	tmp = dis.Read32();
	
	msg_t = static_cast<CLIENT_MSG>(tmp);
	
	return dis;
}

//==== generic STRING LIST ====================================================

//---- StringList unserializing CTOR ------------------------------------------

	StringList::StringList(DataInputStream &dis)
{
	vector::clear();
	
	uint32_t	n = dis.Read32();
	
	for (int i = 0; i < n; i++)	vector::push_back(dis.ReadString());
}

//---- Initializer List CTOR --------------------------------------------------

	StringList::StringList(const initializer_list<string> &init_list)
		: vector<string>(init_list)
{
}

//---- vector CTOR ------------------------------------------------------------

	StringList::StringList(const vector<string> &string_vector)
		: vector(string_vector)
{
}

//---- To Flat String ---------------------------------------------------------

string	StringList::ToFlatString(const string &sep_s, const SEPARATOR_POS &pos) const
{
	if (size() == 0)			return string("");			// empty
	
	string	flat_s = (pos != SEPARATOR_POS::BACK) ? sep_s : string("");		// 1/3 trickery (WTF?)
	
	for (size_t i = 0; i < size(); i++)	flat_s += at(i) + sep_s;
	
	if (pos == SEPARATOR_POS::FRONT)	flat_s.pop_back();			// 1/3 trickery
	
	return flat_s;
}

//---- to StringSet -----------------------------------------------------------

StringSet	StringList::ToStringSet(void) const
{
	StringSet	res_set;
	
	for (size_t i = 0; i < size(); i++)		res_set.insert(at(i));
	
	return res_set;
}

//---- Sort -------------------------------------------------------------------

StringList&	StringList::Sort(void)
{
	if (size() > 0)
		sort(begin(), end(), [](const string &s1, const string &s2){ return s1 < s2; });
		
	return *this;
}

//---- Erase ------------------------------------------------------------------

bool	StringList::Erase(const string &v)
{
	if (empty())		return false;
	
	for (auto it = begin(); it != end(); it++)
	{
		if (v != *it)	continue;
		
		// found
		const size_t	sz_bf = size();
		erase(it);
		assert(size() == sz_bf - 1);
		
		return true;
	}
	
	return false;		// not found
}

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const StringList &string_list)
{
	dos << (uint32_t) string_list.size();
	
	for (const auto it : string_list)	dos << it;
	
	return dos;
}

//===== generic STRING SET ====================================================

//---- vanilla CTOR -----------------------------------------------------------

	StringSet::StringSet()
{
}

//---- vector<string> CTOR ----------------------------------------------------

	StringSet::StringSet(const vector<string> &string_vector)
{
	for (auto const v : string_vector)	insert(v);		// must have built-in ctor
}

//---- initializer_list CTOR --------------------------------------------------
	
	StringSet::StringSet(initializer_list<string> init_list)
		: unordered_set<string>(init_list)
{
}
	
//---- StringList to StringSet ------------------------------------------------

StringList	StringSet::ToStringList(void) const
{
	StringList	res_list;
	
	for (auto it = begin(); it != end(); it++)	res_list.push_back(*it);
	
	res_list.Sort();
	
	return res_list;
}

//---- To Flat String ---------------------------------------------------------

string	StringSet::ToFlatString(const std::string &sep_s) const
{
	StringList	res_list = ToStringList();
	
	return res_list.ToFlatString(sep_s);
}

//---- Insert StringList ------------------------------------------------------

void	StringSet::Insert(const StringList &string_list)
{
	for (const auto s : string_list)	insert(s);
}

//==== Net Source File ========================================================

	NetSourceFile::NetSourceFile(const string &name, const string &ext, const int32_t &n_lines, const string &bin_data)
		: m_Name{name}, m_Ext{ext}, m_NumLines(n_lines), m_BinData(bin_data)
{
}

	NetSourceFile::NetSourceFile(DataInputStream &dis)
		: m_Name{dis.ReadString()}, m_Ext{dis.ReadString()}, m_NumLines(dis.Read32()), m_BinData(dis.ReadString())
{
}

string	NetSourceFile::ModuleName(void) const
{
	return m_Name;
}

string	NetSourceFile::ShortFilename(void) const
{
	return m_Name + "." + m_Ext;
}

size_t	NetSourceFile::NumLines(void) const
{
	return m_NumLines;
}
	
size_t	NetSourceFile::DataSize(void) const
{
	return m_BinData.size();
}

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const NetSourceFile &nsf)
{
	dos << nsf.m_Name << nsf.m_Ext << nsf.m_NumLines << nsf.m_BinData;
	return dos;
}

//==== Request Startup Lua ====================================================

	RequestStartupLua::RequestStartupLua()
{
	Clear();
}
	RequestStartupLua::RequestStartupLua(const string &source, const string &function, const bool &jit_f, const bool &load_break_f, const uint32_t &n_sources)
		: m_Source(source), m_Function(function), m_JITFlag(jit_f), m_LoadBreakFlag(load_break_f), m_NumSourceFiles(n_sources)
{
}

	RequestStartupLua::RequestStartupLua(DataInputStream &dis)
		: m_Source(dis.ReadString()), m_Function(dis.ReadString()), m_JITFlag(dis.ReadBool()), m_LoadBreakFlag(dis.ReadBool()), m_NumSourceFiles{dis.Read32()}
{
}
void	RequestStartupLua::Clear(void)
{
	m_Source.clear();
	m_Function.clear();
	m_JITFlag = m_LoadBreakFlag = false;
	m_NumSourceFiles = 0;
}
bool	RequestStartupLua::IsOk(void) const
{	
	return !m_Source.empty() && !m_Function.empty() && (m_NumSourceFiles > 0);
}
DataOutputStream& DDT::operator<<(DataOutputStream &dos, const RequestStartupLua &rsl)
{
	dos << rsl.m_Source << rsl.m_Function << rsl.m_JITFlag << rsl.m_LoadBreakFlag << rsl.m_NumSourceFiles;
	return dos;
}

//===== locals/globals/watches VARIABLE REQUEST ===============================

//---- CTORs -------------------------------------------------------------------

	VariableRequest::VariableRequest(const uint32_t &requester_id, const int32_t stack_level, const StringList &expanded_tables, const uint32_t &var_flags)
		: m_RequesterID{requester_id}, m_StackLevel{stack_level}
{
	m_ExpandedTables.assign(expanded_tables.begin(), expanded_tables.end());
	m_VarFlags = var_flags;
	m_WatchNameList.clear();
}

//---- atomic memory var CTOR -------------------------------------------------

	VariableRequest::VariableRequest(const uint32_t &requester_id, const std::string &memory_var, const uint32_t &var_flags)
		: m_RequesterID{requester_id}, m_StackLevel{0/*NEED FIX?*/}, m_VarFlags{var_flags}, m_WatchNameList{memory_var}
{
		
}

//---- unserializing CTOR -----------------------------------------------------

	VariableRequest::VariableRequest(DataInputStream &dis)
		: m_RequesterID{dis.Read32()}, m_StackLevel{dis.Read32s()}, m_ExpandedTables{dis}, m_VarFlags{dis.Read32()}, m_WatchNameList{dis}		// order is critical
{
}

uint32_t	VariableRequest::RequesterID(void) const	{return m_RequesterID;}
int32_t		VariableRequest::GetStackLevel(void) const	{return m_StackLevel;}
uint32_t	VariableRequest::VarFlags(void) const		{return m_VarFlags;}
uint32_t	VariableRequest::HideMask(void) const		{return m_VarFlags ^ VAR_VIEW_XOR_MASK;}
StringList	VariableRequest::WatchNames(void) const		{return m_WatchNameList;}
StringList	VariableRequest::ExpandedTables(void) const	{return m_ExpandedTables;}

VariableRequest&	VariableRequest::SetWatchNames(const StringList &watch_names)
{
	m_WatchNameList.assign(watch_names.begin(), watch_names.end());
	
	return *this;
}

void	VariableRequest::clear(void)
{
	m_RequesterID = 0;
	m_StackLevel = 0;
	m_ExpandedTables.clear();
	m_VarFlags = VAR_VIEW_XOR_MASK;
	m_WatchNameList.clear();
}

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const VariableRequest &variable_request)
{
	dos << variable_request.RequesterID() << variable_request.GetStackLevel() << variable_request.ExpandedTables() << variable_request.VarFlags() << variable_request.WatchNames();
	return dos;
}

//==== Notify Breakpoints =====================================================

	NotifyBreakpoints::NotifyBreakpoints(const StringList &bp_list)
{
	m_BreakpointList.assign(bp_list.begin(), bp_list.end());
}
	NotifyBreakpoints::NotifyBreakpoints(DataInputStream &dis)
		: m_BreakpointList{ dis }
{
}
DataOutputStream& DDT::operator<<(DataOutputStream &dos, const NotifyBreakpoints &notify_bp)
{
	dos << notify_bp.GetBreakpoints();
	return dos;
}

//=== Resume VM ===============================================================

	ResumeVM::ResumeVM(const CLIENT_CMD &client_cmd)
		: m_ClientCmd(client_cmd)
{
}

	ResumeVM::ResumeVM(DataInputStream &dis)
		: m_ClientCmd(static_cast<CLIENT_CMD>(dis.Read32s()))
{
}

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const ResumeVM &resume_vm)
{
	dos << static_cast<int32_t>(resume_vm.GetClientCmd());
	return dos;
}

// nada mas
