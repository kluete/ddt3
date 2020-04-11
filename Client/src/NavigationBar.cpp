// navigation icon bar

#include <cassert>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "wx/menu.h"
#include "wx/bitmap.h"

#include "remoter/star.c_raw"
#include "remoter/delete.c_raw"
#include "remoter/hidden.c_raw"

#include "ClientCore.h"

#include "UIConst.h"

#include "Controller.h"
#include "TopFrame.h"

#include "NavigationBar.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

#define	expand_craw(nm)	nm##craw.m_Type, nm##craw.m_Len, (const uint8_t *)nm##craw.m_Data

class craw_class
{
public:
	// ctors
	craw_class()
		: m_Len(0)
	{
		
	}
	craw_class(wxBitmapType t, size_t len, const uint8_t *data)
		: m_Type(t), m_Len(len)
	{
		m_Data = data;
	}
	
	// int		m_Width, m_Height, m_Depth;
	wxBitmapType	m_Type;
	size_t		m_Len;
	const uint8_t	*m_Data;
};

#define	ENTRY(a, b, c, d)	make_tuple(a, craw_class(expand_craw(b)), c, d)				// initializer list won't work otherwise
#define	ENTRY_SEPARATOR		make_tuple(NAV_ID_SEPARATOR, craw_class(), "", wxITEM_NORMAL)

static const
vector<tuple<int, craw_class, string, wxItemKind>>	s_NavIDtoNames =
{	
	ENTRY(NAV_ID_STAR,	star_,		"Bookmark",	wxITEM_DROPDOWN),
 	ENTRY_SEPARATOR,
	ENTRY(NAV_ID_DELETE,	delete_,	"Delete",	wxITEM_NORMAL),
	ENTRY(NAV_ID_HIDDEN,	hidden_,	"Hidden",	wxITEM_CHECK)
};

BEGIN_EVENT_TABLE(NavigationBar, wxToolBar)

	EVT_MENU(		LOCAL_PATH_MRU_CLEAR,						NavigationBar::OnClearMRUMenuEvent)
	
	// forwarded events
	EVT_COMMAND_RANGE(	NAV_ID_FIRST,		NAV_ID_LAST,		wxEVT_TOOL,	NavigationBar::OnToolEventForward)
	EVT_COMMAND_RANGE(	LOCAL_PATH_MRU_START,	LOCAL_PATH_MRU_END,	wxEVT_MENU,	NavigationBar::OnToolEventForward)
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

#define	nav_styl	(wxHORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER)	// | wxTB_TEXT)

	NavigationBar::NavigationBar(wxWindow *parent, int id, TopFrame *tf, Controller *controller, wxEvtHandler *evt_hdl)
		: wxToolBar(parent, id, wxDefaultPosition, wxDefaultSize, nav_styl)
{
	assert(parent);
	assert(tf);
	assert(controller);
	assert(evt_hdl);
	
	m_TopFrame = tf;
	m_Controller = controller;
	m_EvtRecipient = evt_hdl;
	m_NavIDtoBitmapLUT.clear();
	
	SetMargins(2, 2);
	SetToolBitmapSize(wxSize(NAVIGATION_ICON_SIZE, NAVIGATION_ICON_SIZE));
	
	for (auto it : s_NavIDtoNames)
	{
		const int	tool_id = get<0>(it);
		if (tool_id == NAV_ID_SEPARATOR)
		{
			AddSeparator();
			continue;
		}
		
		craw_class	ccraw = get<1>(it);
		const wxString	tool_name = wxString(get<2>(it));
		wxItemKind	kind = get<3>(it);
		
		wxBitmap	bm = tf->LoadCraw(ccraw.m_Data, ccraw.m_Len, ccraw.m_Type);	// (img);
		assert(bm.IsOk());
		
		// store tool -> bitmap lut
		m_NavIDtoBitmapLUT[tool_id] = bm;
		
		// add tool
		AddTool(tool_id, tool_name, bm, tool_name, kind);
	}
	
	// bind to ALL wxCommandEvents (tool, menu, etc.)
	// Bind(wxEVT_TOOL, &NavigationBar::OnToolEvent, this);
	
	SetToolPacking(0);
	SetToolSeparation(1);

	// must call to reflect the changes
	Realize();
}

//---- On Clear MRU menu event ------------------------------------------------

void	NavigationBar::OnClearMRUMenuEvent(wxCommandEvent &e)
{
	// uLog(UI, "NavigationBar::OnClearMRUMenuEvent()");
	assert(m_Controller);
	
	m_Controller->ClearLocalPathBookmarks();
	
	RequestUpdateBookmarks();
}

//---- Request Update Bookmarks -----------------------------------------------

void	NavigationBar::RequestUpdateBookmarks(void)
{
	CallAfter(&NavigationBar::DoUpdateMRUList);
}

//---- Do Update MRU List -----------------------------------------------------

void	NavigationBar::DoUpdateMRUList(void)
{
	assert(m_Controller);
	
	const vector<string>	mru_list = m_Controller->GetLocalPathBookmarkList();
	
	// rebuild menu
	wxMenu	*mru_menu = new wxMenu();
	
	for (int i = 0; i < mru_list.size(); i++)
	{
		const wxString	title{mru_list[i]};
		
		mru_menu->Append(LOCAL_PATH_MRU_START + i, title);
	}
	
	mru_menu->AppendSeparator();
	mru_menu->Append(LOCAL_PATH_MRU_CLEAR, "Clear");
	
	SetDropdownMenu(NAV_ID_STAR, mru_menu);
}

//---- On Tool Event forward --------------------------------------------------

void	NavigationBar::OnToolEventForward(wxCommandEvent &e)
{
	assert(m_EvtRecipient);
	
	// m_EvtRecipient->ProcessEvent(e);
	wxQueueEvent(m_EvtRecipient, e.Clone());
	
	// e.Skip();
}

// nada mas
