// Lua ddt variable collectors

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>

#include "Stack.h"

#include "ddt/Collected.h"
#include "lx/xutils.h"

// forward declarations
struct lua_State;
struct lua_Debug;

namespace DDT
{

LX::DataOutputStream& operator<<(LX::DataOutputStream &dos, const DDT::Collected &coll);
LX::DataOutputStream& operator<<(LX::DataOutputStream &dos, const DDT::CollectedList &collList);

	class LuaGlue;
	enum class TLUA : int;

class CollectProps
{
public:
	// ctors
	CollectProps()	{}
	CollectProps(const uint32_t &mask, const uint32_t &props)
		: m_VarMask(mask), m_PropBits(props)
	{
	}

	uint32_t	m_VarMask;
	uint32_t	m_PropBits;
};

//---- Var Collectors ---------------------------------------------------------

class VarCollectors
{
public:
	VarCollectors();
	virtual ~VarCollectors()	{}
	
	void	CopyStack(const DaemonStack &stack, const StringList &expanded_tables, const uint32_t hide_mask);
	
	void	CollectNOP(LX::DataOutputStream &dos);
	void	CollectLuaLocals(lua_State *L, const int &stack_depth, const uint32_t &hide_mask, LX::DataOutputStream &dos);
	void	CollectLuaGlobals(lua_State *L, const int &stack_depth,  const uint32_t &hide_mask, LX::DataOutputStream &dos);
	void	CollectLuaWatches(lua_State *L, const StringList &watch_list, LX::DataOutputStream &dos);
	void	CollectNOPWatches(lua_State *L, const StringList &watch_list, LX::DataOutputStream &dos);
	
	StringList	GetFunctionArgs(lua_State *L, const int &n_fixed_args, const int &stack_depth);
	
	void	MapGlobalFunctions(lua_State *L);
	std::string	LookupFunctionPtr(lua_State *L) const;
	
private:
	
	bool		SolveOneVar(lua_State *L, const std::string &var_name);
	uint32_t	LuaToCollectedtype(const TLUA &ltype, COLLECTED_T &collec_t) const;		// returns MASK
	void		CollectENVGlobals(lua_State *L, const SCOPE_T &scope);
	void		CollectLuaRegistry(lua_State *L);
	
	void	PushUnsolved(const std::string &key_name);
	
	void	CollectUpvalues(lua_State *L, const int &stack_depth, const uint32_t &hide_mask);
	
	void	DumpLuaTableRecursive(lua_State *L, const std::string &var_path, int indent);
	bool	CollectNextVariable(lua_State *L, const std::string &key_name, const std::string &parent_var, const int indent, const SCOPE_T level);
	
	DDT::CollectedList				m_CollectedList;
	StringSet					m_ExpandedTablesHashSet;
	
	DaemonStack					m_CallStack;
	int						m_RootItemIndex;
	
	std::unordered_map<const void*, std::string>	m_LuaFunctionLUT;
	std::unordered_set<const void*>			m_VisitedUpvalues;

	uint32_t					m_HideMask;
};

} // namespace DDT

// nada mas
