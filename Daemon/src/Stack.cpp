// ddt daemon stack, also has watch classes

#include <algorithm>
#include <cassert>
#include <cstring>

#include "LuaGlue.h"

#include "ddt/sharedLog.h"

#include "Stack.h"

using namespace std;
using namespace LX;
using namespace DDT;

//---- Get Lua Stack Depth ----------------------------------------------------

// static
int	DaemonStack::GetLuaStackDepth(lua_State *L)
{
	// need top stack level (can be retrieved with a function?)
	lua_Debug	ar;
	int		stack_depth;
	
	for (stack_depth = 0; true; stack_depth++)
	{	bool	ok = lua_getstack(L, stack_depth, &ar);
		if (!ok)	break;
	}
	
	return stack_depth - 1;
}

//---- Level CTOR -------------------------------------------------------------

	DaemonStack::Level::Level()
{
	m_Line = 0;
	m_nFixedArguments = 0;
	m_nUpValues = 0;
	m_FunctionArguments.clear();
	m_CFlag = true;
	m_OrgFileName.clear();
	m_CleanFileName.clear();
}

//---- Daemon Stack CTOR ------------------------------------------------------

	DaemonStack::DaemonStack()
{	
	Clear();
}

//---- Clear ------------------------------------------------------------------

void	DaemonStack::Clear(void)
{
	m_FirstLuaLevel = -1;		// [undefined]
	m_Levels.clear();
}

//---- Is Empty ? -------------------------------------------------------------

bool	DaemonStack::IsEmpty(void) const
{
	return (m_Levels.size() == 0);
}

//---- Get Nth Stack Level ----------------------------------------------------

const DaemonStack::Level&	DaemonStack::GetLevel(const size_t &ind) const
{
	assert(ind < m_Levels.size());
	
	return m_Levels[ind];
}

//---- Set Fake Level ---------------------------------------------------------

void	DaemonStack::SetFakeLevel(const string &fn, const int &ln)
{
	Clear();
	
	m_FirstLuaLevel = 0;
	
	DaemonStack::Level	lvl;
	
	lvl.m_CleanFileName = fn;
	lvl.m_OrgFileName = fn;
	lvl.m_Line = ln;

	// (has no function)
	lvl.m_CFlag = false;
	lvl.m_FunctionName = "<LOAD FILE> (fake level)";
	lvl.m_AltFunctionName.clear();
	lvl.m_nFixedArguments = 0;
	lvl.m_nUpValues = 0;
	lvl.m_ValidLines.clear();
	
	m_Levels.push_back(lvl);
}

//---- Daemon to Client Stack -------------------------------------------------

ClientStack	DaemonStack::ToClientStack(const LuaGlue &lglue) const
{
	ClientStack	cstack;
	
	cstack.Clear();
	
	for (int i = 0; i < m_Levels.size(); i++)
	{
		const Level&	daem_lvl = GetLevel(i);
		
		ClientStack::Level	lvl;
		
		lvl.m_CFlag = daem_lvl.m_CFlag;
		if (daem_lvl.m_CFlag)
		{	lvl.m_SourceFileName = "C location";
			lvl.m_FunctionName = "";
			lvl.m_AltFunctionName = "";
			lvl.m_Line = -1;
			lvl.m_nFixedArguments = 0;
			lvl.m_FunctionArguments.clear();
		}
		else
		{	lvl.m_SourceFileName = daem_lvl.m_CleanFileName;
			assert(!lvl.m_SourceFileName.empty());
			lvl.m_FunctionName = daem_lvl.m_FunctionName;
			lvl.m_AltFunctionName = daem_lvl.m_AltFunctionName;
			lvl.m_Line = daem_lvl.m_Line;
			lvl.m_nFixedArguments = daem_lvl.m_nFixedArguments;
			lvl.m_FunctionArguments = daem_lvl.m_FunctionArguments;
		}
		lvl.m_nUpValues = daem_lvl.m_nUpValues;
		
		cstack.m_Levels.push_back(lvl);
	}
	
	// (don't return reference)
	return cstack;
}

//---- Fill Lua Stack ---------------------------------------------------------

void	DaemonStack::Fill(lua_State *L, VarCollectors &vcc)
{
	assert(L);
	
	const int	top0 = lua_gettop(L);
	
	uLog(STACK, "DaemonStack::Fill() top = %d", top0);

	Clear();
	
	/*
	const int	LastTracebackLevel = DaemonStack::GetLuaStackDepth(L);
	assert(LastTracebackLevel != -1);
	*/
	
	// pre-hash global function names & pointers for resolution
	vcc.MapGlobalFunctions(L);
	
	int	num_c_levels = 0;
	int	firstLuaLevel = -1;
	
	for (int stack_depth = 0; true; stack_depth++)
	{	
		ddt_Debug	ar;
		
		bool	ok = ddt_getstack(L, stack_depth, &ar);
		if (!ok)	break;					// all done
		
		// fill actual debug info (function name, source filename, line#, #params and #upvalues) -- CAPITAL S
		ok = ddt_getinfo(L, "nSlu", &ar);
		assert(ok);
		
		// save to own callstack
		Level	sl;
		
		if (::strcmp(ar.source, "=[C]") == 0)
		{	// shouldn't have C level after lua level
			sl.m_CFlag = true;
			sl.m_OrgFileName = "C source file";
			sl.m_CleanFileName = "C source file";
			num_c_levels++;
			sl.m_Line = -1;
			
			uLog(STACK, "  level[%zu] C source file", m_Levels.size());
		}
		else
		{	sl.m_OrgFileName = ar.source;
			sl.m_CFlag = false;
			
			uLog(STACK, "  level[%zu] Lua source file %S", m_Levels.size(), sl.m_OrgFileName);
			
			string	clean_fn(ar.source);
			
			// remove @
			if (clean_fn[0] == '@')	
			{	clean_fn.erase(0, 1);
			
				// and backslashes (though should truncate?)
				while (clean_fn.find('\\') != string::npos)	clean_fn.replace(clean_fn.find('\\'), 1, "/");		// __WIN32__
				
				sl.m_CleanFileName = clean_fn;
			}
			else
			{
				sl.m_CleanFileName = "string-contained script";
			}
			
			// copy any function name
			if (ar.name != nil)
				sl.m_FunctionName = ar.name;
			else if (string("main") == ar.what)
				sl.m_FunctionName = "main Lua chunk";
			
			sl.m_Line = ar.currentline;
			assert(sl.m_Line != -1);
			
			// assign 1st lua level
			if (firstLuaLevel == -1)
				firstLuaLevel = m_Levels.size();
		}
		
		// copy # of upvalues (valid for C too?)
		sl.m_nUpValues = ar.nups;
		
		if (!sl.m_CFlag)
		{	
		// get function arguments
			sl.m_nFixedArguments = ar.nparams;
			sl.m_FunctionArguments = vcc.GetFunctionArgs(L, sl.m_nFixedArguments, stack_depth);
			
		// get valid lines
			
			// pushes table of valid lines as KEYs onto stack
			ok = ddt_getinfo(L, "L", &ar);
			assert(ok);
			
			// make table index forward-index
			int	table_stack_index = lua_gettop(L) -1 + 1;
			assert(lua_istable(L, table_stack_index));
			
			// push first key
			lua_pushnil(L);
			
			while (lua_next(L, table_stack_index))
			{
				// key is line at index -2 (value at index -1 but not used)
				assert(lua_isnumber(L, -2));
				
				int	valid_ln = luaL_checknumber(L, -2);
				
				sl.m_ValidLines.push_back(valid_ln);
				
				// remove value; keeps key for next iteration
				lua_pop(L, 1);
			}
			
			// sort line numbers (arrive out of order)
			std::sort(sl.m_ValidLines.begin(), sl.m_ValidLines.end(), [](int i1, int i2){return (i1 < i2);});
			
			// pop line table
			lua_pop(L, 1);
		
			// search for alternative function name in globals
			{	
				ok = ddt_getstack(L, stack_depth, &ar);
				assert(ok);
				
				const int	top1 = lua_gettop(L);
				
				// push function at this level to stack
				ok = ddt_getinfo(L, "f", &ar);
				assert(ok);
				
				ok = lua_isfunction(L, -1);
				assert(ok);
				
				const string	fn_s = vcc.LookupFunctionPtr(L);
				
				sl.m_AltFunctionName = fn_s;
				
				lua_pop(L, 1);
				
				const int	top2 = lua_gettop(L);
				assert(top2 == top1);
			}
		}
		
		// push my stack level
		m_Levels.push_back(sl);
	}
	
	const int	top3 = lua_gettop(L);
	assert(top3 == top0);
	
	// store level of 1st lua function
	m_FirstLuaLevel = firstLuaLevel;
	assert(firstLuaLevel != -1);
	assert(!m_Levels[firstLuaLevel].m_CFlag);
	
	uLog(STACK, "  stack levels: %zu total, %d in C, %d first", m_Levels.size(), num_c_levels, firstLuaLevel);
}


// nada mas
