/***********************************************
***    Lua DDT debugger                     ***
***    version 0.92b                        ***
***    February 14, 2008                    ***
***    written by Peter Laufenberg          ***
***    copyright Inhance Digital Corp.      ***
***    see accompanying license.txt         ***
***                                         ***
***    Inhance Digital Corporation          ***
***    8057 Beverly Blvd, Ste 200           ***
***    Los Angeles, CA 90048                ***
***    www.inhance.com/developer            ***
***                                         ***
**********************************************/

// PASS-THROUGH debugger implementation

#if 0

#include "PassThruDebugger.h"
#include <iostream>

//---- static PASS-THROUGH CTOR -----------------------------------------------

LuaDDTClass*		LuaDDTClass::NewPassThru()
{
	// doesn't check singleton!
	
	return new PassThruDebugger();
}

//---- CTOR -------------------------------------------------------------------

	PassThruDebugger::PassThruDebugger()
{
	L = nil;		// (no lua state at creation time)
}

//---- DTOR -------------------------------------------------------------------

	PassThruDebugger::~PassThruDebugger()
{
	// nop
}

bool	PassThruDebugger::LoadProject(const wxString &source_filename, const wxString &prefs_folderpath)
{
	return true;
}	

//---- Delete self ------------------------------------------------------------

void	PassThruDebugger::Delete(void)
{
	delete this;
}

//---- Dump Call Stack --------------------------------------------------------

static
void	DumpCallStack(lua_State *L)
{
	std::cout << "lua callstack:\n";
	
	for (int stack_depth = 0; true; stack_depth++)
	{	lua_Debug	ar;
		
		bool	ok = lua_getstack (L, stack_depth, &ar);
		if (!ok)	break;					// all done
		
		// fill actual debug info (source filename and line#) -- CAPITAL S
		ok = lua_getinfo(L, "Sl", &ar);
		wxASSERT(ok);
		
		std::cout << " level" << stack_depth << ": ";
		std::cout << "\"" << ar.source << "\" ";
		std::cout << "line " << ar.currentline << "\n";
	}
}

//---- Lua Error Callback -----------------------------------------------------

	// gets called before the stack unwinds (raw error string will be on top of stack)
static
int	LuaErrFunction(lua_State *L)
{
	wxString	err_str;
	
	// make sure is string on stack otherwise will call lua_tostring()
	if (lua_type(L, -1) == LUA_TSTRING)
		err_str = luaL_checkstring(L, -1);
	else if (lua_type(L, -1) == LUA_TNUMBER)
		err_str = wxString::Format("Lua error %d", (int) luaL_checknumber(L, -1));
	else	err_str = "<UNKNOWN LUA ERROR>";	// should never happen?
	
	std::cout << err_str << "\n";
	
	DumpCallStack(L);
	
	// return the error string
	return 1;
}

//---- NOP --------------------------------------------------------------------

static
int	lg_NOP(lua_State *L)
{
	// nop
	return 0;
}

//---- Protected, Lua Call ----------------------------------------------------

int	PassThruDebugger::lua_pcall_ddt(lua_State *l, int nargs, int nresults)
{
	wxASSERT_MSG((l != nil), "lua_State was not initialized!");
	
	L = l;
	
	// nop any existing ddt functions
	lua_pushcfunction(L, lg_NOP);
	lua_setglobal(L, "breakpoint");

	int	base = lua_gettop(L) - nargs;	// function index
	lua_pushcfunction(L, LuaErrFunction);	// push error handling function
	lua_insert(L, base);			// put it under chunk and args
	
	int	res = lua_pcall(L, nargs, nresults, base/*error function index*/);
	
	lua_remove(L, base);			// remove error handling function

	return res;
}

#endif

// nada mas