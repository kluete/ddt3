// DDT Lua common CLIENT message classes

#pragma once

#include <cstdint>
#include <vector>

namespace LX
{
	class DataOutputStream;
	class DataInputStream;
	
} // namespace LX

#include "ddt/sharedDefs.h"

namespace DDT
{
// import into namespace
using std::string;
using LX::DataInputStream;
using LX::DataOutputStream;

//---- NET SOURCE FILE --------------------------------------------------------

class NetSourceFile
{
public:
	// ctors
	NetSourceFile(const string &name, const string &ext, const int32_t &n_lines, const string &bin_data);
	NetSourceFile(DataInputStream &dis);
	
	string	ModuleName(void) const;		// (name)
	string	ShortFilename(void) const;	// (name.ext)
	size_t	DataSize(void) const;
	size_t	NumLines(void) const;
	
	string	m_Name;				// should be const?
	string	m_Ext;
	int32_t	m_NumLines;
	string	m_BinData;
};

DataOutputStream& operator<<(DataOutputStream &dos, const NetSourceFile &met_src_file);

//---- REQUEST STARTUP LUA ----------------------------------------------------

class RequestStartupLua
{
public:
	// ctors
	RequestStartupLua();
	RequestStartupLua(const string &source, const string &function, const bool &jit_f, const bool &load_break_f, const uint32_t &n_sources);
	RequestStartupLua(DataInputStream &dis);
	bool	IsOk(void) const;
	void	Clear(void);
	
	string		m_Source;
	string		m_Function;
	bool		m_JITFlag;
	bool		m_LoadBreakFlag;
	uint32_t	m_NumSourceFiles;
};

DataOutputStream& operator<<(DataOutputStream &dos, const RequestStartupLua &req_start_lua);

//---- locals/globals/watches VARIABLE REQUEST --------------------------------

class VariableRequest
{
public:	
	// ctors
	VariableRequest(const uint32_t &requester_id, const string &memory_var, const uint32_t &var_flags = VAR_VIEW_XOR_MASK);
	VariableRequest(const uint32_t &requester_id, const int32_t client_stack_level, const StringList &expanded_table_list, const uint32_t &var_flags);
	VariableRequest(DataInputStream &dis);
	
	void	clear(void);
	VariableRequest&	SetWatchNames(const StringList &watches);

	uint32_t	RequesterID(void) const;
	int32_t		GetStackLevel(void) const;
	
	uint32_t	HideMask(void) const;
	uint32_t	VarFlags(void) const;
	StringList	WatchNames(void) const;
	StringList	ExpandedTables(void) const;
	
private:

	uint32_t	m_RequesterID;
	int32_t		m_StackLevel;
	StringList	m_ExpandedTables;
	uint32_t	m_VarFlags;
	StringList	m_WatchNameList;
};

DataOutputStream& operator<<(DataOutputStream &dos, const VariableRequest &variable_request);

//---- NOTIFY BREAKPOINTS -----------------------------------------------------

class NotifyBreakpoints
{
public:
	// ctors
	NotifyBreakpoints(const StringList &bp_list);
	NotifyBreakpoints(DataInputStream &dis);
	
	const StringList&	GetBreakpoints(void) const	{return m_BreakpointList;}
	
private:

	StringList	m_BreakpointList;
};

DataOutputStream& operator<<(DataOutputStream &dos, const NotifyBreakpoints &notify_bp);

//---- RESUME VM --------------------------------------------------------------

class ResumeVM
{
public:
	ResumeVM(const CLIENT_CMD &client_cmd);
	ResumeVM(DataInputStream &dis);
	
	CLIENT_CMD	GetClientCmd(void) const		{return m_ClientCmd;}
	
private:

	CLIENT_CMD	m_ClientCmd;	
};

DataOutputStream& operator<<(DataOutputStream &dos, const ResumeVM &resume_vm);

} // namespace DDT

// nada mas
