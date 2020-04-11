// lua ddt wxEvent filter

#include "wx/window.h"

#include "TopFrame.h"

#include "logImp.h"

#include "lx/misc/autolock.h"

#include "EvtFilter.h"

using namespace std;
using namespace DDT_CLIENT;

// constexpr auto	FOCUS = "FOCUS"_log;

// WARNING: can NOT use statistically defined wxEventType -- must be instantiated bt wx FIRST!

// const wxEventCategory	cat = e.GetEventCategory();	// mask

//---- CTOR -------------------------------------------------------------------

	MyEventFilter::MyEventFilter()
		: m_OrgHandler(nil)
{
	m_EnabledFlag = false;
	m_RecurseLock = false;
	
	m_WantSet.clear();
	m_MouseSet.clear();
	m_FocusSet.clear();
	m_KeySet.clear();
	m_TypeToName.clear();
	
	// m_IgnoreSet = {, wxEVT_IDLE, wxEVT_CHILD_FOCUS};
	// wxEVT_SHOW, wxEVT_UPDATE_UI
	// wxEVT_MENU_OPEN, wxEVT_MENU_HIGHLIGHT, wxEVT_MENU_CLOSE
	// wxEVT_THREAD, wxEVT_TIMER
	
	// m_WantSet = {wxEVT_SET_FOCUS, wxEVT_KILL_FOCUS, wxEVT_CHILD_FOCUS};
	
	// m_MouseSet = {wxEVT_MOTION, wxEVT_LEFT_DOWN, wxEVT_LEFT_UP};
	// m_FocusSet = {wxEVT_SET_FOCUS, wxEVT_KILL_FOCUS, wxEVT_CHILD_FOCUS};
	// m_KeySet = {wxEVT_KEY_DOWN, wxEVT_KEY_UP, wxEVT_CHAR_HOOK};
	
	// (remove noise)
	m_IgnoreSet = {wxEVT_ERASE_BACKGROUND, wxEVT_NC_PAINT, wxEVT_PAINT, wxEVT_IDLE, wxEVT_SET_CURSOR};
	
	m_TypeToName = {	{wxEVT_ERASE_BACKGROUND,	"wxEVT_ERASE_BACKGROUND"},
				{wxEVT_NC_PAINT,		"wxEVT_NC_PAINT"},
				{wxEVT_PAINT,			"wxEVT_PAINT"},
				{wxEVT_IDLE,			"wxEVT_IDLE"},
				{wxEVT_SET_CURSOR,		"wxEVT_SET_CURSOR"},
				
				{wxEVT_SET_FOCUS,		"wxEVT_SET_FOCUS"},
				{wxEVT_KILL_FOCUS,		"wxEVT_KILL_FOCUS"},
				{wxEVT_CHILD_FOCUS,		"wxEVT_CHILD_FOCUS"},
				
				{wxEVT_MENU_OPEN,		"wxEVT_MENU_OPEN"},
				{wxEVT_MENU_HIGHLIGHT,		"wxEVT_MENU_HIGHLIGHT"},
				{wxEVT_MENU_CLOSE,		"wxEVT_MENU_CLOSE"},
				
				};
}

//---- DTOR -------------------------------------------------------------------

	MyEventFilter::~MyEventFilter()
{
	RemoveSelf();
	
	m_EnabledFlag = false;
}

//---- Add Self ---------------------------------------------------------------

void	MyEventFilter::AddSelf(wxEvtHandler *hdl)
{
	assert(hdl);
	
	m_OrgHandler = hdl;
	
	hdl->AddFilter(this);
}

//---- Remove Self ------------------------------------------------------------

void	MyEventFilter::RemoveSelf(void)
{
	if (!m_OrgHandler)	return;		// nop
	
	m_OrgHandler->RemoveFilter(this);
	
	m_OrgHandler = nil;
}

//---- Toggle On/Off ----------------------------------------------------------
	
void	MyEventFilter::Toggle(const bool &f)
{
	m_EnabledFlag = m_OrgHandler && f;
}
	
//---- Filter Event -----------------------------------------------------------

int	MyEventFilter::FilterEvent(wxEvent &evt)
{
	assert(m_OrgHandler);
	if (!m_EnabledFlag)	return Event_Skip;
	if (m_RecurseLock)	return Event_Ignore;
	
	AutoLock	recurse_guard(m_RecurseLock);
	
	const wxEventType	t = evt.GetEventType();
	
	if (m_IgnoreSet.count(t))	return Event_Skip;
	
	if (m_FocusSet.count(t))
	{
		wxWindow	*w = nil;
		
		wxFocusEvent	*e_p = static_cast<wxFocusEvent*> (&evt);
		if (e_p)
		{
			const wxFocusEvent	&e {*e_p};
		
			w = e.GetWindow();
		}
		else
		{	wxChildFocusEvent	*e2_p = static_cast<wxChildFocusEvent*> (&evt);
			if (!e2_p)	return Event_Skip;
		
			const wxChildFocusEvent	&e {*e2_p};
			
			w = e.GetWindow();
		}
		
		if (!w)		return Event_Skip;
		
		const int	win_id = w->GetId();
		const int	id = evt.GetId();
		string		typ_name;
		
		if (m_TypeToName.count(t))	typ_name = m_TypeToName.at(t);
		
		// const wxString	id_name {wxString{TopFrame::GetWinIDName(win_id) + wxString(" ") + ((t == wxEVT_SET_FOCUS) ? "setfocus" : "killfocus")}};
		const wxString	id_name {wxString{TopFrame::GetWinIDName(win_id) + wxString(" ") + wxString(typ_name)}};
		
		uLog(FOCUS, "focus %s (id %d)", id_name, id);
		
		const wxWindow	*w2 = wxWindow::FindFocus();
		if (w2)
		{
			uLog(FOCUS, "   focus id name2 %S", TopFrame::GetWinIDName(w2->GetId()));
		}
		
		return Event_Skip;
	}
	
	if (0)		// m_KeySet.count(t))
	{
		wxKeyEvent	*e_p = static_cast<wxKeyEvent*> (&evt);
		if (!e_p)	return Event_Skip;
		
		const wxKeyEvent	&e{*e_p};
		const uint32_t		raw_vk = e.GetRawKeyCode();
		
		if ((e.GetKeyCode() == 'f') && (e.AltDown() || e.ControlDown()))
		{
			uLog(EV_KEY, "pressed ctrl-F");
		}
		if (raw_vk == 'f')
		{	
			uLog(EV_KEY, "pressed 0x%08x", raw_vk);
			return Event_Skip;
		}
		uLog(EV_KEY, "unknown 0x%08x", raw_vk);
		
		return Event_Skip;
	}
	
	if (0)		// m_MouseSet.count(t))
	{
		// assert(s_MouseSet.count(t));
		
		wxMouseEvent	*mouse_p = static_cast<wxMouseEvent*> (&evt);
		if (mouse_p)
		{	const wxMouseEvent	&e {*mouse_p};
		
			const wxPoint	pt = e.GetPosition();
			
			if (e.LeftDown())
			{
				m_DragMode = DRAG_MODE::START;
				m_InitPt = pt;
				uLog(EV_DRAG, "DRAG_MODE::START");
			}
			else if (e.Dragging() && (DRAG_MODE::NONE != m_DragMode))
			{
				if (DRAG_MODE::START == m_DragMode)
				{
					const int	tolerance = 3;
					int	dx = abs(m_InitPt.x - pt.x);
					int	dy = abs(m_InitPt.y - pt.y);
					if ((dx <= tolerance) && (dy <= tolerance))	return Event_Ignore;
					
					// start drag
					m_DragMode = DRAG_MODE::DRAGGING;
				}
				else if (DRAG_MODE::DRAGGING == m_DragMode)
				{
					uLog(EV_DRAG, "DRAGGING(%d, %d)", pt.x, pt.y);
				}
			}
			else if (e.LeftUp() && (DRAG_MODE::NONE != m_DragMode))
			{
				m_DragMode = DRAG_MODE::NONE;
				uLog(EV_DRAG, "DRAG_MODE::NONE");
				return Event_Skip;
			}
		}
		
		return Event_Ignore;
	}
	
	if (!m_WantSet.count(t))	return Event_Skip;
	
	const wxClassInfo	*info = evt.GetClassInfo();
	assert(info);
	
	const int	id = evt.GetId();
	wxWindow	*win = wxWindow::FindWindowById(id);
	
	// get event type name
	const wxString		typ_s {wxString(info->GetClassName())};
	assert(!typ_s.IsEmpty());
	
	const wxString	id_name {wxString{TopFrame::GetWinIDName(id)}};
	
	if (win)
	{	const wxString	label = win->GetLabel();
		
		// get class info
		const wxClassInfo	*class_info = win->GetClassInfo();
		assert(class_info);
	
		// get class type name
		const wxString	class_type = class_info->GetClassName();
		assert(!class_type.IsEmpty());

		uLog(EV_FILTER, "evt %s, id: %d, name: %S, label %S, typ %s", typ_s, id, id_name, label, class_type);
	}
	else
	{
		uLog(EV_FILTER, "evt %s, id: %d, name: %S (NULL win)", typ_s, id, id_name);
	}
	
	return Event_Skip;
}

// nada mas
