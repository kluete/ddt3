// ddt daemon LuaJIT glue

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct lua_State;

namespace DDT
{

using std::string;
using std::unordered_map;

class IJitGlue
{
public:
	virtual ~IJitGlue() = default;
	
	virtual void	ToggleJit(lua_State *L, const bool f) = 0;
	virtual bool	IsProfiling(void) const = 0;
	virtual bool	ToggleProfiler(lua_State *L, const bool f, const int interval_ms, const size_t max_depth) = 0;		// returns [ok]
	
	static
	IJitGlue*	Create(lua_State *L);
};

} // namespace DDT

// nada mas

