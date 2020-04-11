// project listCtrl

#include <vector>
#include <algorithm>

#include "wx/listctrl.h"

#include "logImp.h"
#include "TopFrame.h"
#include "SourceFileClass.h"
#include "Controller.h"

#include "BottomNotebook.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

static const
vector<SFC_PROP>	s_PropList = 
{
	SFC_PROP::SHORT_NAME,
	SFC_PROP::FILE_SIZE,
	SFC_PROP::MODIFY_DATE,
	SFC_PROP::SOURCE_LINES,
};

// #define	style	(wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES | wxLC_SORT_ASCENDING | wxLC_SORT_DESCENDING)	// built-in sorting does NOT work
#define	style	(wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES)

class ProjectList: public wxListCtrl
{
public:
	// ctor
	ProjectList(wxWindow *parent_win, Controller &controller)
		: wxListCtrl(parent_win, -1, wxDefaultPosition, wxDefaultSize, style),
		m_Controller(controller)
	{
		SetImageList(&controller.GetImageList(), wxIMAGE_LIST_SMALL);
		
		// (uses COW)
		wxFont	ft(wxFontInfo(9).Family(wxFONTFAMILY_MODERN).Encoding(wxFONTENCODING_DEFAULT));
		
		SetFont(ft);
	}
	
	ISourceFileClass*	GetNthSFC(const int row) const
	{
		const vector<ISourceFileClass*>	&sfc_instances  = m_Controller.GetSFCInstances();
		assert(row < sfc_instances.size());
		
		ISourceFileClass	*sfc = sfc_instances.at(row);
		assert(sfc);
		
		return sfc;
	}
	
	void	Redraw(void)
	{
		uLog(UI, "ProjectList::Redraw()");
		
		wxListCtrl::ClearAll();		// (DeleteAllItems() doesn't delete columns)
		
		InsertColumn(SFC_PROP::SHORT_NAME, "name", wxLIST_FORMAT_LEFT);
		InsertColumn(SFC_PROP::FILE_SIZE, "size", wxLIST_FORMAT_RIGHT);
		InsertColumn(SFC_PROP::MODIFY_DATE, "modified", wxLIST_FORMAT_LEFT);
		InsertColumn(SFC_PROP::SOURCE_LINES, "lines", wxLIST_FORMAT_RIGHT);
		
		const vector<ISourceFileClass*>	&sfc_list = m_Controller.GetSFCInstances();
		
		for (int row = 0; row < sfc_list.size(); row++)
		{
			const ISourceFileClass	*sfc = sfc_list[row];
			assert(sfc);
			
			const SFC_props		props = sfc->GetProperties();
			
			InsertItem(row, " ");
			
			for (int col = 0; col < GetColumnCount(); col++)
			{
				const SFC_PROP	prop_id = s_PropList[col];
				assert(props.count(prop_id));
				SetItem(row, col, wxString{props.at(prop_id)});
			}
		}
		
		for (int col = 0; col < GetColumnCount(); col++)
		{
			bool	ok = SetColumnWidth(col, WX_LIST_AUTO_SZ);
			assert(ok);
		}
	}

	void	OnKeyDown(wxKeyEvent &e)
	{
		// get 1st selected item
		const int	row = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (row == -1)		return;
		
		ISourceFileClass	*sfc = GetNthSFC(row);
		assert(sfc);
		
		switch (e.GetKeyCode())
		{
			case WXK_DELETE:
			{	// deleted SFC -- will NOT trigger a UI refresh (as it should)
				uLog(LOGIC, "ProjectList::OnKeyDown(delete)");
				bool	ok = sfc->DestroySFC();
				assert(ok);
				
				CallAfter(&ProjectList::Redraw);			// hack for now	- FIXME
			}	break;
			
			default:
			
				// let handle up/down, etc.
				e.Skip();
				break;
		}
	}
	
	void	OnRightClick(wxListEvent &e)
	{
		const int	row = e.GetIndex();
	
		ISourceFileClass	*sfc = GetNthSFC(row);
		assert(sfc);
		
		wxMenu	menu;
		
		menu.Append(POPUP_MENU_ID_SET_STARTUP_SCRIPT, "Set As Startup Script");
		
		const int	menu_id = GetPopupMenuSelectionFromUser(menu);
		if (menu_id < 0)	return;		// canceled
		
		switch (menu_id)
		{
			case POPUP_MENU_ID_SET_STARTUP_SCRIPT:
			{
				const string	s = sfc->GetShortName();
				
				uMsg("startup script %S", s);
				
			}	break;
			
			default:
			
				break;
		}
	}

	void	OnItemActivated(wxListEvent &e)
	{
		const int	row = e.GetIndex();
		
		ISourceFileClass	*sfc = GetNthSFC(row);
		assert(sfc);
		
		// show source
		sfc->ShowNotebookPage(true);
	}
	
	void	OnSizeEvent(wxSizeEvent &e)
	{
		CallAfter(&ProjectList::Redraw);
		
		e.Skip();
	}
	
private:
	
	Controller	&m_Controller;
	
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ProjectList, wxListCtrl)

	EVT_LIST_ITEM_ACTIVATED(	-1,		ProjectList::OnItemActivated)
	EVT_KEY_DOWN(					ProjectList::OnKeyDown)
	EVT_SIZE(					ProjectList::OnSizeEvent)
	EVT_LIST_ITEM_RIGHT_CLICK(-1,			ProjectList::OnRightClick)		// 
	
END_EVENT_TABLE()

//---- Project List Control ---------------------------------------------------

class ProjectListCtrl : public wxPanel, public PaneMixIn, private SlotsMixin<CTX_WX>
{
public:
	ProjectListCtrl(IBottomNotePanel &parent, int id, Controller &controller)
		: wxPanel(parent.GetWxWindow(), id, wxDefaultPosition, wxSize(0, 0)),
		PaneMixIn(parent, id, this),
		SlotsMixin(CTX_WX, "ProjectListCtrl"),
		m_ListCtrl(this, controller)
	{
		wxBoxSizer	*top_sizer = new wxBoxSizer(wxVERTICAL);
		
		top_sizer->Add(&m_ListCtrl, wxSizerFlags(1).Expand());
		
		SetSizer(top_sizer);
		
		MixConnect(this, &ProjectListCtrl::OnRefreshBroadcast, controller.GetSignals().OnRefreshProject);
	}
	
	virtual ~ProjectListCtrl()
	{
		uLog(DTOR, "ProjectListCtrl::DTOR()");
	}
	
private:
	
	void	OnRefreshBroadcast(void)
	{
		uLog(UI, "ProjectListCtrl::OnRefreshBroadcast()");
	
		m_ListCtrl.Redraw();
	}
	
	ProjectList	m_ListCtrl;
};

//---- INSTANTIATE ------------------------------------------------------------

// static
IPaneMixIn*	IPaneMixIn::CreateProjectListCtrl(IBottomNotePanel &parent, int id, Controller &controller)
{
	return new ProjectListCtrl(parent, id, controller);
}

// nada mas
