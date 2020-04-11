// Lua DDT variable collectors

#include <cassert>
#include <memory>
#include <string>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <type_traits>

#include "ddt/sharedDefs.h"

#include "lx/stream/DataOutputStream.h"

#include "lx/xstring.h"

#include "ddt/sharedLog.h"

#include "VarCollectors.h"

#include "LuaGlue.h"

#if DDT_JIT
	#include "luajit.h"
	#include "lj_debug.h"
	#include "lj_obj.h"
	#include "lj_ctype.h"
#endif

#ifndef LUA_OK
	#define LUA_OK	0
#endif

using namespace std;
using namespace LX;
using namespace DDT;

// redefine Lua types with STRONG enum for internal debugging (from lua.h)
enum class DDT::TLUA : int
{
	NONE		= LUA_TNONE,			// -1
	NIL		= LUA_TNIL,			// 0
	BOOLEAN		= LUA_TBOOLEAN,			// 1
	LIGHTUSERDATA	= LUA_TLIGHTUSERDATA,		// 2
	NUMBER		= LUA_TNUMBER,			// 3
	STRING		= LUA_TSTRING,			// 4
	TABLE		= LUA_TTABLE,			// 5
	FUNCTION	= LUA_TFUNCTION,		// 6
	USERDATA	= LUA_TUSERDATA,		// 7
	THREAD		= LUA_TTHREAD,			// 8
	
	#if DDT_JIT
		FFI_PROTO	= LUA_TPROTO,			// 9 (in lj_obj.h)
		FFI_CDATA	= LUA_TCDATA,			// 10
	#else
		FFI_PROTO	= LUA_TTHREAD + 1,
		FFI_CDATA	= LUA_TTHREAD + 2,
	#endif
};

static const
unordered_map<TLUA, COLLECTED_T, LX::EnumClassHash>		s_LuaToCollectedTypeMap
{
	{TLUA::NONE,			COLLECTED_T::NONE},
	{TLUA::NIL,			COLLECTED_T::NIL},
	{TLUA::BOOLEAN,			COLLECTED_T::BOOLEAN},
	{TLUA::LIGHTUSERDATA,		COLLECTED_T::LIGHTUSERDATA},
	{TLUA::NUMBER,			COLLECTED_T::NUMBER},
	{TLUA::STRING,			COLLECTED_T::STRING},
	{TLUA::TABLE,			COLLECTED_T::TABLE},
	{TLUA::FUNCTION,		COLLECTED_T::C_FUNCTION},	// C function until fully resolved
	{TLUA::USERDATA,		COLLECTED_T::USERDATA},
	{TLUA::THREAD,			COLLECTED_T::THREAD},

	{TLUA::FFI_CDATA,		COLLECTED_T::FFI_CDATA},
};

enum COLLECTED_MASK : uint32_t
{
	CMASK_COARSE_FUNCTION	= (1L << 0),
	CMASK_CONVERT_TO_PTR	= (1L << 1),		// out flag
	CMASK_FILTERED		= (1L << 2)		// out flag
};

// don't obsess on var filter, may have better solution
static const
unordered_map<COLLECTED_T, CollectProps, LX::EnumClassHash>	s_CollectedPropsMap
{
	{COLLECTED_T::NONE,		{0,				0}},	
	{COLLECTED_T::NIL,		{0,				0}},		
	{COLLECTED_T::BOOLEAN,		{VAR_VIEW_BOOLEAN,		0}},
	{COLLECTED_T::LIGHTUSERDATA,	{VAR_VIEW_USERDATA,		CMASK_CONVERT_TO_PTR}},			// don't collect?
	{COLLECTED_T::NUMBER,		{VAR_VIEW_NUMBER,		0}},
	{COLLECTED_T::STRING,		{VAR_VIEW_STRING,		0}},
	{COLLECTED_T::TABLE,		{VAR_VIEW_TABLE,		CMASK_CONVERT_TO_PTR}},
	{COLLECTED_T::C_FUNCTION,	{VAR_VIEW_C_FUNCTION,		CMASK_COARSE_FUNCTION | CMASK_CONVERT_TO_PTR}},
	{COLLECTED_T::LUA_FUNCTION,	{VAR_VIEW_LUA_FUNCTION,		CMASK_COARSE_FUNCTION}},
	{COLLECTED_T::USERDATA,		{VAR_VIEW_USERDATA,		CMASK_CONVERT_TO_PTR}},
	{COLLECTED_T::THREAD,		{VAR_VIEW_THREAD,		CMASK_CONVERT_TO_PTR}},
	
	{COLLECTED_T::FFI_CDATA,	{VAR_VIEW_FFI_CDATA,		CMASK_CONVERT_TO_PTR}}
};

static const
unordered_set<string>	s_BuiltInLuaStringSet
{
	// tables
	"bit32", "coroutine", "debug", "io", "math", "os", "package", "string", "table", "_G",

	// functions
	"assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "load", "loadfile", "next", "pairs",
	"pcall", "print", "rawequal", "rawget", "rawlen", "rawset", "require", "select", "setmetatable", "tonumber",
	"tostring", "type", "xpcall",
	
	// strings
	"_VERSION",
			
	// lua 5.1
	"module", "unpack", "loadstring",
	
	// DDT C functions
	"breakpoint"
};

inline
bool	IsReservedLua(const string &name)
{
	if (s_BuiltInLuaStringSet.count(name))					return true;
	if ((name.length() >= 2) && (name[0] == '_') && isupper(name[1]))	return true;
	
	return false;
}

inline
string	ConvNum(const double &d)
{	
	return xsprintf("%g", d);
}

inline
string	ConvPtr(const void *p)
{	
	// return xsprintf("%p", p);
	return xsprintf("0x%08x", (uint32_t)((const char *)p - (const char*)nil));	// lua can access only 32 bits (or 31)
}

void	Collected::SetExpanded(const bool &f)
{
	m_ExpandedFlag = f;
}

void	Collected::SetScope(const SCOPE_T &scope)
{
	m_Scope = scope;
}

void	Collected::SetHasMetatable(const bool &f)
{
	m_HasMetaTableFlag = f;
}

void	Collected::SetTemporaryFlag(const bool &f)
{	
	m_TemporaryFlag = f;
}

void	Collected::SetType(const COLLECTED_T &typ)
{
	m_Type = typ;
}

void	Collected::SetLineRange(const int &start, const int &end)
{
	m_LineStart = start;
	m_LineEnd = end;
}

void	Collected::SetPath(const string &path)
{
	m_Path = path;
}

void	Collected::SetIndent(const int &indent)
{
	m_Indent = indent;
}

LX::DataOutputStream&	Collected::Serialize(LX::DataOutputStream &dos) const
{
	assert(m_KeyS.length() < DDT_MAX_LUA_STRING_LENGTH);
	assert(m_ValS.length() < DDT_MAX_LUA_STRING_LENGTH);
	
	dos << m_RootOffset << m_KeyS << static_cast<uint32_t>(m_Type) << m_ExpandedFlag << m_HasMetaTableFlag << m_TemporaryFlag << m_ValS << m_Indent << m_Path << static_cast<underlying_type<SCOPE_T>::type>(m_Scope) << m_LineStart << m_LineEnd;
	
	return dos;
}

//---- Collected element serializer -------------------------------------------

DataOutputStream& DDT::operator<<(DataOutputStream &dos, const Collected &ce)
{
	return ce.Serialize(dos);
}

//---- Collected list Serializer ----------------------------------------------

LX::DataOutputStream& DDT::operator<<(LX::DataOutputStream &dos, const CollectedList &collList)
{
	dos << (int) collList.size();
	
	for (const auto it : collList)		dos << it;
	
	return dos;
}

inline
TLUA	LUA_TYPE(lua_State *L, const int &ind)
{
	TLUA	typ = static_cast<TLUA> (lua_type(L, ind));
	
	return typ;
}

//---- CTOR -------------------------------------------------------------------

	VarCollectors::VarCollectors()
{
	m_ExpandedTablesHashSet.clear();
}

//---- Convert Lua to Collected Type ------------------------------------------

uint32_t	VarCollectors::LuaToCollectedtype(const TLUA &ltype, COLLECTED_T &collec_t) const
{
	assert(s_LuaToCollectedTypeMap.count(ltype));
	collec_t = s_LuaToCollectedTypeMap.at(ltype);
	
	// look up collect_type properties
	const auto &it2 = s_CollectedPropsMap.find(collec_t);
	assert(s_CollectedPropsMap.end() != it2);
	
	const CollectProps	collec_props = it2->second;
	uint32_t	flags = collec_props.m_PropBits;
	if (flags & CMASK_COARSE_FUNCTION)		return flags;				// can't determine type yet
	
	if (collec_props.m_VarMask & m_HideMask)	return flags | CMASK_FILTERED;		// masked out	
		
	return flags;
}

//---- Copy Stack--------------------------------------------------------------

void	VarCollectors::CopyStack(const DaemonStack &stack, const StringList &expanded_tables, const uint32_t hide_mask)
{
	m_CollectedList.clear();
	m_RootItemIndex = 0;
	m_HideMask = hide_mask;
	
	// is deep copy?
	m_CallStack = stack;
	
	m_ExpandedTablesHashSet = expanded_tables.ToStringSet();
}

//---- Set Next Variable ------------------------------------------------------
	
bool	VarCollectors::CollectNextVariable(lua_State *L, const string &key_name, const string &parent_var, const int indent, const SCOPE_T scope)
{
	Collected	ce;
	
	ce.SetIndent(indent);
	ce.m_KeyS = key_name;
	
	const TLUA	val_type = LUA_TYPE(L, -1);
	// const char	*val_t = lua_typename(L, lua_type(L, -1));
	
	// check if is temporary variable (should be local var)
	// name starts with "(", could be "(*temporary)", loop vars, var args, or C function
	const bool	temp_f = (key_name.front() == '(') && (key_name.back() == ')') && (scope != SCOPE_T::VAR_ARG);
	if (temp_f && (VAR_VIEW_TEMPORARY & m_HideMask))		return false;		// don't show
	
	ce.SetTemporaryFlag(temp_f);
	
	COLLECTED_T		val_ctype;
	
	const uint32_t	res = LuaToCollectedtype(val_type, val_ctype/*&*/);
	if (res & CMASK_FILTERED)	return false;		// var view masked
	
	ce.SetType(val_ctype);
	ce.SetHasMetatable(false);
	
	string		val_str;
	bool		convert_to_ptr = false;
	
	switch (val_type)
	{	case TLUA::NONE:
		
			val_str = "none";
			break;
			
		case TLUA::NIL:
			
			// always show since is not a "type" proper
			val_str = "nil";
			break;
		
		case TLUA::BOOLEAN:
			
			val_str = (lua_toboolean(L, -1) != 0) ? "true" : "false";
			break;
			
		case TLUA::NUMBER:
			
			// val_str = to_string(lua_tonumber(L, -1));
			val_str = ConvNum(lua_tonumber(L, -1));
			break;
		
		case TLUA::STRING:
		{
			// extended so we get full "byte buffer" INCLUDING zero-byte pot-holes (not just ASCII)
			size_t	c_len = 0;
			const char	*c_ptr = lua_tolstring(L, -1, &c_len);
			if (c_ptr && c_len)
			{	val_str.assign(c_ptr, c_len);
				// computed sizes should be identical
				const size_t	sz = val_str.size();
				const size_t	len = val_str.length();
				assert((sz == c_len) && (len == c_len));
				assert(c_len < DDT_MAX_LUA_STRING_LENGTH);
			}
			else	val_str.clear();
		}	break;
		
		case TLUA::FUNCTION:
		{	
			// is a function, determine if is C, Lua or a main chunk
			// duplicate the function value on the stack, as will get popped by getinfo()
			// (otherwise will fuck up next iteration)
			lua_pushvalue(L, -1);
			
			lua_Debug	ar;
			
			bool	ok = lua_getinfo(L, ">Su", &ar);
			assert(ok);
			
			const string	what{ar.what};
			
			if (!(m_HideMask & VAR_VIEW_C_FUNCTION) && (what == "C"))
			{	ce.SetType(COLLECTED_T::C_FUNCTION);
				convert_to_ptr = true;
			}
			else if (!(m_HideMask & VAR_VIEW_LUA_FUNCTION) && ((what == "Lua")||(what == "main")))		// (combines both Lua function types)
			{	assert(ar.source && (ar.source[0] != 0));
				const bool	file_f = ('@' == ar.source[0]);
				if (file_f)
				{	string	fn_s{ar.source + (file_f ? 1 : 0)};
					val_str = fn_s;
					ce.SetType(COLLECTED_T::LUA_FUNCTION);
				}
				else
				{	val_str = ar.source;
					ce.SetType(COLLECTED_T::LUA_STRING_FUNCTION);
				}
				ce.SetLineRange(ar.linedefined, ar.lastlinedefined);
				assert(val_str.length() < DDT_MAX_LUA_STRING_LENGTH);
			}
			else							return false;		// unknown error ?
		}	break;
		
		case TLUA::TABLE:
			
			// if ((indent == 0) && (key_name == "_G"))		return false;		// ignore _G ?
			
			convert_to_ptr = true;
			break;
			
		case TLUA::THREAD:
		{	
			const int	top0 = lua_gettop(L);
			
			lua_State	*co = lua_tothread(L, -1);
			if (co == L)	val_str = "running";
			else switch (lua_status(co))
			{	case LUA_YIELD:
					val_str = "suspended";
					break;
					
				case LUA_OK:
				{	lua_Debug	ar;
					if (lua_getstack(co, 0, &ar) > 0)	val_str = "normal";		// has stack frames, is running
					else if (lua_gettop(co) == 0)		val_str = "dead";
					else					val_str = "suspended";		// initial state
				}	break;
				
				default:
					val_str = "dead";	// had some error?
					break;
			}
			assert(top0 == lua_gettop(L));
		}	break;
			
		case TLUA::USERDATA:
		{	
			ce.SetType(COLLECTED_T::USERDATA);
			// COPY data
			#if LUA_51
				const size_t	sz = lua_objlen(L, -1);
			#else
				const size_t	sz = lua_rawlen(L, -1);
			#endif
			const char	*p = (const char*)lua_touserdata(L, -1);
			if (p && (sz > 0))
			{	assert(sz < DDT_MAX_LUA_STRING_LENGTH);
				// ce.m_ValS.resize(sz, 0);
				// ::memcpy(&ce.m_ValS[0], p, sz);
				val_str.assign(p, sz);
				
			}
			else
			{	// conversion failed
				val_str.clear();
				convert_to_ptr = true;
			}
		}	break;
		
		case TLUA::LIGHTUSERDATA:
		
			ce.SetType(COLLECTED_T::LIGHTUSERDATA);
			convert_to_ptr = true;
			break;
			
		case TLUA::FFI_CDATA:
		
			ce.SetType(COLLECTED_T::FFI_CDATA);
			val_str.clear();
			convert_to_ptr = true;
			break;
		
		default:
			
			// dunno and others
			ce.SetType(COLLECTED_T::NOT_IMPLEMENTED);
			convert_to_ptr = true;
			break;
	}
	
	// if (!(m_HideMask & VAR_VIEW_METATABLE))				// detect in all cases
	{	// test metatable presence
		// do NOT test string,  has DEFAULT metatable!
		if (TLUA::STRING != val_type)
		{	const int	mt_res = lua_getmetatable(L, -1);
			if (mt_res != 0)	lua_pop(L, 1);
		
			ce.SetHasMetatable(mt_res != 0);
		}
	}
	
	if (convert_to_ptr)
	{	// duplicate value to convert since will turn to integer
		lua_pushvalue(L, -1);
		// turn table value into ptr
		const void	*p = lua_topointer(L, -1);
		// pop to restore stack
		lua_pop(L, 1);
		val_str = ConvPtr(p);
	}
	
	// empty values are OKAY
	// assert(!val_str.empty());
	
	string	var_path;
	
	// variable PATH - not yet universal locator
	if (!parent_var.empty())
	{	assert(key_name[0] != '[');
		assert(parent_var[0] != '[');
		var_path = parent_var + "[" + key_name + "]";			// DECORATION - should chain ancestor instead?
	}
	else	var_path = key_name;
	
	ce.SetPath(var_path);
	
	bool	recurse_f = false;
	
	if (COLLECTED_T::TABLE == ce.GetType())
	{	
		const bool	expanded_f = (m_ExpandedTablesHashSet.count(var_path) > 0);
		
		ce.SetExpanded(expanded_f);
		
		recurse_f = expanded_f;
		
		if (indent > DDT_MAX_TABLE_RECUSION)
		{	// val_str = "MAX_TABLE_RECUSION";			// detect in UI?
			recurse_f = false;
		}
	}
	
/*
	// metatable?
	const bool	metatable_f =	!(VAR_VIEW_METATABLE & m_HideMask) &&					// show metatables
					!(scope == SCOPE_FUNCTION_PARAMETER) &&					// not querying function parameters
					!(scope == SCOPE_VAR_ARG) &&						// ditto
					!((val_type == LUA_TSTRING) && (VAR_VIEW_BUILT_IN && m_HideMask)) &&
					(indent == 0);
*/	
	
	assert(val_str.length() < DDT_MAX_LUA_STRING_LENGTH);
	ce.m_ValS.assign(val_str);					// should be MOVE operator?
	
	ce.SetScope(scope);
	
	if (indent == 0)
	{	m_RootItemIndex = m_CollectedList.size();
	}
	
	ce.m_RootOffset = ((int)m_CollectedList.size()) - m_RootItemIndex;
	
	m_CollectedList.push_back(ce);
	
	#if 0
		// if show metatable
		if (!metatable_f)	return recurse_f;
		
		const int	mt_res = lua_getmetatable(L, -1);
		if (mt_res == 0)					return recurse_f;	// has no metatable
		
		// metatable may be SELF table!
		//   doesn't seem to care about sub-name nor indent???
		DumpLuaTableRecursive(L, "__mt", indent + 1);
		
		lua_pop(L, 1);
	#endif
	
	return recurse_f;
}

//---- Dump Lua Table Recursive -----------------------------------------------

void	VarCollectors::DumpLuaTableRecursive(lua_State *L, const string &var_path, int indent)
{
	assert(L);
	
	const int	table_stack_index = lua_gettop(L);
	
	// push first key
	lua_pushnil(L);
	
	while (lua_next(L, table_stack_index) != 0)
	{
		// key is at index -2, value at index -1
		const TLUA	key_type = LUA_TYPE(L, -2);
		string		key_str;
		
		if (key_type == TLUA::STRING)
			key_str = string("\"") + luaL_checkstring(L, -2) + "\"";
		else if (key_type == TLUA::NUMBER)
		{	key_str = ConvNum(luaL_checknumber(L, -2));
		}
		else if (key_type == TLUA::BOOLEAN)
		{	// will lose BOOL
			bool	f = (lua_toboolean(L, -2) != 0);
			key_str = f ? "true" : "false";			// can't be false, since is nil (just sayin')
		}
		else
		{	// duplicate key before convert cause will turn into a pumpkin
			lua_pushvalue(L, -2);
			// turn table value into ptr
			const void	*p = lua_topointer(L, -1);
			// pop to restore stack
			lua_pop(L, 1);
			
			key_str = ConvPtr(p);
		}
		
		if (CollectNextVariable(L, key_str, var_path, indent, SCOPE_T::NONE))
		{	const string	last_path = m_CollectedList.back().GetPath();
			const string	new_path = var_path + "[" + key_str + "]";
			
			assert(new_path == last_path);								// new path is REDUNDANT on recursion!!
		
			DumpLuaTableRecursive(L, new_path, indent + 1/*further down the rabbit hole*/);		// DECORATION
		}
		
		lua_pop(L, 1);		// remove value, keep key, iterate
	}

	/* While traversing tables don't call lua_tostring() on a key unless it's already a string because
	it changes the value at the given index and would otherwise fuck up lua_next()
	*/
}

//---- Get Lua Locals ---------------------------------------------------------

void	VarCollectors::CollectLuaLocals(lua_State *L, const int &stack_depth, const uint32_t &hide_mask, DataOutputStream &dos)
{
	assert(L);
	
	m_HideMask = hide_mask;
	
	LuaStackDancer	ctop(L);
	
	ddt_Debug	ar;
	
	bool	ok = ddt_getstack(L, stack_depth, &ar);
	assert(ok);
	
	m_VisitedUpvalues.clear();
	
	// get #params and #upvalues
	ok = ddt_getinfo(L, "u", &ar);
	assert(ok);

	const int	n_function_args = ar.nparams;		// indices below this index are function args
	const bool	var_arg_f = ar.isvararg;
	
	// when a local var's name appears multiple times, keep only last instance
	// negative indices are varargs
	unordered_map<string, int>	locals_map;
	
	int	temp_cnt = 0, non_temp_cnt = 0;
	
	ctop.LoopPoint();
	
	for (int i = 1; true; i++)
	{
		ctop.Bump();
		
		const char *key_p = ddt_getlocal(L, &ar, i);
		if (!key_p)		break;			// done with locals
		
		string	key_name(key_p);
		
		const bool	temp_f = (key_name[0] == '(');
		if (temp_f)
		{	// ignore nil temporaries
			if (lua_isnil(L, -1))		continue;
			
			// rename (non-nil) temporaries with counter
			key_name.assign(string("(temp") + to_string(temp_cnt++) + ")");
		}
		else	non_temp_cnt++;
		
		locals_map.insert({key_name, i});
	}
	
	if (var_arg_f)
	{	// fetch var args
		LuaStackDancer	ctop2(L);
		
		for (int i = -1; true; i--)
		{
			ctop2.Bump();
			
			const char *key_p = ddt_getlocal(L, &ar, i);
			if (!key_p)		break;			// done with var args
		
			string	key_name(key_p);
			assert(key_name == "(*vararg)");
			
			// rename var arg
			key_name.assign(string("(vararg") + to_string(-i) + ")");
		
			locals_map.insert({key_name, i});
		}
	}
	
	for (auto &it : locals_map)
	{
		ctop.Bump();
		
		// take STORED name, with renamed temporaries
		const string	key_name{it.first};
		const int	i{it.second};
		const char *dummy_key_p = ddt_getlocal(L, &ar, i);
		assert(dummy_key_p);
		
		const SCOPE_T	scope = (i < 0) ? SCOPE_T::VAR_ARG : ((i <= n_function_args) ? SCOPE_T::FUNCTION_PARAMETER : SCOPE_T::LOCAL);
		
		if (CollectNextVariable(L, key_name, "", 0/*indent*/, scope))
			DumpLuaTableRecursive(L, key_name, 1/*indent*/);
	}
	
	ctop.Bump0();
	
	// get upvalues
	CollectUpvalues(L, stack_depth, hide_mask);
	
	dos << m_CollectedList;
	
	uLog(COLLECT, "CollectLuaLocals(temp = %d, non-temp %d)", temp_cnt, non_temp_cnt);
}

#if LUA_51
	
static
int	lua_absindex(lua_State *L, const int idx)
{
	return ((idx > 0) || (idx <= LUA_REGISTRYINDEX)) ? idx : (lua_gettop(L) + idx + 1);
}

static
int	luax_getglobalstab(lua_State *L)
{
	lua_pushvalue(L, LUA_GLOBALSINDEX);
	assert(lua_istable(L, -1));
	
	return lua_absindex(L, -1);
}

#else

static
int	luax_getglobalstab(lua_State *L)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
	assert(lua_istable(L, -1));
	
	return lua_absindex(L, -1);
}	

#endif

//---- Get Upvalues ...and envars as long as not vanilla globals --------------

void	VarCollectors::CollectUpvalues(lua_State *L, const int &stack_depth, const uint32_t &hide_mask)
{
	uLog(COLLECT, "VarCollectors::CollectLuaUpvalues(depth %d)", stack_depth);
	
	assert(L);
	
	LuaStackDancer	ctop(L);
	
	m_HideMask = hide_mask;
	m_VisitedUpvalues.clear();
	
	// topmost globals
	const int	globals_table_index = luax_getglobalstab(L);
	
	lua_Debug	ar;
		
	bool	ok = lua_getstack(L, stack_depth, &ar);
	// if (!ok)								return;			// HAS NO STACK AT THIS LEVEL
	assert(ok);
	
	// push function at this stack level
	ok = lua_getinfo(L, "f", &ar);
	assert(ok && lua_isfunction(L, -1));
	
	const int	func_index = lua_absindex(L, -1);
	int	env_cnt = 0, nup_cnt = 0;
	
	ctop.LoopPoint();
	
	for (int i = 1; true; i++)
	{	
		ctop.Bump();
		
		// returns key and pushes value to stack
		const char *key_p = lua_getupvalue(L, func_index, i);
		if (!key_p)								break;		// done
		
		#if !LUA_51
			const void	*upval_id = lua_upvalueid(L, func_index, i/*upvalue*/);
			if (upval_id && m_VisitedUpvalues.count(upval_id))		continue;	// don't revisit same env, except real/topmost globals
			m_VisitedUpvalues.insert(upval_id);
		#endif

		string	var_name(key_p);
		
		const TLUA	val_type = LUA_TYPE(L, -1);
		const bool	env_f = (TLUA::TABLE == val_type) && (var_name == "_ENV");		// is closure's _ENV?
		if (env_f)
		{	// _ENV -- but does it point to "vanilla" globals?
			env_cnt++;
			const bool	vanilla_globals_f = (lua_rawequal(L, globals_table_index, -1) == 1);
			if (vanilla_globals_f)						continue;	// yes, ignore for locals view
		
			CollectENVGlobals(L, SCOPE_T::ENVAL);
		}
		else
		{	// upvalue
			nup_cnt++;
			
			if (CollectNextVariable(L, var_name, "", 0/*indent*/, SCOPE_T::UPVALUE))
				DumpLuaTableRecursive(L, var_name/*path*/, 1/*indent*/);
		}
	}
	
	uLog(COLLECT, "CollectLuaUpvalues(_ENV = %d, upvalues = %d)", env_cnt, nup_cnt);
}

//---- Collect _ENV globals/envals --------------------------------------------

void	VarCollectors::CollectENVGlobals(lua_State *L, const SCOPE_T &scope)
{
	uLog(COLLECT, "VarCollectors::CollectENVGlobals()");
	
	assert(L);
	assert(lua_istable(L, -1));
	
	LuaStackDancer	ctop(L);
	
	const int	envals_table_index = lua_absindex(L, -1);
	
	// push first key
	lua_pushnil(L);
	
	ctop.LoopPoint();
	
	while (ctop.Bump() && lua_next(L, envals_table_index))
	{
		// key is at index -2, value at index -1
		const TLUA	key_type = LUA_TYPE(L, -2);
		if (key_type != TLUA::STRING)								continue;	// not a string-keyed entry, not interesting (?)
		
		const string	key_str = luaL_checkstring(L, -2);
			
		if ((m_HideMask & VAR_VIEW_BUILT_IN) && IsReservedLua(key_str))		continue;
		
		if (CollectNextVariable(L, key_str, "", 0/*indent*/, scope))
			DumpLuaTableRecursive(L, key_str, 1/*indent*/);
	}
}

//---- Get Lua Globals --------------------------------------------------------

void	VarCollectors::CollectLuaGlobals(lua_State *L, const int &start_depth, const uint32_t &hide_mask, DataOutputStream &dos)
{
	uLog(COLLECT, "VarCollectors::CollectLuaGlobals() stack[%d; %d[", start_depth, m_CallStack.m_Levels.size());
	
	assert(L);
	
	LuaStackDancer	ctop(L);
	
	m_HideMask = hide_mask;
	m_VisitedUpvalues.clear();
	
	// "global global" level
	const int	globals_table_index = luax_getglobalstab(L);
	
	#if LUA_51
	
		CollectENVGlobals(L, SCOPE_T::GLOBAL);
		
		dos << m_CollectedList;
	
		return;
	#endif
	
	
	// find _ENV at each stack level, scan it linearly, then bubble up
	// integrate var occlusion
	// - could just pre-fill hashset<string> with masked vars, then add new globals?
	
	int	n_good_env = 0, n_bad_end = 0;
	
	ctop.LoopPoint();
	
	for (int sp = start_depth; sp < m_CallStack.m_Levels.size(); sp++)
	{	
		ctop.Bump();
		
		lua_Debug	ar;
	
		bool	ok = lua_getstack(L, sp, &ar);
		// if (!ok)							continue;		// HAS NO STACK AT THIS LEVEL
		assert(ok);
	
		// push function at this stack level
		ok = lua_getinfo(L, "f", &ar);
		assert(ok && lua_isfunction(L, -1));
		
		const int	func_index = lua_absindex(L, -1);
		
		const char *key_p = lua_getupvalue(L, func_index, 1/*upvalue index*/);			// _ENV is always 1st upvalue?
		if (!key_p)
		{	n_bad_end++;
			continue;
		}
		// assert(key_p);									// generally means is at wrong level?
		
		const string	up_key(key_p);
		const TLUA	up_val_type = LUA_TYPE(L, -1);
		if ((up_key != "_ENV") || (up_val_type != TLUA::TABLE))		continue;		// not _ENV
		
		n_good_env++;
		
		#if LUA_51
			const void	*upval_id = nil;
			(void)upval_id;
		#else
			// only Lua 5.2
			const void	*upval_id = lua_upvalueid(L, func_index, 1/*upvalue*/);		// when NIL is "global global" (topmost) _ENV
			if (upval_id && m_VisitedUpvalues.count(upval_id))	continue;		// don't revisit same _ENV unless is "global global" ?
			m_VisitedUpvalues.insert(upval_id);
		#endif
		
	// found this stack level's _ENV, traverse its "envals"
	
	// determine scope before loop
		
		// is at "real global" _ENV ?
		const bool	globals_f = /*(upval_id == nil) ||*/ (lua_rawequal(L, globals_table_index, -1) == 1);
		const SCOPE_T	scope = globals_f ? SCOPE_T::GLOBAL : SCOPE_T::ENVAL;
		
		CollectENVGlobals(L, scope);
	}
	
	uLog(COLLECT, "  _ENV cnt: good %d, bad %d", n_good_env, n_bad_end);
	
	if (!(m_HideMask & VAR_VIEW_REGISTRY))
	{	// do NOT reset collected here
		CollectLuaRegistry(L);
	}
	
	dos << m_CollectedList;
}

//---- Collect Registry -------------------------------------------------------
	
void	VarCollectors::CollectLuaRegistry(lua_State *L)
{	
	assert(L);
	
	uLog(COLLECT, "VarCollectors::CollectLuaRegistry()");
	
	LuaStackDancer	ctop(L);
	
	// push first key
	lua_pushnil(L);
	
	ctop.LoopPoint();
	
	while (ctop.Bump() && lua_next(L, LUA_REGISTRYINDEX))
	{
		// key is at index -2, value at index -1
		const TLUA	key_type = LUA_TYPE(L, -2);
		if (key_type == TLUA::NUMBER)		continue;			// ignore lua references
		if (key_type != TLUA::STRING)		continue;			// ignore non-strings (redundant test, granted)
		
		const string	key_str = luaL_checkstring(L, -2);
		if ((m_HideMask & VAR_VIEW_BUILT_IN) && IsReservedLua(key_str))		continue;	// builtin
		
		if (CollectNextVariable(L, key_str, "", 0/*indent*/, SCOPE_T::REGISTRY))
			DumpLuaTableRecursive(L, key_str, 1/*indent*/);
	}
}

//---- Push Unsolved ----------------------------------------------------------

void	VarCollectors::PushUnsolved(const string &key_name)
{
	const Collected	ce(Collected::GetUnsolved(key_name));
	
	m_CollectedList.push_back(ce);
}

//---- Solve One Variable (private) -------------------------------------------

bool	VarCollectors::SolveOneVar(lua_State *L, const string &var_name)
{
	assert(L);
	assert(!var_name.empty());
	
	// const int	top0 = lua_gettop(L);
	
	int	stack_level = m_CallStack.FirstLuaLevel();
	
	ddt_Debug	ar;
	
	bool	ok = ddt_getstack(L, stack_level, &ar);
	if (!ok)					return false;	// HAS NO STACK AT THIS LEVEL
	
	// get #params and #upvalues
	ok = ddt_getinfo(L, "u", &ar);
	assert(ok);
	
	const int	n_function_args = ar.nparams;		// indices below this index are function args
	// const bool	var_arg_f = ar.isvararg;
	
	int	index = -1;
	
// scan locals	
	for (int i = 1; true; i++)
	{
		const char *key_p = ddt_getlocal(L, &ar, i);
		if (!key_p)				break;		// hit nil var
		if (var_name.compare(key_p) == 0)	index = i;	// save index of last occurrance
		
		lua_pop(L, 1);
		
		// CONTINUE LOOPING to get last instance of that var (if has more than one instance)
	}
	
	if (index > 0)
	{	// if found
		const char *key_p = ddt_getlocal(L, &ar, index);
		assert(key_p && (var_name.compare(key_p) == 0));
		
		// will never be vararg here since is solving a var NAME and varargs have none
		const SCOPE_T	scope = (index <= n_function_args) ? SCOPE_T::FUNCTION_PARAMETER : SCOPE_T::LOCAL;
		
		if (CollectNextVariable(L, var_name, "", 0/*indent*/, scope))
			DumpLuaTableRecursive(L, var_name/*path*/, 1/*indent*/);
		
		lua_pop(L, 1);
		return true;
	}

// scan upvalues
	
	// push function at this stack level
	ok = ddt_getinfo(L, "f", &ar);
	assert(ok);
		
	for (int i = 1; true; i++)
	{	// returns key and pushes value to stack
		const char *key_p = lua_getupvalue(L, -1/*funcindex*/, i);
		if (!key_p)				break;			// hit nil var
		if (var_name.compare(key_p) != 0)
		{	lua_pop(L, 1);
			continue;			// not var we want (equality returns zero)
		}
		
		if (CollectNextVariable(L, var_name, "", 0/*indent*/, SCOPE_T::UPVALUE))
			DumpLuaTableRecursive(L, var_name/*path*/, 1/*indent*/);
		
		// pop variable AND function
		lua_pop(L, 2);
		return true;				// FOUND!
	}
		
	// pop lua function
	lua_pop(L, 1);

// scan functions' _ENV, trickling up stack

	#if LUA_51
	
	{	// "global global" level
		const int	globals_table_index = luax_getglobalstab(L);
		(void)globals_table_index;
	
	#else // !LUA_51
	
	const int			max_stack_level = m_CallStack.m_Levels.size();
	unordered_set<const void*>	visited_envs;
	
	LuaStackDancer	ctop(L);
	
	for (; stack_level < max_stack_level; stack_level++)
	{	
		ctop.Bump();
		
		ok = lua_getstack(L, stack_level, &ar);
		assert(ok);
		
		// push function at this stack level
		ok = lua_getinfo(L, "f", &ar);
		assert(ok);
		
		const int	func_index = lua_absindex(L, -1);
		
		const char *key_p = lua_getupvalue(L, -1/*funcindex*/, 1/*upvalue index*/);		// could THEORETICALLY keep on searching for _ENV at this stack level?
		if (!key_p)						continue;	// empty upvalue?
		
		const string	key(key_p);
		const TLUA	val_type = LUA_TYPE(L, -1);
		
		if ((key != "_ENV") || (val_type != TLUA::TABLE))	continue;	// not _ENV
		
		const void	*upval_id = lua_upvalueid(L, func_index, 1/*upvalue index*/);
		if (visited_envs.count(upval_id))			continue;	// visited this _ENV before
		visited_envs.insert(upval_id);
		
		// have _ENV on stack
		
	#endif // !LUA_51
		
		// get _ENV[var_name]
		lua_pushstring(L, var_name.c_str());
		lua_rawget(L, -2/*table index*/);
		if (lua_isnil(L, -1))
		{	
			// not found in _ENV
			return false;
		}
		
	// found in _ENV
		if (CollectNextVariable(L, var_name, "", 0/*indent*/, SCOPE_T::GLOBAL))		// is actually an "_ENV" (among several), not really a "global"
			DumpLuaTableRecursive(L, var_name/*path*/, 1/*indent*/);
			
		return true;
	}
	
	return false;
}

//---- Collect Watch/Freeform Variables ---------------------------------------

void	VarCollectors::CollectLuaWatches(lua_State *L, const StringList &watch_list, DataOutputStream &dos)
{
	assert(L);
	
	LuaStackDancer	ctop(L);
	
	// loop through all watches
	for (int i = 0; i < watch_list.size(); i++)
	{	
		const string	key_name = watch_list[i];
		
		bool	f = SolveOneVar(L, key_name);
		if (!f)		PushUnsolved(key_name);		// add placeholder
	}
	
	dos << m_CollectedList;
}

//---- Collect NOP Variables --------------------------------------------------

void	VarCollectors::CollectNOPWatches(lua_State *L, const StringList &watch_list, DataOutputStream &dos)
{
	assert(L);
	
	LuaStackDancer	ctop(L);
	
	for (int i = 0; i < watch_list.size(); i++)
	{	
		const string	key_name = watch_list[i];
		
		PushUnsolved(key_name);		// add placeholder
	}
	
	dos << m_CollectedList;
}

//---- Get Function Arguments at Stack Level ----------------------------------

StringList	VarCollectors::GetFunctionArgs(lua_State *L, const int &n_fixed_args, const int &stack_level)
{
	assert(L);
	
	LuaStackDancer	ctop(L);
	
	m_CollectedList.clear();
	m_ExpandedTablesHashSet.clear();
	
	StringList	arg_list;

	lua_Debug	ar;

	bool	ok = lua_getstack(L, stack_level, &ar);
	if (!ok)		return arg_list;			// HAS NO STACK AT THIS LEVEL
	
	m_HideMask = VAR_VIEW_FUNCTION_ARGS ^ VAR_VIEW_XOR_MASK;
	
	ctop.LoopPoint();

	for (int i = 1; i <= n_fixed_args; i++)
	{
		ctop.Bump();
		
		const char *key_p = lua_getlocal(L, &ar, i);
		if (!key_p)	break;
		
		m_CollectedList.clear();
		CollectNextVariable(L, string(key_p), "", 0, SCOPE_T::FUNCTION_PARAMETER);			// (never recurse)
		assert(m_CollectedList.size() == 1);
		
		const string	val = m_CollectedList.back().GetDecoratedVal();
		
		arg_list.push_back(val);
	}
	
	// varargs (negative indices) are temporaries... implicitly?
	m_HideMask = VAR_VIEW_FUNCTION_VAR_ARGS ^ VAR_VIEW_XOR_MASK;
	
	ctop.Bump0();
	ctop.LoopPoint();
	
	for (int i = -1; true; i--)
	{
		ctop.Bump();
		
		const char *key_p = lua_getlocal(L, &ar, i);
		if (!key_p)	break;
		
		const string	key_s(key_p);
		assert(key_s == "(*vararg)");
		
		m_CollectedList.clear();
		CollectNextVariable(L, key_s, "", 0, SCOPE_T::VAR_ARG);		// (never recurse)
		assert(m_CollectedList.size() == 1);				// hardcoded
		
		const string	val = m_CollectedList.back().GetDecoratedVal();
		
		arg_list.push_back(val);
		
		// lua_pop(L, 1);
	}
	
	m_CollectedList.clear();
	
	return arg_list;
}

//---- Map Globals Functions --------------------------------------------------

void	VarCollectors::MapGlobalFunctions(lua_State *L)
{
	uLog(COLLECT, "VarCollectors::MapGlobalFunctions()");
	
	assert(L);
	
	m_LuaFunctionLUT.clear();
	
	LuaStackDancer	ctop(L);
	
	const int	globals_table_index = luax_getglobalstab(L);
	
	// push first key
	lua_pushnil(L);
	
	ctop.LoopPoint();
	
	while (ctop.Bump() && lua_next(L, globals_table_index) != 0)
	{
		// key is at index -2, value at index -1
		const TLUA	key_type = LUA_TYPE(L, -2);
		if (key_type != TLUA::STRING)			continue;
		
		const string	key_str = luaL_checkstring(L, -2);
		assert(!key_str.empty());
		
		const TLUA	val_type = LUA_TYPE(L, -1);
		if (val_type != TLUA::FUNCTION)			continue;
		if (IsReservedLua(key_str))			continue;
		
		// safely turn Lua function to a pointer
		lua_pushvalue(L, -1);
		const void	*p = lua_topointer(L, -1);
		assert(p);
		// lua_pop(L, 1);	// (automatic)
		
		m_LuaFunctionLUT.insert({p, key_str});
	}
	
	uLog(COLLECT, "  mapped %zu functions", m_LuaFunctionLUT.size());
}

//---- Look-Up Function name from opaque pointer ------------------------------

	// for alternative stack function name

string	VarCollectors::LookupFunctionPtr(lua_State *L) const
{
	assert(L);
	assert(lua_isfunction(L, -1));
	
	if (m_LuaFunctionLUT.empty())	return "";
	
	uLog(COLLECT, "VarCollectors::LookupFunctionPtr()");
	
	LuaStackDancer	ctop(L);
	
	// safely turn Lua function to a pointer
	lua_pushvalue(L, -1);
	const void	*p = lua_topointer(L, -1);
	assert(p);
	lua_pop(L, 1);
	
	if (!m_LuaFunctionLUT.count(p))
	{	uLog(COLLECT, "  not found");
		return "";
	}
	
	const string	func_name = m_LuaFunctionLUT.at(p);
	uLog(COLLECT, "  found %S", func_name);
	assert(!func_name.empty());
	
	// re-verify by comparing actual Lua functions
	ddt_getglobal(L, func_name.c_str());
	bool	ok = lua_isfunction(L, -1);
	assert(ok);
	
	// compare both
	ok = (lua_rawequal(L, -1, -2) == 1);
	// lua_pop(L, 1);			// (automatic)
	if (!ok)
	{	uLog(COLLECT, "*** WARNING: LookupFunctionPtr() mapped a FALSE POSITIVE with %S ***", func_name);
		return "";
	}
	
	return func_name;
}

//---- Collect NOP ------------------------------------------------------------

void	VarCollectors::CollectNOP(LX::DataOutputStream &dos)
{
	m_CollectedList.clear();
	
	dos << m_CollectedList;
}

// nada mas
