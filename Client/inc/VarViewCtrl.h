// Lua variable view control

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <initializer_list>

#include "wx/wx.h"

#include "UIConst.h"

#include "ddt/Collected.h"
#include "ddt/CommonClient.h"

#include "BottomNotebook.h"

// forward references

namespace DDT
{
	class Collected;
	
} // namespace DDT

namespace DDT_CLIENT
{
using namespace DDT;

class TopFrame;
class BottomNotePanel;
class WatchBag;

using std::string;
using std::vector;
using std::unordered_map;

class IVarViewFlags
{
public:
	virtual ~IVarViewFlags() = default;
	
	virtual bool		GetFlag(const int index) const = 0;
	virtual void		SetFlag(const int index, const bool f) = 0;
	virtual uint32_t	GetMask(void) const = 0;
	virtual void		SetMask(const uint32_t mask) = 0;
	
	static
	IVarViewFlags*	Create(TopFrame &tf);
};

//---- Var View Info ----------------------------------------------------------

struct varview_info
{
	varview_info(const PANE_ID pane_id, const bool editable_f, const CLIENT_MSG msg, const uint32_t id, const string &label)
		: m_PaneID(pane_id), m_EditableFlag(editable_f), m_RequestMsg(msg), m_ClientID(id), m_Label(label)
	{}
	
	const PANE_ID		m_PaneID;
	const bool		m_EditableFlag;
	const CLIENT_MSG	m_RequestMsg;
	const uint32_t		m_ClientID;		// requester id
	const string		m_Label;
};

//---- Variable View Control interface ----------------------------------------

class IVarViewCtrl
{
public:	
	virtual ~IVarViewCtrl() = default;
	
	static
	IVarViewCtrl*	Create(IBottomNotePanel &bottom_notebook, int id, TopFrame &tf, Controller &controller, IVarViewFlags &vwf, const bool showscope_f, WatchBag *watch_ptr);
	
	static
	varview_info	GetVarViewInfo(const PANE_ID pane_id);

	static
	unordered_map<COLUMN_T, string>		GetCollectedFields(const Collected &ce, const uint32_t flags);
};

} // namespace DDT_CLIENT

// nada mas
