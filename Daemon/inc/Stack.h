
#pragma once

#include <vector>
#include <string>
#include <memory>

#include "ddt/CommonDaemon.h"

#if LUA_CXX
	#include "lua.h"
	#include "lualib.h"
	#include "lauxlib.h"
#else
	#include "lua.hpp"
#endif

namespace DDT
{
// import into namespace
using std::string;

class LuaGlue;
class VarCollectors;

//---- Lua Stack Dancer ("fair and balanced") ---------------------------------

class LuaStackDancer
{
public:
	// ctor
	LuaStackDancer(lua_State *L)
		: m_L{L}, m_Top{lua_gettop(L)}
	{
		assert(L);
		m_LoopTop = m_Top;
	}
	~LuaStackDancer()
	{
		Bump0();
	}
	
	bool	Bump0(void)
	{
		assert(m_L);
		lua_settop(m_L, m_Top);
		return true;
	}
	
	void	LoopPoint(void)
	{
		assert(m_L);
		m_LoopTop = lua_gettop(m_L);
	}
	
	bool	Bump(void)
	{
		assert(m_L);
		lua_settop(m_L, m_LoopTop);
		return true;
	}
	
private:
	
	lua_State	*m_L;
	const int	m_Top;
	int		m_LoopTop;
};

//---- Daemon Stack -----------------------------------------------------------

class DaemonStack
{
public:
	// ctor
	DaemonStack();
	
	class Level
	{
	public:
		// level ctor
		Level();
		
		string			m_FunctionName;
		std::string		m_AltFunctionName;
		bool			m_CFlag;
		std::string		m_OrgFileName;		// (as passed by the Lua VM, with @, path with \ and /)
		std::string		m_CleanFileName;
		int			m_Line;
		int			m_nFixedArguments;
		int			m_nUpValues;
		std::vector<int>	m_ValidLines;
		
		StringList		m_FunctionArguments;
	};

	bool			IsEmpty(void) const;
	void			Clear(void);
	void			Fill(lua_State *L, VarCollectors &vcc);		// kinda lame, should be static?
	const Level&		GetLevel(const size_t &ind) const;
	void			SetFakeLevel(const std::string &fn, const int &ln);
	
	ClientStack		ToClientStack(const LuaGlue &lglue) const;
	
	int	FirstLuaLevel(void) const	{return m_FirstLuaLevel;}
	
	std::vector<Level>	m_Levels;
	
	static
	int	GetLuaStackDepth(lua_State *L);

private:

	int	m_FirstLuaLevel;
};

} // namespace DDT

// nada mas