// ddt client state

#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace DDT_CLIENT
{
class TopFrame;

using std::vector;
using std::unique_ptr;

//---- Client State interface -------------------------------------------------

class IClientState
{
public:
	virtual ~IClientState() = default;
	
	virtual bool	GetGlobalBreakpointsFlag(void) const = 0;
	virtual void	SetGlobalBreakpointsFlag(const bool f) = 0;
	
	virtual bool	GetStepModeLinesFlag(void) const = 0;
	virtual void	SetStepModeLinesFlag(const bool f) = 0;
	
	virtual bool	GetLoadBreakFlag(void) const = 0;
	virtual void	SetLoadBreakFlag(const bool f) = 0;
	
	virtual bool	GetJitFlag(void) const = 0;
	virtual void	SetJitFlag(const bool f) = 0;
	
	static
	IClientState*	Create(TopFrame &tf);
};

} // namespace DDT_CLIENT

// nada mas
