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

// project breakpoints
// - do NOT use UI to determine breakpoint state; should ALWAYS use internal logic

#include <algorithm>

#include "Controller.h"

#include "ProjectBreakPoints.h"

using namespace std;

//---- CTOR -------------------------------------------------------------------

	ProjectBreakpoints::ProjectBreakpoints()
{
	
}

//---- DTOR -------------------------------------------------------------------

	ProjectBreakpoints::~ProjectBreakpoints()
{
}

// nada mas