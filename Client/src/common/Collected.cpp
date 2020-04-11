// Lua DDT collected var

// notes
// - only DE-serializers need to be shared
// - serializers can be daemon-only

#include <cassert>

#include "lx/stream/DataInputStream.h"

#include "ddt/Collected.h"
#include "lx/xstring.h"
#include "lx/xutils.h"

using namespace std;
using namespace LX;
using namespace DDT;

static const
unordered_map<COLLECTED_T, string, EnumClassHash>	s_CollectedNameLUT
{
	{COLLECTED_T::NONE,			"none"},			// for table fields ?
	{COLLECTED_T::NIL,			"nil"},	
	{COLLECTED_T::BOOLEAN, 			"boolean"},
	{COLLECTED_T::LIGHTUSERDATA,		"light userdata"},
	{COLLECTED_T::NUMBER,			"number"},
	{COLLECTED_T::STRING,			"string"},
	{COLLECTED_T::TABLE,			"table"},	
	{COLLECTED_T::LUA_FUNCTION,		"function"},
	{COLLECTED_T::USERDATA,			"userdata"},
	{COLLECTED_T::THREAD,			"thread"},
	
	{COLLECTED_T::C_FUNCTION,		"C function"},
	{COLLECTED_T::LUA_STRING_FUNCTION,	"function"},
	
	// luajit
	{COLLECTED_T::FFI_CDATA,		"ffi cdata"},
	
	{COLLECTED_T::UNSOLVED,			""},	
	{COLLECTED_T::NOT_IMPLEMENTED,		"<NOT_IMPLEMENTED>"}
};
	
static const
unordered_map<SCOPE_T, string, EnumClassHash>	s_ScopeNameLUT
{
	{SCOPE_T::NONE,			""},			// for table fields ?
	{SCOPE_T::FUNCTION_PARAMETER,	"param"},
	{SCOPE_T::VAR_ARG,		"vararg"},
	{SCOPE_T::LOCAL,		"local"},
	{SCOPE_T::UPVALUE,		"upvalue"},
	{SCOPE_T::ENVAL,		"enval"},
	{SCOPE_T::GLOBAL,		"global"},
	{SCOPE_T::REGISTRY,		"registry"}
};

static const
unordered_set<COLLECTED_T, EnumClassHash>	s_UnsolvedTypeSet
{
	COLLECTED_T::UNSOLVED,
	COLLECTED_T::NOT_IMPLEMENTED,
	COLLECTED_T::NONE
};

static const
unordered_set<COLLECTED_T, EnumClassHash>	s_DataTypeSet
{
	COLLECTED_T::STRING,
	COLLECTED_T::LUA_STRING_FUNCTION,
	COLLECTED_T::USERDATA,
	COLLECTED_T::FFI_CDATA,
};

//---- CTOR -------------------------------------------------------------------

	Collected::Collected()
{
	m_RootOffset = -1;
	m_HasMetaTableFlag = false;
	m_TemporaryFlag = false;
	m_Type = COLLECTED_T::ILLEGAL;		// (value type)
}

//---- Defaults ---------------------------------------------------------------

void	Collected::Defaults(void)
{
	m_RootOffset = -1;
	m_KeyS.clear();
	m_Type = COLLECTED_T::ILLEGAL;		// (value type)
	m_ValS.clear();
	m_Indent = 0; 
	m_Path.clear();
	m_ExpandedFlag = false;
	m_HasMetaTableFlag = false;
	m_Scope = SCOPE_T::NONE;
	m_LineStart = m_LineEnd = -1;
}

//---- CTOR from stream -------------------------------------------------------

	Collected::Collected(DataInputStream &dis)
{
	// Defaults();		// shouldn't be called every time
	
	// (to avoid aliasing warning)
	uint32_t	typ = 0;
	int32_t		scope = 0;
	
	// (unrolled to debug)
	dis >> m_RootOffset;
	dis >> m_KeyS;
	assert(m_KeyS.length() < DDT_MAX_LUA_STRING_LENGTH);
	dis >> typ;
	dis >> m_ExpandedFlag;
	dis >> m_HasMetaTableFlag;
	dis >> m_TemporaryFlag;
	dis >> m_ValS;
	assert(m_ValS.length() < DDT_MAX_LUA_STRING_LENGTH);
	dis >> m_Indent;
	dis >> m_Path;
	assert(m_Path.length() < DDT_MAX_LUA_STRING_LENGTH);
	dis >> scope;
	dis >> m_LineStart;
	dis >> m_LineEnd;
	
	m_Type = static_cast<COLLECTED_T>(typ);
	m_Scope = static_cast<SCOPE_T>(scope);
}

//---- Get Type String --------------------------------------------------------

// static
string	Collected::GetTypeS(const COLLECTED_T &t)
{
	assert(s_CollectedNameLUT.count(t) > 0);
	
	return s_CollectedNameLUT.at(t);
}

//---- Get Unsolved -----------------------------------------------------------

// static
Collected	Collected::GetUnsolved(const string &key_name)
{	
	Collected	ce;
	
	ce.Defaults();
	
	ce.m_RootOffset = 0;
	ce.m_KeyS = key_name;
	ce.m_Type = COLLECTED_T::UNSOLVED;
	ce.m_ValS.clear();
	ce.m_Path = key_name;			// to detect changed var
	
	return ce;
}

//---- Get Field Root Index (base for table) ----------------------------------

int	Collected::GetRootIndex(const int &index) const
{
	const int	root_index = index - m_RootOffset;
	
	return root_index;
}

SCOPE_T	Collected::GetScope(void) const
{
	return m_Scope;
}

string	Collected::GetKey(void) const
{
	return m_KeyS;
}

const string&	Collected::GetVal(void) const
{
	return m_ValS;
}

const string&	Collected::GetPath(void) const
{
	return m_Path;
}

COLLECTED_T	Collected::GetType(void) const
{
	return m_Type;
}

string	Collected::GetTypeString(void) const
{
	return GetTypeS(GetType());
}

bool	Collected::IsSolved(void) const
{
	return (s_UnsolvedTypeSet.count(m_Type) == 0);
}

string	Collected::GetScopeStr(void) const
{
	assert(s_ScopeNameLUT.count(m_Scope) > 0);
	
	return s_ScopeNameLUT.at(m_Scope);
}

size_t	Collected::GetIndent(void) const
{
	return m_Indent;
}

//---- Get Decorated Value String ---------------------------------------------

string	Collected::GetDecoratedVal(void) const
{
	if (IsMemoryURL())
	{
		return xsprintf("[%zu %s]", m_ValS.length(), IsBinaryData() ? "bytes" : "chars");
	}
	
	switch (m_Type)
	{	case COLLECTED_T::UNSOLVED:	return ""; break;
		case COLLECTED_T::STRING:	return string("\"") + m_ValS + "\""; break;
		
		case COLLECTED_T::LUA_FUNCTION:
		case COLLECTED_T::LUA_STRING_FUNCTION:
		
			return m_ValS + ":" + to_string(m_LineStart);
			break;
		
		case COLLECTED_T::C_FUNCTION:
		default:
		
			break;
	}
	
	return m_ValS;
}

//---- Get Decorated Key String -----------------------------------------------

string	Collected::GetDecoratedKey(void) const
{
	// would need KEY_TYPE to make right LUT
	/*
	if (IsCoarseFunction() && (m_KeyS.front() != '[' ) && (!IsTemporary()))
		return m_KeyS + "()";
	else	return m_KeyS;
	*/
	return m_KeyS;
}

//---- Is Table ? -------------------------------------------------------------

bool	Collected::IsTable(void) const
{
	return (m_Type == COLLECTED_T::TABLE);
}

bool	Collected::HasMetatable(void) const
{
	return m_HasMetaTableFlag;
}

bool	Collected::IsTemporary(void) const
{
	return m_TemporaryFlag;
}

bool	Collected::IsExpanded(void) const
{
	return m_ExpandedFlag;
}

bool	Collected::IsLuaFileFunction(void) const
{
	return (m_Type == COLLECTED_T::LUA_FUNCTION);
}

int	Collected::GetFunctionLineStart(void) const
{
	if ((m_Type != COLLECTED_T::LUA_FUNCTION) && (m_Type != COLLECTED_T::LUA_STRING_FUNCTION))	return -1;	// error
	
	return m_LineStart;
}

bool	Collected::GetLuaFileFunction(string &filename, int &ln) const
{
	filename.clear();
	ln = -1;
	
	if (!IsLuaFileFunction())	return false;
	
	filename = m_ValS;
	ln = m_LineStart;
	
	return true;
}

//---- Get Value string/memory/buffer Data Type -------------------------------

COLLECTED_DATA	Collected::GetValDataType(void) const
{
	if (!s_DataTypeSet.count(m_Type))	return COLLECTED_DATA::NONE;
	
	const size_t	len = m_ValS.length();
	if (!len)				return COLLECTED_DATA::VANILLA_ASCII;
	
	int	n_print = 0, n_unprint = 0, n_lines = 0;
	
	for (size_t i = 0; i < len; i++)
	{
		const char	c = m_ValS[i];
		if (isprint(c) || isspace(c))	n_print++;
		else				n_unprint++;
		
		n_lines += ((c == '\n') || (c == '\r')) ? 1 : 0;
	}
	
	if (n_unprint > 0)			return COLLECTED_DATA::BINARY;
	if (n_lines > 0)			return COLLECTED_DATA::MULTILINE;
	if (len > DDT_LUA_HUGE_STRING_LENGTH)	return COLLECTED_DATA::MULTILINE;		// not actually "multiline", but hey
	
	return COLLECTED_DATA::VANILLA_ASCII;
}

bool	Collected::IsBinaryData(void) const
{
	return (GetValDataType() == COLLECTED_DATA::BINARY);
}

bool	Collected::IsMemoryURL(void) const
{
	const auto	typ = GetValDataType();
	
	const bool	f = ((typ == COLLECTED_DATA::BINARY) || (typ == COLLECTED_DATA::MULTILINE));
	
	return f;
}

//----- CollectedList CTORs----------------------------------------------------

	CollectedList::CollectedList()
{
	clear();
}
	
	CollectedList::CollectedList(DataInputStream &dis)
{
	const size_t	sz = dis.Read32s();
	
	for (size_t i = 0; i < sz; i++)		push_back(Collected(dis));
}

// nada mas
