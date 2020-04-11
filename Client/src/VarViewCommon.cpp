// VarView common code

#include <cassert>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <thread>

#include "JuceHeader.h"

#include "lx/ulog.h"
#include "lx/color.h"

#include "lx/misc/geometry.h"
#include "lx/juce/jeometry.h"
#include "lx/juce/jwxBinder.h"
#include "lx/juce/JuceApp.h"

#include "lx/stream/MemoryInputStream.h"
#include "lx/stream/MemoryOutputStream.h"
#include "lx/stream/DataInputStream.h"
#include "lx/stream/DataOutputStream.h"

#include "TopFrame.h"
#include "Controller.h"
#include "TopNotebook.h"
#include "BottomNotebook.h"

#include "UIConst.h"
#include "SourceFileClass.h"

#include "sigImp.h"
#include "logImp.h"

#include "ddt/MessageQueue.h"

#include "VarViewCtrl.h"

using namespace std;
using namespace juce;
using namespace LX;
using namespace DDT_CLIENT;

static const
unordered_map<PANE_ID, varview_info, EnumHash>	s_PaneToVarRequestType
{
	{PANE_ID_LOCALS,	{PANE_ID_LOCALS, 	false,	CLIENT_MSG::REQUEST_LOCALS,	VAR_SOLVE_REQUESTER::SOLVE_REQUESTER_LOCALS,		"locals"}},
	{PANE_ID_GLOBALS,	{PANE_ID_GLOBALS,	false,	CLIENT_MSG::REQUEST_GLOBALS,	VAR_SOLVE_REQUESTER::SOLVE_REQUESTER_GLOBALS,		"globals"}},
	{PANE_ID_WATCHES,	{PANE_ID_WATCHES,	true,	CLIENT_MSG::REQUEST_WATCHES,	VAR_SOLVE_REQUESTER::SOLVE_REQUESTER_WATCHES,		"watches"}},
	
	{PANE_ID_JUCE_LOCALS,	{PANE_ID_JUCE_LOCALS,	false,	CLIENT_MSG::REQUEST_LOCALS,	VAR_SOLVE_REQUESTER::SOLVE_REQUESTER_JUCE_LOCALS,	"jlocals"}},
};

class VarViewFlags : public IVarViewFlags
{
public:
	VarViewFlags(TopFrame &tf)
		: m_TopFrame(tf)
	{
		(void)m_TopFrame;
		
		for (int i = 0; i < VV_LAST_UI; i++)	m_Flags[i] = false;
	
		// defaults bf project prefs
		m_Flags[VV_NUMBER] = true;
		m_Flags[VV_BOOLEAN] = true;
		m_Flags[VV_STRING] = true;
		m_Flags[VV_LUA_FUNCTION] = true;
		m_Flags[VV_C_FUNCTION] = true;
		m_Flags[VV_TABLE] = true;
		m_Flags[VV_THREAD] = true;
	}
	
	
	bool	GetFlag(const int index) const override		{return m_Flags[index];}

	void	SetFlag(const int index, const bool f) override
	{
		// uLog(UI, "VarViewFlags::SetFlag(index = %d, flag = %c)", index, f);
		
		m_Flags[index] = f;
		
		// save PROJECT prefs
		// m_TopFrame.DirtyProjectPrefs();
		
		// should CACHE ?
		// m_TopFrame.UpdateVarViewPrefs();
	}

	uint32_t	GetMask(void) const override
	{	
		uint32_t	res = 0;

		for (int i = 0; i < (int) VV_LAST_UI; i++)
		{
			const uint32_t	mask = (1ul << i);
			
			if (m_Flags[i])		res |= mask;
		}
		
		return res;
	}

	void	SetMask(const uint32_t mask) override
	{
		for (int i = 0; i < (int) VV_LAST_UI; i++)
		{
			const uint32_t	tst = (1ul << i);
			
			const bool	f = ((mask & tst) != 0);
			
			SetFlag(i, f);
		}
	}

private:
	
	TopFrame	&m_TopFrame;	
	bool		m_Flags[VV_LAST_UI];		// specialized ?
};

//---- Get Info ---------------------------------------------------------------

// static
varview_info	IVarViewCtrl::GetVarViewInfo(const PANE_ID pane_id)
{
	assert(s_PaneToVarRequestType.count(pane_id));
	
	return s_PaneToVarRequestType.at(pane_id);
}
	
//---- INSTANTIATE ------------------------------------------------------------

// static
IVarViewFlags*	IVarViewFlags::Create(TopFrame &tf)
{
	return new VarViewFlags(tf);
}

//---- Get Collected (text) field ---------------------------------------------
	
// static
unordered_map<COLUMN_T, string>	IVarViewCtrl::GetCollectedFields(const Collected &ce, const uint32_t mask)
{
	unordered_map<COLUMN_T, string>	res;
	
	res.emplace(COLUMN_T::KEY, ce.GetDecoratedKey());
	res.emplace(COLUMN_T::VAL_STR, ce.GetDecoratedVal());
	res.emplace(COLUMN_T::VAL_TYPE, ce.GetDecoratedVal().append(ce.HasMetatable() ? " MT" : ""));
	res.emplace(COLUMN_T::SCOPE, (mask & VAR_FLAG_SHOW_SCOPE) ? ce.GetScopeStr() : string(""));
	res.emplace(COLUMN_T::VAL_PATH, (mask & VAR_FLAG_SHOW_PATH) ? ce.GetPath() : string(""));
	
	return res;
}

// nada mas