// ddt daemon LuaJIT glue

#include <cassert>
#include <string.h>		// (for strcmp())
#include <list>
#include <algorithm>

#include "lua.hpp"

#if DDT_JIT
	#include "luajit.h"
#endif

#include "ddt/sharedLog.h"

#include "DaemonJIT.h"

using namespace std;
using namespace LX;
using namespace DDT;

//---- Jit Dummy --------------------------------------------------------------

class JitDummy : public IJitGlue
{
public:
	JitDummy(lua_State *L)
	{
		(void)L;
	}
	
	void	ToggleJit(lua_State *L, const bool f) override	{}
	
	bool	IsProfiling(void) const	override {return false;}
	bool	ToggleProfiler(lua_State *L, const bool f, const int interval_ms, const size_t max_depth) override	{return false;}
};

#if DDT_JIT

//---- LuaJIT Glue ------------------------------------------------------------

class JitGlue : public IJitGlue
{
public:
	JitGlue(lua_State *L);
	virtual ~JitGlue() = default;
	
	void	ToggleJit(lua_State *L, const bool f) override;
	
	bool	IsProfiling(void) const override;
	bool	ToggleProfiler(lua_State *L, const bool f, const int interval_ms, const size_t max_depth) override;		// returns [ok]
	
	void	OnSample(lua_State *L, const int n_samples, const int vm_state);

private:
	
	void	InitProf(const size_t max_depth);
	void	DumpProf(void);
	
	bool	m_JitFlag;
	bool	m_ProfilerFlag;
	size_t	m_MaxDepth;
	
	struct tally
	{
		size_t	leaf;
		size_t	cummul;
	};
	
	unordered_map<string, tally>	m_SourceLineToCntMap;
	size_t				m_NonLuaCnt, m_CLevelCnt;
};

static
void	s_profiler_callback(void *data, lua_State *L, int n_samples, int vm_state)
{
	JitGlue	*jglue = static_cast<JitGlue*>(data);
	assert(jglue);
	
	jglue->OnSample(L, n_samples, vm_state);
}

//---- CTOR -------------------------------------------------------------------

	JitGlue::JitGlue(lua_State *L)
{
	assert(L);
	
	m_JitFlag = false;
	m_ProfilerFlag = false;
}

//---- Toggle JIT on/off ------------------------------------------------------

void	JitGlue::ToggleJit(lua_State *L, const bool f)
{
	uLog(PROF, "JitGlue::ToggleJit(f = %c)", f);
	
	assert(L);
	m_JitFlag = f;
	
	const int	res = luaJIT_setmode(L, 0/*stack level*/, LUAJIT_MODE_ENGINE | (f ? LUAJIT_MODE_ON : LUAJIT_MODE_OFF));
	assert(res);
}

//---- Is Profiling ? ---------------------------------------------------------

bool	JitGlue::IsProfiling(void) const
{
	return (m_JitFlag && m_ProfilerFlag);		// bullshit !
}

//---- Toggle Profiler on/off -------------------------------------------------

bool	JitGlue::ToggleProfiler(lua_State *L, const bool f, const int interval_ms, const size_t max_depth)
{
	if (f)	uLog(PROF, "JitGlue::ToggleProfiler(ON), interval %d ms, depth %zu", interval_ms, max_depth);
	else	uLog(PROF, "JitGlue::ToggleProfiler(off)");
	
	assert(L);
	
	if (f == m_ProfilerFlag)
	{	// error
		uErr("error - redundant profiler toggle");
		return false;
	}
	
	m_ProfilerFlag = f;
	
	if (f)
	{	InitProf(max_depth);
		
		// granularity
		//   (f)unction
		//   (l)ine
		const string	mod = string("l") + "i" + to_string(interval_ms);
		
		luaJIT_profile_start(L, mod.c_str(), s_profiler_callback, this);
	}
	else
	{
		luaJIT_profile_stop(L);
		
		DumpProf();
	}
	
	return true;		// ok
}

//---- Init Profiler ----------------------------------------------------------

void	JitGlue::InitProf(const size_t max_depth)
{
	uLog(PROF, "JitGlue::InitProf(max_depth %zu)", max_depth);
	
	m_MaxDepth = max_depth;
	
	m_SourceLineToCntMap.clear();
	m_NonLuaCnt = 0;
	m_CLevelCnt = 0;
}

//---- On Profiler callback ---------------------------------------------------

void	JitGlue::OnSample(lua_State *L, const int n_samples, const int vm_state)
{
	assert(m_ProfilerFlag);
	
	switch (vm_state)
	{
		case 'N':
		case 'I':
		{
			assert(L);
			
			// cummulate with upper levels
			for (size_t level = 0; level <= m_MaxDepth; level++)
			{
				lua_Debug	ar;
		
				bool	ok = lua_getstack(L, level, &ar);
				if (!ok)	return;				// reached top
				
				//  get source filename & line#
				ok = lua_getinfo(L, "Sl", &ar);
				assert(ok);
				if (::strcmp(ar.source, "=[C]") == 0)
				{	// C stack level function
					m_CLevelCnt++;
					continue;
				}
				
				const string	key = string(ar.source) + ":" + to_string(ar.currentline);		// shouldn't do here ?
				auto	it = m_SourceLineToCntMap.find(key);
				if (m_SourceLineToCntMap.end() == it)
				{	// first source:line entry
					it = m_SourceLineToCntMap.insert(m_SourceLineToCntMap.end(), {key, {0, 0}});
				}
				
				if (0 == level)
					it->second.leaf += n_samples;		// should add to cummul AS WELL ?
				/*else*/	it->second.cummul += n_samples;
			}
		}	break;
		
		default:
			
			m_NonLuaCnt += n_samples;
			break;
	}
}

//---- Dump Profiled ----------------------------------------------------------

void	JitGlue::DumpProf(void)
{
	uLog(PROF, "JitGlue::DumpProf(%zu entries)", m_SourceLineToCntMap.size());
	
	using entry = pair<string, tally>;
	list<entry>	sorted_lines;
	
	for (const auto &it : m_SourceLineToCntMap)	sorted_lines.emplace_back(it.first, it.second);
	
	sorted_lines.sort([](const entry &e1, const entry &e2){return (e1.first < e2.first);});
	
	for (const auto &it : sorted_lines)
		uLog(PROF, "  %s = leaf %5zu, cummul %5zu", it.first, it.second.leaf, it.second.cummul);
	
	uLog(PROF, "");
	uLog(PROF, " (non-Lua %zu)", m_NonLuaCnt);
	uLog(PROF, " (C stack levels %zu)", m_CLevelCnt);
	uLog(PROF, "");
}

#endif // DDT_JIT

//---- INSTANTIATE ------------------------------------------------------------

// static
IJitGlue*	IJitGlue::Create(lua_State *L)
{
	#if DDT_JIT
		return new JitGlue(L);
	#else
		return new JitDummy(L);
	#endif
}

// nada mas

