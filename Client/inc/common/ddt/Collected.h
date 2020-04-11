// ddt collected var

#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "ddt/sharedDefs.h"

enum class COLLECTED_T : uint32_t
{
	NONE = 0,
	NIL,
	BOOLEAN,
	LIGHTUSERDATA,
	NUMBER,
	STRING,
	TABLE,
	LUA_FUNCTION,
	USERDATA,
	THREAD,
	
	C_FUNCTION,
	LUA_STRING_FUNCTION,
	FFI_CDATA,
	
	UNSOLVED,			// (i.e. NIL)
	
	NOT_IMPLEMENTED,		// (should be error?)
	ILLEGAL
};

enum class COLLECTED_DATA : uint32_t
{
	NONE = 0,
	VANILLA_ASCII,
	BINARY,
	MULTILINE
};

namespace DDT
{
// import into namespace
using std::string;
using std::vector;
using LX::DataInputStream;
using LX::DataOutputStream;

class Collected
{
public:	
	// ctors
	Collected();
	Collected(DataInputStream &dis);
	// dtor
	~Collected()	{}
	
	// (setters only defined in daemon)
	void	Defaults(void);
	void	SetExpanded(const bool &f);
	void	SetHasMetatable(const bool &f);
	void	SetScope(const SCOPE_T &scope);
	void	SetTemporaryFlag(const bool &f);
	void	SetType(const COLLECTED_T &typ);
	void	SetPath(const string &path);
	void	SetIndent(const int &indent);
	void	SetLineRange(const int &start, const int &end);
	
	int		GetRootIndex(const int &index) const;
	bool		IsSolved(void) const;
	bool		IsTable(void) const;
	bool		IsExpanded(void) const;
	bool		IsLuaFileFunction(void) const;
	bool		IsBinaryData(void) const;
	bool		HasMetatable(void) const;
	SCOPE_T		GetScope(void) const;
	bool		IsTemporary(void) const;
	bool		IsMemoryURL(void) const;
	
	string		GetKey(void) const;
	const string&	GetVal(void) const;
	COLLECTED_T	GetType(void) const;
	string		GetTypeString(void) const;
	const string&	GetPath(void) const;
	size_t		GetIndent(void) const;
	
	int		GetFunctionLineStart(void) const;
	bool		GetLuaFileFunction(string &filename, int &ln) const;
	
	string		GetScopeStr(void) const;
	
	string		GetDecoratedKey(void) const;
	string		GetDecoratedVal(void) const;
	
	int32_t		m_RootOffset;
	string		m_KeyS;
	string		m_ValS;
	
	DataOutputStream&	Serialize(DataOutputStream &dos) const;
	
	static
	string	GetTypeS(const COLLECTED_T &t);
	
	static
	Collected	GetUnsolved(const string &key_name);
	
private:

	COLLECTED_DATA		GetValDataType(void) const;
	
	COLLECTED_T	m_Type;		// is VALUE type
	
	int16_t		m_Indent;
	SCOPE_T		m_Scope;
	int32_t		m_LineStart, m_LineEnd;
	
	bool		m_ExpandedFlag;
	bool		m_HasMetaTableFlag;
	bool		m_TemporaryFlag;
	
	string	m_Path;		// (for internal debugging)
};

class CollectedList : public std::vector<Collected>
{
public:
	// ctors
	CollectedList();
	CollectedList(DataInputStream &dis);
};

} // namespace DDT

// nada mas
