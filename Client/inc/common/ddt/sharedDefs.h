// Lua DDT shared client/daemon header

#pragma once

#include <cassert>
#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <ostream>
#include <unordered_set>
#include <unordered_map>

#ifndef nil
	#define	nil	nullptr
#endif

namespace LX
{
	class DataInputStream;
	class DataOutputStream;
	class MemDataOutputStream;
	
	class timestamp_t;
	
} // namespace LX

namespace DDT
{
const int	DDT_MAX_TABLE_RECUSION = 32;
const int32_t	DDT_MAX_LUA_STRING_LENGTH = 0x100000;
const int	DDT_LUA_HUGE_STRING_LENGTH = 128;		// (for UI, not a hard limit)

// import into namespace
using std::string;
using std::vector;
using std::array;
using std::unordered_set;
using std::initializer_list;

using LX::DataInputStream;
using LX::DataOutputStream;
using LX::timestamp_t;

	// (note: brace elision warnings solved in C++14, not C++11)
	
	template<char _C0, char _C1, char _C2, char _C3>
	class MsgHeader
	{
		const array<uint8_t,8>	HDR_TAG {{_C0, _C1, _C2, _C3, 0, 0, 0, 0}};
	public:
		// ctors
		MsgHeader()
			: m_Header({{0, 0, 0, 0, 0, 0, 0, 0}})
		{
		}
		
		MsgHeader(const size_t &sz)
			: m_Header(HDR_TAG)
		{
			for (int i = 0; i < 4; ++i)	m_Header[i + 4] = (sz >> ((3 - i) * 8));
		}
		
		MsgHeader(const vector<uint8_t>::const_iterator &it)
		{
			for (int i = 0; i < 8; i++)	m_Header[i] = it[i];
		}
		
		void	Zap(void)
		{
			for (int i = 0; i < 8; ++i)	m_Header[i] = 0;
		}
		
		bool	IsOK(void) const
		{
			for (int i = 0; i < 4; ++i)
			{
				if (m_Header[i] != HDR_TAG[i])		return false;
			}
		
			return true;
		}
		
		size_t	GetBodySize(void) const
		{
			assert(IsOK());
			
			size_t	sz = 0;
			
			for (int i = 0; i < 4; ++i)	sz += ((size_t)m_Header[i + 4]) << ((3 - i) * 8);
			
			return sz;
		}
		
		const array<uint8_t, 8>&	Array(void) const
		{
			return m_Header;
		}
		
		array<uint8_t, 8>&	Array(void)
		{
			return m_Header;
		}
		
	private:
		
		array<uint8_t, 8>	m_Header;
	};
	
	typedef MsgHeader<'u', 'd', 'p', '4'>	UdpHeader;
	typedef MsgHeader<'t', 'c', 'p', '4'>	TcpHeader;
	
class StringSet: public unordered_set<string>
{
public:
	// ctors
	StringSet();
	StringSet(const vector<string> &string_vector);
	StringSet(initializer_list<string> init_list);
	
	class StringList	ToStringList(void) const;
	string			ToFlatString(const string &sep_s = " ") const;
	void			Insert(const class StringList &string_list);
};

class StringList: public vector<string>
{
public:
	// ctors
	StringList()	{}
	StringList(const initializer_list<string> &init_list);
	StringList(const vector<string> &string_vector);
	StringList(DataInputStream &dis);
	
	enum class SEPARATOR_POS
	{
		FRONT = 0,		// (not used as mask)
		BACK,
		FRONT_N_BACK
	};
	
	string		ToFlatString(const string &sep_s = " ", const SEPARATOR_POS &sep_pos = SEPARATOR_POS::BACK) const;
	string		ToFlatString(const string &sep_s, const bool &sep_f) const = delete;	// (deleted function - works SO SO)
	StringSet	ToStringSet(void) const;
	StringList&	Sort(void);
	bool		Erase(const string &v);
};

DataOutputStream& operator<<(DataOutputStream &dos, const StringList &string_list);

// (originates from client)
enum class CLIENT_MSG : uint32_t
{
	ILLEGAL = 0,
	
	REQUEST_PLATFORM,
	REQUEST_SYNC_N_START_LUA,
	REQUEST_VM_RESUME,
	
	REQUEST_STACK,
	REQUEST_LOCALS,
	REQUEST_GLOBALS,
	REQUEST_WATCHES,
	REQUEST_SOLVE_VARIABLE,
	
	NOTIFY_BREAKPOINTS,
	
	REQUEST_DAEMON_RELOOP,
	REQUEST_DAEMON_LOG_OFF,
	
	// stress test
	REQUEST_ECHO_DATA,
	REQUEST_ECHO_SIZE
};
string	MessageName(const CLIENT_MSG &t);
// must be INSIDE namespace or won't be found
DataOutputStream& operator<<(DataOutputStream &dos, const CLIENT_MSG &msg_t);
DataInputStream& operator>>(DataInputStream &dis, CLIENT_MSG &msg_t);

// (originates from daemon)
enum class DAEMON_MSG : uint32_t
{
	ILLEGAL_DAEMON_MSG = 0,
	
	NOTIFY_USER_LOG_SINGLE,
	
	REPLY_PLATFORM,
	
	NOTIFY_SCRIPT_LOADED,
	
	NOTIFY_VM_SUSPENDED,
	REPLY_STACK,
	REPLY_LOCALS,
	REPLY_GLOBALS,
	REPLY_WATCHES,
	REPLY_SOLVED_VARIABLE,
	
	NOTIFY_USER_LOG_BATCH,
	
	NOTIFY_SESSION_ENDED,		// not an error
	
	NOTIFY_SYNC_FAILED,
	UNEXPECTED_CLIENT_MESSAGE_DURING_SYNC,
	UNEXPECTED_CLIENT_MESSAGE_DURING_DEBUG,
	
	REPLY_ECHO_DATA,
	REPLY_ECHO_SIZE
};
string	MessageName(const DAEMON_MSG &t);
DataOutputStream& operator<<(DataOutputStream &dos, const DAEMON_MSG &msg_t);
DataInputStream& operator>>(DataInputStream &dis, DAEMON_MSG &msg_t);

enum VAR_VIEW_INDEX : uint32_t
{
	VV_FIRST = 0,
	
	VV_NUMBER = VV_FIRST,
	VV_BOOLEAN,
	VV_STRING,
	VV_LUA_FUNCTION,				// NOT SAME ORDER AS SIMILAR enums, but should be ok (where ???)
	VV_C_FUNCTION,
	VV_TABLE,
	VV_TEMPORARY,
	VV_THREAD,
	VV_USERDATA,
	VV_BUILT_IN,
	VV_METATABLE,
	VV_REGISTRY,
	VV_FFI_CDATA,
	
	VV_LAST_UI,
	
	VV_LAST_LOGIC = VV_LAST_UI,			// *** UPDATE IF CHANGING ABOVE ***
	
	VV_SEPARATOR,
	VV_STRETCH_SEPARATOR
};

enum VAR_VIEW_FLAG : uint32_t
{
	VAR_VIEW_NUMBER			= (1L << VV_NUMBER),
	VAR_VIEW_BOOLEAN		= (1L << VV_BOOLEAN),
	VAR_VIEW_STRING			= (1L << VV_STRING),
	VAR_VIEW_LUA_FUNCTION		= (1L << VV_LUA_FUNCTION),
	VAR_VIEW_C_FUNCTION		= (1L << VV_C_FUNCTION),
	VAR_VIEW_TABLE			= (1L << VV_TABLE),
	VAR_VIEW_TEMPORARY		= (1L << VV_TEMPORARY),
	VAR_VIEW_THREAD			= (1L << VV_THREAD),
	VAR_VIEW_USERDATA		= (1L << VV_USERDATA),
	VAR_VIEW_BUILT_IN		= (1L << VV_BUILT_IN),
	VAR_VIEW_METATABLE		= (1L << VV_METATABLE),
	VAR_VIEW_REGISTRY		= (1L << VV_REGISTRY),
	VAR_VIEW_FFI_CDATA		= (1L << VV_FFI_CDATA),
	
	VAR_VIEW_XOR_MASK		= ((1L << VV_LAST_LOGIC) - 1),
	
	VAR_VIEW_FUNCTION_ARGS		= (VAR_VIEW_NUMBER | VAR_VIEW_BOOLEAN | VAR_VIEW_STRING | VAR_VIEW_LUA_FUNCTION | VAR_VIEW_TABLE | VAR_VIEW_THREAD | VAR_VIEW_USERDATA),
	VAR_VIEW_FUNCTION_VAR_ARGS	= VAR_VIEW_FUNCTION_ARGS | VAR_VIEW_TEMPORARY
};

enum class SCOPE_T : uint32_t
{
	NONE = 0,			// for table member
	FUNCTION_PARAMETER,		// argument is local too
	VAR_ARG,			// vararg is local too
	LOCAL,
	UPVALUE,
	ENVAL,
	GLOBAL,
	REGISTRY
};

enum class CLIENT_CMD : int32_t
{	
	ABORT			= -1,
	ILLEGAL			= 0,		// reserve zero for error detection
	
	IDLE,
	
	RUN,
	STEP_INTO_LINE,
	STEP_INTO_INSTRUCTION,
	STEP_OVER,
	STEP_OUT,
	
	END_SESSION				// (not an error)
};

string	CommandName(const CLIENT_CMD &t);

DataOutputStream& operator<<(DataOutputStream &dos, const timestamp_t &stamp);
DataInputStream& operator>>(DataInputStream &dis, timestamp_t &stamp);

} // namespace DDT



// nada mas
