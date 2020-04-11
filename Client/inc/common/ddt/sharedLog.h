// ddt shared log defs

#pragma once

#include "lx/ulog.h"

namespace LX
{

#define LOG_MACRO(arg)	arg = #arg##_log

enum SharedLogLevel : LOG_HASH_T
{
	LOG_MACRO(STOPWATCH),
	LOG_MACRO(MSG_IN),
	LOG_MACRO(MSG_OUT),
	LOG_MACRO(MSG_UDP),
	LOG_MACRO(THREADING),
	LOG_MACRO(LUA_USER),

	LOG_MACRO(USER_MSG),
	LOG_MACRO(INTERCEPT),
	LOG_MACRO(RECURSE),
	LOG_MACRO(STACK),
	LOG_MACRO(ASIO),
	LOG_MACRO(COLLECT),
	LOG_MACRO(GLUE),
	LOG_MACRO(SESSION),
	LOG_MACRO(PROF),
	LOG_MACRO(BREAKPOINT),
	LOG_MACRO(LUA_ERR),
};	

#undef LOG_MACRO

} // namespace LX

// nada mas
