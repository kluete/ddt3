// DDT Lua common daemon message classes

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace LX
{
	class DataOutputStream;
	class DataInputStream;
}

#include "ddt/sharedDefs.h"

namespace DDT
{
// import into namespace
using std::string;
using std::vector;
using LX::DataInputStream;
using LX::DataOutputStream;
	
//---- Platform Info ----------------------------------------------------------

class PlatformInfo
{
public:
	// ctors
	PlatformInfo();
	PlatformInfo(DataInputStream &dis);
	
	void	Defaults(void);
	
	string		OS(void) const			{return m_OS;}
	uint32_t	Architecture(void) const	{return m_Architecture;}
	string		Hostname(void) const		{return m_Hostname;}
	string		Lua(void) const			{return m_Lua;}
	uint32_t	LuaNum(void) const		{return m_LuaNum;}
	string		LuaJIT(void) const		{return m_LuaJIT;}
	uint32_t	LuaJITNum(void) const		{return m_LuaJITNum;}
	
	bool		HasJIT(void) const		{return (m_LuaJITNum > 0);}

	string		m_OS;
	uint32_t	m_Architecture;
	string		m_Hostname;
	string		m_Lua;
	uint32_t	m_LuaNum;
	string		m_LuaJIT;
	uint32_t	m_LuaJITNum;
};

DataOutputStream& operator<<(DataOutputStream &dos, const PlatformInfo &info);

//---- Suspend State ----------------------------------------------------------

class SuspendState
{
public:
	// ctors
	SuspendState();
	SuspendState(const string &fn, const int &ln, const string &err_s = "");
	SuspendState(DataInputStream &dis);
	
	bool	HasError(void) const;
	
	string	m_Source;
	int	m_Line;
	string	m_ErrorString;
};

DataOutputStream& operator<<(DataOutputStream &dos, const SuspendState &suspendState);

//---- Client Stack -----------------------------------------------------------

class ClientStack
{
	friend class DaemonStack;
public:
	// ctors
	ClientStack();
	ClientStack(DataInputStream &dis);
	
	class Level
	{
		friend class DaemonStack;
	public:
		Level()	{}
		// ctor & deserializer
		Level(DataInputStream &dis);
		
		DataOutputStream&	Serialize(DataOutputStream &dos) const;
		
		bool		IsC(void) const			{return m_CFlag;}
		string		SourceFileName(void) const	{return m_SourceFileName;}
		string		FunctionName(void) const	{return m_FunctionName;}
		string		AltFunctionName(void) const	{return m_AltFunctionName;}
		int32_t		Line(void) const		{return m_Line;}
		int32_t		NumFixedArguments(void) const	{return m_nFixedArguments;}
		int32_t		NumUpValues(void) const		{return m_nUpValues;}
		StringList	FunctionArguments(void) const	{return m_FunctionArguments;}
		
	private:
	
		bool		m_CFlag;
		string		m_SourceFileName;
		string		m_FunctionName;
		string		m_AltFunctionName;
		int32_t		m_Line;
		int32_t		m_nFixedArguments;
		int32_t		m_nUpValues;
		
		StringList	m_FunctionArguments;
	};

	void		Clear(void);
	const Level&	GetLevel(const size_t &ind) const;
	size_t		NumLevels(void) const;
	
private:
	
	vector<Level>	m_Levels;		
};

DataOutputStream& operator<<(DataOutputStream &dos, const ClientStack::Level &client_stack_level);
DataOutputStream& operator<<(DataOutputStream &dos, const ClientStack &clientStack);

} // namespace DDT

// nada mas
