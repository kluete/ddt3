// breakpoints list control

#include <algorithm>

#include "wx/wx.h"
#include "wx/listctrl.h"

#include "sigImp.h"

#include "TopFrame.h"
#include "Controller.h"

#include "ddt/CommonDaemon.h"

#include "BottomNotebook.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

//---- Breakpoints List Control Class -----------------------------------------

class BreakpointsListCtrl: public wxListCtrl, public PaneMixIn, private SlotsMixin<CTX_WX>
{
public:
	BreakpointsListCtrl(IBottomNotePanel &parent, int id, Controller &controller);
	virtual ~BreakpointsListCtrl() = default;
	
	// bool		IsPaneShown(void) const override	{return m_MixIn->IsPaneShown();}
	// wxWindow*	GetWxWindow(void) override		{return m_MixIn->GetWxWindow();}
	
private:
	
	void	UpdateView(void);
	
	void	ClearVariables(void);
	void	RequestVarReload(void)
	{
		UpdateView();
	}
	
	void	SelectAll(void);
	void	DeleteSelectedBreakpoints(void);
	
	// events
	void	OnItemSelected(wxListEvent &e);
	void	OnItemDoubleClick(wxListEvent &e);
	void	OnKeyDown(wxKeyEvent &e);
	void	OnRefreshSfcBreakpoints(ISourceFileClass *sfc, vector<int> bp_list);
	
	// unique_ptr<IPaneMixIn>	m_MixIn;
	
	Controller	&m_Controller;
	
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(BreakpointsListCtrl, wxListCtrl)

	EVT_LIST_ITEM_ACTIVATED(	-1,					BreakpointsListCtrl::OnItemDoubleClick)
	EVT_KEY_DOWN(								BreakpointsListCtrl::OnKeyDown)
	
END_EVENT_TABLE()

#define	style	(wxLC_REPORT | /*wxLC_SINGLE_SEL |*/ wxLC_HRULES | wxLC_VRULES)

//---- CTOR -------------------------------------------------------------------

	BreakpointsListCtrl::BreakpointsListCtrl(IBottomNotePanel &bottom_panel, int id, Controller &controller)
		: wxListCtrl(bottom_panel.GetWxWindow(), id, wxDefaultPosition, wxDefaultSize, style),
		PaneMixIn(bottom_panel, id, this),
		// m_MixIn(new PaneMixIn(bottom_panel, id, this)),
		m_Controller(controller)
{
	InsertColumn(0, "source");
	InsertColumn(1, "line");
	
	MixConnect(this, &BreakpointsListCtrl::ClearVariables, bottom_panel.GetSignals(PANE_ID_BREAKPOINTS).OnPanelClearVariables);
	MixConnect(this, &BreakpointsListCtrl::RequestVarReload, bottom_panel.GetSignals(PANE_ID_BREAKPOINTS).OnPanelRequestReload);
	
	MixConnect(this, &BreakpointsListCtrl::UpdateView, controller.GetSignals().OnRefreshProject);
	MixConnect(this, &BreakpointsListCtrl::OnRefreshSfcBreakpoints, controller.GetSignals().OnRefreshSfcBreakpoints);
}

//---- On Refresh Breakpoints -------------------------------------------------

void	BreakpointsListCtrl::OnRefreshSfcBreakpoints(ISourceFileClass *sfc, vector<int> bp_list)
{
	UpdateView();
}

void	BreakpointsListCtrl::ClearVariables(void)
{
	DeleteAllItems();
}

//---- Update Breakpoints -------------------------------------------------------

void	BreakpointsListCtrl::UpdateView(void)
{
	DeleteAllItems();
	
	const StringList	bp_string_list = m_Controller.GetSortedBreakpointList(1);
	
	for (int i = 0; i < bp_string_list.size(); i++)
	{
		const string	s = bp_string_list[i];
		
		const size_t	sep = s.find(":");
		assert(sep != string::npos);
		
		const string	fname = s.substr(0, sep);
		const string	ln_s = s.substr(sep + 1);
		
		// increase the list first with dummy
		InsertItem(i, " ");
		SetItemImage(i, -1);		// (no icon)
		
		SetItem(i, 0, wxString(fname));
		SetItem(i, 1, wxString(ln_s));
	}
	
	// set to auto-size AFTER filled the list (otherwise will be zero)
	SetColumnWidth(0, WX_LIST_AUTO_SZ);
	SetColumnWidth(1, WX_LIST_AUTO_SZ);
	
	Refresh();
}

//---- Delete Selected Breakpoints --------------------------------------------

void	BreakpointsListCtrl::DeleteSelectedBreakpoints(void)
{
	vector<int>	item_list;
	
	// phase 1: collect selected
	int	ind = -1;
	
	while ((ind = GetNextItem(ind, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1)
	{	
		item_list.push_back(ind);
	}
	
	// phase 2: delete back to front -- BULLSHIT ?
	while (item_list.size() > 0)
	{	
		ind = item_list.back();
		
		item_list.pop_back();
		
		// (gets breakpoint names from UI)
		wxListItem	info;
		
		// get src
		info.m_mask = wxLIST_MASK_TEXT;
		info.m_itemId = ind;
		info.m_col = 0;
		GetItem(info/*&*/);
		const wxString	src_str = info.GetText();
		
		// get line string
		info.m_col = 1;
		GetItem(info/*&*/);
		const wxString	ln_str = info.GetText();
		
		// convert to int
		const int	ln = Soft_stoi(ln_str.ToStdString(), -1);
		assert(ln != -1);
		
		// remove breakpoint
		m_Controller.SetSourceBreakpoint(src_str.ToStdString(), ln - 1, false);
	}
	
	// refresh this control
	UpdateView();
}

//---- On Key Down ------------------------------------------------------------

void	BreakpointsListCtrl::OnKeyDown(wxKeyEvent &event)
{
	// handle delete here
	switch (event.GetKeyCode())
	{
		case WXK_DELETE:
			
			DeleteSelectedBreakpoints();
			break;
			
		case 'A':
		
			if (event.ControlDown())	SelectAll();
			break;
	}
}

//---- Select All -------------------------------------------------------------

void	BreakpointsListCtrl::SelectAll(void)
{
	wxListItem	info;
	
	info.m_mask = wxLIST_MASK_STATE;
	info.m_state = wxLIST_STATE_SELECTED;
	info.m_stateMask = wxLIST_STATE_SELECTED;
	
	// NOTE: SetItemState() doesn't work
	
	for (int i = 0; i < GetItemCount(); i++)
	{	info.m_itemId = i;
		
		SetItem(info);
	}
}

//---- On Item Double-Click ---------------------------------------------------

void	BreakpointsListCtrl::OnItemDoubleClick(wxListEvent &e)
{
	const int	row = e.GetIndex();
	assert(row >= 0);
	
	const StringList	bp_list = m_Controller.GetSortedBreakpointList(1);
	
	const string	bp_s = bp_list.at(row);
	
	const Bookmark	bmk = Bookmark::Decode(bp_s);
		
	const string	src = bmk.m_Name;
	if (src == "C source file")
	{	wxMessageBox("Can't display C source file in Lua debugger");
		return;
	}
	
	m_Controller.ShowSourceAtLine(src, bmk.m_Line, true/*focus?*/);
}

//---- INSTANTIATE ------------------------------------------------------------

// static
IPaneMixIn*	IPaneMixIn::CreateBreakpointListCtrl(IBottomNotePanel &bottom_book, int id, Controller &controller)
{
	return new BreakpointsListCtrl(bottom_book, id, controller);
}

// nada mas