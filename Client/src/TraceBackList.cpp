// traceback list control

#include "wx/wx.h"
#include "wx/listctrl.h"

#include "BottomNotebook.h"

#include "TopFrame.h"
#include "Controller.h"
#include "SourceFileClass.h"

#include "logImp.h"

#include "ddt/CommonDaemon.h"

#include "lx/xstring.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

const wxColour	COLOR_LUA_LEVEL = wxColour(0x10, 0x10, 0x10);
const wxColour	COLOR_C_LEVEL = wxColour(0xa0, 0xa0, 0xa8);

class TraceBackListCtrl: public wxListCtrl, public PaneMixIn, private LX::SlotsMixin<LX::CTX_WX>
{
public:
	// ctor
	TraceBackListCtrl(IBottomNotePanel &parent, int id, TopFrame &tf, Controller &controller);
	// dtor
	virtual ~TraceBackListCtrl() = default;
	
private:
	
	// events
	void	ClearVariables(void);
	void	RequestVarReload(void);
	
	void	OnItemActivated(wxListEvent &e);
	
	TopFrame	&m_TopFrame;
	Controller	&m_Controller;
	
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(TraceBackListCtrl, wxListCtrl)

	EVT_LIST_ITEM_ACTIVATED(	-1,				TraceBackListCtrl::OnItemActivated)

END_EVENT_TABLE()

#define	style	(wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES)

//---- CTOR -------------------------------------------------------------------

	TraceBackListCtrl::TraceBackListCtrl(IBottomNotePanel &bottom_notebook, int id, TopFrame &tf, Controller &controller)
		: wxListCtrl(bottom_notebook.GetWxWindow(), id, wxDefaultPosition, wxDefaultSize, style),
		PaneMixIn(bottom_notebook, id, this),
		m_TopFrame(tf),
		m_Controller(controller)
{
	// assign images (list is managed by me)
	SetImageList(&controller.GetImageList(), wxIMAGE_LIST_SMALL/*which*/);
	
	InsertColumn(0, "source");
	InsertColumn(1, "line");
	InsertColumn(2, "function");
	InsertColumn(3, "alt name");
	InsertColumn(4, "args");
	InsertColumn(5, "upvalues");
	
	DeleteAllItems();
	
	MixConnect(this, &TraceBackListCtrl::ClearVariables, bottom_notebook.GetSignals(PANE_ID_STACK).OnPanelClearVariables);
	MixConnect(this, &TraceBackListCtrl::RequestVarReload, bottom_notebook.GetSignals(PANE_ID_STACK).OnPanelRequestReload);
}

void	TraceBackListCtrl::ClearVariables(void)
{
	if (m_TopFrame.GetClientStack())	m_TopFrame.GetClientStack()->Clear();
	
	DeleteAllItems();
}

//---- Request Reload ---------------------------------------------------------

void	TraceBackListCtrl::RequestVarReload(void)
{
	DeleteAllItems();
	
	// should check if is CONNECTED
	
	shared_ptr<ClientStack> client_stack = m_TopFrame.GetClientStack();
	if (!client_stack)	return;			// ignore EMPTY shared_ptr<>
	
	const int	current_level = m_TopFrame.GetVisibleStackLevel();
	
	for (int i = 0; i < client_stack->NumLevels(); i++)
	{	
		const ClientStack::Level &sl = client_stack->GetLevel(i);
	
		// increase the list first with dummy
		InsertItem(i, " ");
		
		const int	icon_id = (current_level == i) ? BITMAP_ID_ARROW_RIGHT : BITMAP_ID_EMPTY_PLACEHOLDER;
		
		SetItemImage(i, icon_id);
		
		SetItemTextColour(i, sl.IsC() ? COLOR_C_LEVEL : COLOR_LUA_LEVEL);
		
		if (sl.IsC())
		{	SetItem(i, 0, "C location");
			SetItem(i, 1, "n/a");
			
			if (sl.NumUpValues() > 0)
				SetItem(i, 5, wxString(xsprintf("%d", sl.NumUpValues())));
			else	SetItem(i, 5, "");
		}
		else
		{	SetItem(i, 0, sl.SourceFileName());
		
			if (sl.Line() != -1)
				SetItem(i, 1, wxString(to_string(sl.Line())));
			else	SetItem(i, 1, "n/a");
			
			const StringList	args {sl.FunctionArguments()};
			string	flat_args;
			
			for (const auto &it : args)
			{	flat_args.append(it);
				if (it != args.back())	flat_args.append(", ");
			}
			
			const string	name_s = sl.FunctionName();
			if (!name_s.empty())
			{	const string	fn_w_args = xsprintf("%s(%s)", name_s, flat_args);
				SetItem(i, 2, wxString(fn_w_args));
			}
			else	SetItem(i, 2, "?");
			
			// alternative function name in globals
			if (!sl.AltFunctionName().empty())
			{	const string	alt_fn_w_args = xsprintf("%s(%s)", sl.AltFunctionName(), flat_args);
				SetItem(i, 3, wxString(alt_fn_w_args));
			}
			
			if (sl.NumFixedArguments() > 0)
				SetItem(i, 4, wxString(to_string(sl.NumFixedArguments())));
			// else	SetItem(i, 4, "");
			
			if (sl.NumUpValues() > 0)
				SetItem(i, 5, wxString(to_string(sl.NumUpValues())));
			// else	SetItem(i, 5, "");
		}
	}
	
	// set to auto-size AFTER filled the list (otherwise will be zero)
	for (int i = 0; i < GetColumnCount(); i++)	SetColumnWidth(i, WX_LIST_AUTO_SZ);
}

//---- On Item Double-Click ---------------------------------------------------

void	TraceBackListCtrl::OnItemActivated(wxListEvent &e)
{
	const int	row = e.GetIndex();
	
	shared_ptr<ClientStack>	client_stack = m_TopFrame.GetClientStack();
	assert(client_stack);
	
	const int	stack_depth = row;
	assert(stack_depth < client_stack->NumLevels());
	
	// can SELECT C level...
	m_TopFrame.SetVisibleStackLevel(stack_depth);
	
	const ClientStack::Level	&lvl = client_stack->GetLevel(stack_depth);
	if (lvl.IsC())
	{	// ... but not DISPLAY it
		// e.Veto();
		return;	
	}
	
	const string	src = lvl.SourceFileName();
	
	m_Controller.ShowSourceAtLine(src, lvl.Line(), true/*focus?*/);
}

//---- INSTANTIATE ------------------------------------------------------------

// static
IPaneMixIn*	IPaneMixIn::CreateTraceBackListCtrl(IBottomNotePanel &bottom_book, int id, TopFrame &tf, Controller &controller)
{
	return new TraceBackListCtrl(bottom_book, id, tf, controller);
}

// nada mas