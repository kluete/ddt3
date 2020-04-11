/**********************************************
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

// source breakpoints

#ifndef LUA_DDT_PROJECT_BREAKPOINTS_H_INCLUDED
#define	LUA_DDT_PROJECT_BREAKPOINTS_H_INCLUDED

#include <cstdint>
#include <vector>
#include <unordered_set>

#include "ddt/sharedDefs.h"

//---- Project Break Points Class ---------------------------------------------

class ProjectBreakpoints
{
public:
	// ctor
	ProjectBreakpoints();
	// dtor
	virtual ~ProjectBreakpoints();

};

#endif // LUA_DDT_PROJECT_BREAKPOINTS_H_INCLUDED

// nada mas