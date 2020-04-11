
#include <cassert>

#include "TopFrame.h"

#include "ClientState.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

class ClientState : public IClientState
{
public:	
	ClientState(TopFrame &tf)
		: m_TopFrame(tf),
		m_GlobalBreakpointsFlag(true),
		m_StepModeLinesFlag(true),
		m_LoadBreakModeFlag(false),
		m_JitModeFlag(false)
	{
		(void)m_TopFrame;
	}
	
	bool	GetGlobalBreakpointsFlag(void) const override	{return m_GlobalBreakpointsFlag;}
	void	SetGlobalBreakpointsFlag(const bool f) override
	{
		m_GlobalBreakpointsFlag = f;
	}
	
	bool	GetStepModeLinesFlag(void) const override	{return m_StepModeLinesFlag;}
	void	SetStepModeLinesFlag(const bool f) override
	{
		m_StepModeLinesFlag = f;
	}
	
	bool	GetLoadBreakFlag(void) const override	{return m_LoadBreakModeFlag;}
	void	SetLoadBreakFlag(const bool f) override
	{
		m_LoadBreakModeFlag = f;
	}
	
	bool	GetJitFlag(void) const override			{return m_JitModeFlag;}
	void	SetJitFlag(const bool f) override
	{
		m_JitModeFlag = f;
	}
	
private:

	TopFrame&	m_TopFrame;
	bool		m_GlobalBreakpointsFlag;
	bool		m_StepModeLinesFlag;
	bool		m_LoadBreakModeFlag;
	bool		m_JitModeFlag;
};

//---- INSTANTIATE ------------------------------------------------------------

// static
IClientState*	IClientState::Create(TopFrame &tf)
{
	return new ClientState(tf);
}

// nada mas