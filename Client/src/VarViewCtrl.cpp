// Lua variable view control

#include <cassert>
#include <vector>
#include <unordered_map>

#include "wx/listctrl.h"
#include "wx/notebook.h"
#include "wx/tglbtn.h"

#include "logImp.h"
#include "BottomNotebook.h"

#include "TopFrame.h"
#include "Controller.h"

#include "WatchBag.h"

#include "ddt/sharedDefs.h"
#include "ddt/MessageQueue.h"

#include "lx/stream/MemoryInputStream.h"
#include "lx/stream/MemoryOutputStream.h"
#include "lx/stream/DataInputStream.h"
#include "lx/stream/DataOutputStream.h"

#include "lx/xstring.h"

#include "lx/ScrollableEventImp.h"

#include "lx/gridevent.h"
#include "lx/GridImp.h"

#include "lx/renderer.h"
#include "lx/context.h"
#include "lx/ModalTextEdit.h"
#include "lx/celldata.h"

#include "VarViewCtrl.h"

const int	DDT_TABLE_INDENT = 2;

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

const Color	COLOR_ALT_TABLE_INDENT	(0xfa, 0xfa, 0xff, 0xff);	// light blue (hard to see on some LCDs)
const Color	COLOR_TWIN_CELL		(243, 240, 90, 0x20);		// yellow

static const
vector<COLUMN_T>	s_ColList {COLUMN_T::KEY, COLUMN_T::VAL_STR, COLUMN_T::VAL_TYPE, COLUMN_T::SCOPE, COLUMN_T::VAL_PATH};

struct view_filter_info
{
	wxString	m_ToolTip;
	wxString	m_Label;
};

static const
unordered_map<int, view_filter_info>	s_VarNameLUT
{
	{VV_NUMBER,		{"number",			"#"}},
	{VV_BOOLEAN,		{"boolean",			"b"}},
	{VV_STRING,		{"string",			"\""}},
	{VV_LUA_FUNCTION,	{"Lua function",		"()"}},
	{VV_C_FUNCTION,		{"C function",			"c()"}},
	{VV_TABLE,		{"Table",			"[]"}},
	{VV_TEMPORARY,		{"temporary",			"tmp"}},
	{VV_THREAD,		{"thread",			"th"}},
	{VV_USERDATA,		{"userdata",			"ud"}},
	{VV_BUILT_IN,		{"built-in",			"."}},
	{VV_REGISTRY,		{"Registry",			"R"}},
	{VV_METATABLE,		{"metatable",			"_[]"}},
	{VV_FFI_CDATA,		{"ffi cdata",			"ffi"}},
};

static const
vector<int>	s_VarLayout =
{
	// common Lua types
	VV_BOOLEAN, VV_NUMBER, VV_STRING, VV_TABLE, VV_LUA_FUNCTION, VV_THREAD, VV_METATABLE,
	VV_SEPARATOR,

	// lower-level / needs C++
	VV_USERDATA, VV_C_FUNCTION, VV_TEMPORARY, VV_BUILT_IN, VV_REGISTRY, VV_FFI_CDATA,
	VV_STRETCH_SEPARATOR
};

//---- VarListCtrl class ------------------------------------------------------

class VarListCtrl: public ScrollableEventImp
{
public:
	// ctor
	VarListCtrl(wxWindow *parent_win, const int id, Renderer &cairo_renderer, ModalTextEdit *edit_dlg, GridClient *grid_client)
		: ScrollableEventImp(parent_win, id, cairo_renderer, edit_dlg, ""),
		m_GridImp(this, grid_client, "")
	{
	}
	
	Grid&		GetGrid(void)		{return m_GridImp;}
	GridImp&	GetGridImp(void)	{return m_GridImp;}
	
private:
	
	GridImp	m_GridImp;
};

//---- Variable View Control Class --------------------------------------------

class VarViewCtrl: public wxPanel, public PaneMixIn, public GridClient, public IVarViewCtrl, private SlotsMixin<CTX_WX>
{
public:
	VarViewCtrl(IBottomNotePanel &bottom_notebook, int id, TopFrame &tf, Controller &controller, IVarViewFlags &vwf, const bool showscope_f, WatchBag *watch_ptr);
	virtual ~VarViewCtrl() = default;
	
	// GridClient IMP
	void	OnGridRealized(GridImp &grid) override;
	bool	IsGridShown(GridImp &grid) override;
	Color	GetGridRowColor(GridImp &grid, const size_t row) override;
	void	OnGridEvent(GridImp &grid, GridEvent &e) override;
	bool	IsGridSelectable(GridImp &grid) const override;
	
private:

	void	UpdateVarCheckboxes(const uint32_t flags);
	void	SetGridOverlays(const bool f);
	void	ResetColumns(const uint32_t flags);
	
	void	OnPostedCollectedMessage(Message msg)
	{
		MemoryInputStream	mis(msg.GetBuff());
		
		DataInputStream		dis(mis);
		
		DAEMON_MSG	msg_t;
		uint32_t	requester_id;
	
		dis >> msg_t >> requester_id;
	
		SetVariables(dis);
	}
	
	void	SetVariables(DataInputStream &mis);
	
	void	DoReload(void);
	
	// wired signals
	void	ClearVariables(void);
	void	RequestVarReload(void);
	
	bool	IsEditable(void) const;
	void	UpdateOneButton(const int index);
	void	AddOneCollected(const int row, const Collected &ce, const uint32_t mask);
	bool	ToCellData(const int row, const int col, const Collected &ce, const uint32_t mask, CellData &cdata);
	
	void	HighlightTwinCells(const string &base_s);
	int	GetFontStyle(const int id) const;
	
	// (internally-dispatched)
	void	OnListItemClicked(GridEvent &e);
	void	OnListItemRightClick(GridEvent &e);
	void	OnListItemActivated(GridEvent &e);
	void	OnListItemDelete(GridEvent &e);
	
	// button events
	void	OnVarToggleButton(wxCommandEvent &e);
	void	OnAutoButtonToggled(wxCommandEvent &e);
	
	TopFrame			&m_TopFrame;
	Controller			&m_Controller;
	const varview_info		m_Info;
	IVarViewFlags			&m_VarViewFlags;
	VarListCtrl			m_ListCtrl;
	Grid				&m_Grid;
	Renderer			&m_CairoRenderer;	// (shared)
	const bool			m_ShowScopeFlag;
	
	WatchBag			*m_WatchBag;
	
	wxToggleButton			*m_Buttons[VV_LAST_UI];
	wxToggleButton			*m_AutoButton;
	
	vector<Collected>		m_CollectedList;
	
	int				m_UsedCnt;		// (excluding dummy watch)
	HitItem				m_LastHoverItem;
	
	// (keep in UI side)
	StringSet			m_ExpandedTableSet;
	unordered_map<string, string>	m_VarPathToLastValMap;
	
	bool				m_RealizedFlag;
	
	vector<CellData>		m_CellDataList;
	
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(VarViewCtrl, wxPanel)

	EVT_COMMAND_RANGE(		TOGGLE_ID_FIRST, TOGGLE_ID_LAST,	wxEVT_TOGGLEBUTTON,	VarViewCtrl::OnVarToggleButton)
	EVT_TOGGLEBUTTON(		BUTTON_ID_AUTO_SORT_VARS,					VarViewCtrl::OnAutoButtonToggled)
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

	VarViewCtrl::VarViewCtrl(IBottomNotePanel &bottom_panel, int win_id, TopFrame &tf, Controller &controller, IVarViewFlags &vwf, const bool show_scope_f, WatchBag *w_bag)
		: wxPanel(bottom_panel.GetWxWindow(), win_id),
		PaneMixIn(bottom_panel, win_id, this),
		m_TopFrame(tf),
		m_Controller(controller),
		m_Info(IVarViewCtrl::GetVarViewInfo(IBottomNotePanel::WinToPaneID(win_id))),
		m_VarViewFlags(vwf),
		m_ListCtrl(this, LISTCTRL_ID_VAR_VIEW, controller.GetCairoRenderer(), controller.GetModalTextEdit(), this/*grid client*/),			// list control (editable or not)
		m_Grid(m_ListCtrl.GetGrid()),
		m_CairoRenderer(controller.GetCairoRenderer()),
		m_ShowScopeFlag(show_scope_f)
{
	static_assert((TOGGLE_ID_LAST - TOGGLE_ID_FIRST) == (VV_LAST_UI - VV_FIRST), "VarView MENU_ID vs VV_xxx discrepancy");
	
	m_WatchBag = w_bag;			// may be nil
	m_AutoButton = nil;
	m_RealizedFlag = false;
	
	wxFont	ft(wxFontInfo(9).Family(wxFONTFAMILY_MODERN).Encoding(wxFONTENCODING_DEFAULT));
	SetFont(ft);
	
// var flags
	wxBoxSizer *check_sizer = new wxBoxSizer(wxHORIZONTAL);
	
	for (const auto &it : s_VarLayout)
	{	
		if (it == VV_STRETCH_SEPARATOR)
		{	check_sizer->AddStretchSpacer();
			continue;
		}
		else if (it == VV_SEPARATOR)
		{
			check_sizer->AddSpacer(32);
			continue;
		}
		
		const int	i = it;
		assert(i < VV_LAST_UI);
		assert(s_VarNameLUT.count(i) > 0);
		
		const view_filter_info	&info = s_VarNameLUT.at(i);
		const int	id = TOGGLE_ID_FIRST + i;
		
		wxToggleButton	*button = new wxToggleButton(this, id, info.m_Label, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
		button->SetToolTip(info.m_ToolTip);
		
		// (prevent focus outline)
		button->SetCanFocus(false);
		
		check_sizer->Add(button, wxSizerFlags(0).Border(wxALL, 2).Align(wxALIGN_CENTER_VERTICAL));
		
		m_Buttons[i] = button;
	}
	
	// UpdateVarCheckboxes();
	
	m_AutoButton = new wxToggleButton(this, BUTTON_ID_AUTO_SORT_VARS, "auto", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	m_AutoButton->SetToolTip("auto sort new/changed vars");
	m_AutoButton->SetCanFocus(false);
	m_AutoButton->SetValue(false);
	
	// check_sizer->AddStretchSpacer();
	check_sizer->Add(m_AutoButton, wxSizerFlags(0).Border(wxALL, 2).Align(wxALIGN_CENTER_VERTICAL));
	
	m_Grid.SetHeaderFontID(VAR_UI_COLUMN_HEADER);
	m_Grid.SetHeaderHeight(26);
	m_Grid.SetRowHeight(-2);
	
	const uint32_t	flags = (IsEditable() ? VAR_FLAG_EDITABLE : 0) | (show_scope_f ? VAR_FLAG_SHOW_SCOPE : 0);
	
	m_Grid.AddColumn("key");
	m_Grid.AddColumn("value");
	m_Grid.AddColumn("type");
	m_Grid.AddColumn((flags & VAR_FLAG_SHOW_SCOPE) ? "scope" : "");

	if (flags & VAR_FLAG_SHOW_PATH)		m_Grid.AddColumn("path");
	
	wxBoxSizer *v_sizer = new wxBoxSizer(wxVERTICAL);

	v_sizer->Add(check_sizer, 0, wxEXPAND | wxALL, 4);
	v_sizer->Add(&m_ListCtrl, 1, wxEXPAND | wxALL, 4);
	
	// hide var mask for editable/watches?
	// if (IsEditable())		v_sizer->Hide(check_sizer, true/*recurse*/);
	
	SetSizer(v_sizer);
	
	m_Grid.ApplyCellData();
	
	auto	&varview_ui_sig = tf.GetSignals(PANE_ID_ALL_VAR_VIEWS);
	MixConnect(this, &VarViewCtrl::UpdateVarCheckboxes, varview_ui_sig.OnVarCheckboxes);
	MixConnect(this, &VarViewCtrl::SetGridOverlays, varview_ui_sig.OnGridOverlays);
	MixConnect(this, &VarViewCtrl::ResetColumns, varview_ui_sig.OnVarColumns);
	
	const int	requester_id = m_Info.m_ClientID;
	
	MixConnect(this, &VarViewCtrl::OnPostedCollectedMessage, tf.GetSignals(requester_id).OnCollectedMessage);
	
	MixConnect(this, &VarViewCtrl::ClearVariables, bottom_panel.GetSignals(m_Info.m_PaneID).OnPanelClearVariables);
	MixConnect(this, &VarViewCtrl::RequestVarReload, bottom_panel.GetSignals(m_Info.m_PaneID).OnPanelRequestReload);
}

//---- Is Editable ? ----------------------------------------------------------

bool	VarViewCtrl::IsEditable(void) const
{
	return (m_WatchBag != nil);
}

//---- Update Var Flags -------------------------------------------------------

void	VarViewCtrl::UpdateVarCheckboxes(const uint32_t flags)
{
	uLog(UI, "VarViewCtrl<%S>::UpdateVarCheckboxes(flags = 0x%08x)", m_Info.m_Label, flags);
	
	for (int i = 0; i < VV_LAST_UI; i++)	UpdateOneButton(i);
	
	Refresh();
}

//---- Update One Button ------------------------------------------------------

void	VarViewCtrl::UpdateOneButton(const int index)
{
	assert((index >= 0) && (index < VV_LAST_UI));
	
	wxFont	normal(wxFontInfo(9).Family(wxFONTFAMILY_SWISS).Encoding(wxFONTENCODING_DEFAULT).Light());
	wxFont	bold(wxFontInfo(10).Family(wxFONTFAMILY_SWISS).Encoding(wxFONTENCODING_DEFAULT).Bold());
	
	const bool	f = m_VarViewFlags.GetFlag(index);
	
	wxToggleButton	*button = m_Buttons[index];
	assert(button);
		
	button->SetFont(f ? bold : normal);
	
	button->SetValue(f);
	
	// re-apply label or won't update correctly (?)
	const view_filter_info	&info = s_VarNameLUT.at(index);
	button->SetLabel(info.m_Label);
	
	button->Refresh();
}

//---- On pressed Var Toggle Button event -------------------------------------

void	VarViewCtrl::OnVarToggleButton(wxCommandEvent &e)
{
	const int	id = e.GetId();
	const int	index = id - TOGGLE_ID_FIRST;
	assert((index >= 0) && (index < VV_LAST_UI));
	
	uLog(UI, "VarViewCtrl<%S>::OnVarToggleButton(%d)", m_Info.m_Label, index);
	
	m_VarViewFlags.SetFlag(index, !m_VarViewFlags.GetFlag(index));
	
	// refresh IF CONNECTED
	if (m_Controller.IsLuaListening())	DoReload();
}

//---- On Auto Button Toggled event -------------------------------------------

void	VarViewCtrl::OnAutoButtonToggled(wxCommandEvent &e)
{
	assert(m_AutoButton);
	
	e.Skip();
	
	/*
	wxFont	normal(wxFontInfo(9).Family(wxFONTFAMILY_SWISS).Encoding(wxFONTENCODING_DEFAULT).Light());
	wxFont	bold(wxFontInfo(10).Family(wxFONTFAMILY_SWISS).Encoding(wxFONTENCODING_DEFAULT).Bold());
	
	const bool	f = e.IsChecked();
		
	m_AutoButton->SetFont(f ? bold : normal);
	
	// save prefs
	m_TopFrame.DirtyProjectPrefs();
	*/
	
	if (m_Controller.IsLuaListening())	DoReload();
}

//---- Set Grid Overlays on/off -----------------------------------------------

void	VarViewCtrl::SetGridOverlays(const bool f)
{
	m_Grid.SetCellOverlayFlag(f);
	
	m_ListCtrl.GetGridImp().OnImmediateRefresh();
}

//---- Add One Collected variable ---------------------------------------------
		
void	VarViewCtrl::AddOneCollected(const int row, const Collected &ce, const uint32_t mask)
{
	auto	&grid = m_ListCtrl.GetGrid();
	
	const int	n_cols = grid.GetNumCols();
	
	for (int col = (int)COLUMN_T::KEY; col < n_cols; col++)
	{	
		CellData	cdata(row, col);
		
		const bool	ok = ToCellData(row, col, ce, mask, cdata/*&*/);
		if (ok)		m_CellDataList.push_back(cdata);
	}
}

//---- Collected to CellData --------------------------------------------------
	
bool	VarViewCtrl::ToCellData(const int row, const int col, const Collected &ce, const uint32_t mask, CellData &cdata)
{
	cdata.SetRow(row).SetCol(col).SetText("").SetFont(VAR_UI_DEFAULT).SetIcon(-1/*no icon*/);
	
	assert(col < s_ColList.size());
	const COLUMN_T	col_id = s_ColList[col];
	
	switch (col_id)
	{
		case COLUMN_T::KEY:
		{
			// indented, decorated key
			const string	indented_key_s = string(ce.GetIndent() * DDT_TABLE_INDENT, ' ') + ce.GetDecoratedKey();
			cdata.SetText(indented_key_s);
			
			if (ce.IsTable())	cdata.SetIcon(ce.IsExpanded() ? BITMAP_ID_MINUS : BITMAP_ID_PLUS);
		}	break;
		
		case COLUMN_T::VAL_STR:
		{
			int	ui_id;
			
			if (ce.IsLuaFileFunction())
				ui_id = VAR_UI_EDITOR_URL;		// Lua source url
			else if (ce.IsMemoryURL())
				ui_id = VAR_UI_MEMORY_URL;		// memory url
			else	ui_id = (mask & VAR_FLAG_CHANGED) ? VAR_UI_CHANGED_VAL : VAR_UI_DEFAULT;	// changed (or not)
			
			cdata.SetText(ce.GetDecoratedVal()).SetFont(ui_id);
		}	break;
		
		case COLUMN_T::VAL_TYPE:
		{
			const bool	metatab_f = ce.HasMetatable();
			
			if (!metatab_f)
				cdata.SetText(Collected::GetTypeS(ce.GetType()));
			else	cdata.SetText(Collected::GetTypeS(ce.GetType()) + " MT");
			
			// underline if metatable
			cdata.SetFont(metatab_f ? VAR_UI_METATABLE_URL : VAR_UI_DEFAULT);
		}	break;
		
		case COLUMN_T::SCOPE:
		
			if (!(mask & VAR_FLAG_SHOW_SCOPE))	return false;		// should HIDE column?
			cdata.SetText(ce.GetScopeStr());
			break;
			
		case COLUMN_T::VAL_PATH:
		
			// if has this column, always show
			// if (!(mask & VAR_FLAG_SHOW_PATH))	return false;
			cdata.SetText(xsprintf("[%d] %s", row, ce.GetPath()));
			break;
			
		default:
		
			assert(false);
			break;
	}
	
	return true;
}

//---- Request Var Reload -----------------------------------------------------

void	VarViewCtrl::RequestVarReload(void)
{
	if (!m_Controller.IsLuaListening())		return;		// should show BLANK panel
	
	// uWarn("VarViewCtrl::RequestVarReload(%S)", m_Info.m_Label);
	
	const CLIENT_MSG	msg_t = m_Info.m_RequestMsg;
	const uint32_t		client_id = m_Info.m_ClientID;
	
	const int32_t		client_stack_level = m_TopFrame.GetVisibleStackLevel();
	
	const uint32_t		var_flags = m_VarViewFlags.GetMask();
	
	VariableRequest	req(client_id, client_stack_level, m_ExpandedTableSet.ToStringList(), var_flags);
	
	if (m_Info.m_PaneID == PANE_ID_WATCHES)
		req.SetWatchNames(m_TopFrame.GetWatchBag().Get());
	
	MemoryOutputStream	mos;
	DataOutputStream	dos(mos);
	
	dos << msg_t << req;
	
	m_TopFrame.SendRequestReply(mos);
}

//---- Do Reload --------------------------------------------------------------

void	VarViewCtrl::DoReload(void)
{
	if (!m_Controller.IsLuaListening())
	{
		uErr("VarViewCtrl::DoReload() aborted, Lua not listening");
		return;
	}
	
	auto	&grid = m_ListCtrl.GetGrid();
	
	grid.ApplyCellData();
	
	// saves prefs & refreshes
	CallAfter(&VarViewCtrl::RequestVarReload);
}

//---- Reset Columns ----------------------------------------------------------

void	VarViewCtrl::ResetColumns(const uint32_t flags0)
{
	const uint32_t	flags = flags0 | (m_ShowScopeFlag ? VAR_FLAG_SHOW_SCOPE : 0);
	
	auto	&grid = m_ListCtrl.GetGrid();
	
	grid.DeleteAllColumns();
	grid.AddColumn("key");
	grid.AddColumn("value");
	grid.AddColumn("type");
	grid.AddColumn((flags & VAR_FLAG_SHOW_SCOPE) ? "scope" : "");

	if (flags & VAR_FLAG_SHOW_PATH)		grid.AddColumn("path");
	
	DoReload();
}

//---- Clear Variables --------------------------------------------------------

void	VarViewCtrl::ClearVariables(void)
{
	m_CollectedList.clear();
	m_VarPathToLastValMap.clear();
	m_ExpandedTableSet.clear();
	m_UsedCnt = 0;

	auto	&gridImp = m_ListCtrl.GetGrid();
	
	gridImp.DeleteAllCells();
	
	gridImp.FlushScroll();		// ApplyCellData();
	
	/*
	if (IsEditable())
	{	// add empty row to the end so can edit (watch view)
		grid.SetCellData(m_UsedCnt, 0, "", VAR_UI_DEFAULT);
	}
	*/
}

//---- Set Variables DIRECTLY by deserializing from daemon --------------------

void	VarViewCtrl::SetVariables(DataInputStream &mis)
{
	assert(m_AutoButton);
	
	m_LastHoverItem.clear();
	
	const bool	auto_sort_f = (m_AutoButton->GetValue() != 0);
	
	CollectedList	new_collected(mis);
	const size_t	new_cnt = new_collected.size();
	
	// store UI-side logic LOCALLY
	unordered_set<int>	new_vars, changed_vars, unchanged_vars;
	unordered_set<string>	changed_var_path_set;
	
	// detect changed variables
	for (int i = 0; i < new_collected.size(); i++)
	{
		const Collected	&ce = new_collected[i];
		
		if (ce.IsSolved())
		{	// find same var path, with different value
			const auto	it = m_VarPathToLastValMap.find(ce.GetPath());
			if ((it == m_VarPathToLastValMap.end()) || ce.IsTemporary())
			{	new_vars.insert(ce.GetRootIndex(i));			// new var or temporary (prio 1)
			}
			else
			{	if (ce.m_ValS != it->second)
				{	changed_var_path_set.insert(ce.GetPath());
					changed_vars.insert(ce.GetRootIndex(i));	// changed var (prio 2)
				}
				else
				{	// unchanged vars (prio 3)
					unchanged_vars.insert(ce.GetRootIndex(i));
				}
			}
		}
		else
		{	// unsolved are always last
			
		}
	}
	
	// build auto LUT
	vector<int>	new_list, changed_list, unchanged_list, unsolved_list;
	
	// add new vars
	for (int i = 0; i < new_cnt; i++)
	{	const int	root_index = new_collected[i].GetRootIndex(i);
		if (new_vars.count(root_index) > 0)
			new_list.push_back(i);
		else if (changed_vars.count(root_index) > 0)
			changed_list.push_back(i);
		else if (unchanged_vars.count(root_index) > 0)
			unchanged_list.push_back(i);			// (unchanged keep same order)
		else	unsolved_list.push_back(i);
	}
	
	vector<int>	autoLUT;
	
	autoLUT.insert(autoLUT.end(), new_list.begin(), new_list.end());
	autoLUT.insert(autoLUT.end(), changed_list.begin(), changed_list.end());
	autoLUT.insert(autoLUT.end(), unchanged_list.begin(), unchanged_list.end());
	autoLUT.insert(autoLUT.end(), unsolved_list.begin(), unsolved_list.end());
	
	assert(autoLUT.size() == new_cnt);
	
	m_CollectedList.clear();
	
	// write (possibly) sorted collected
	for (int k = 0; k < new_cnt; k++)
	{
		const int	i = auto_sort_f ? autoLUT[k] : k;
	
		m_CollectedList.push_back(new_collected[i]);
	}
	
	m_UsedCnt = new_cnt;
	
	// clear hashtab & refill
	m_VarPathToLastValMap.clear();
	
	m_CellDataList.clear();
	
	// rebuild expanded hashset
	m_ExpandedTableSet.clear();
	
	// unroll to listCtrl
	for (int i = 0; i < m_CollectedList.size(); i++)
	{
		const Collected	&ce = m_CollectedList[i];
		const wxString	path = ce.GetPath();
		
		m_VarPathToLastValMap[path] = ce.m_ValS;
		
		uint32_t	mask = 0;
		
		if (m_ShowScopeFlag)				mask |= VAR_FLAG_SHOW_SCOPE;
		if (changed_var_path_set.count(path) > 0)	mask |= VAR_FLAG_CHANGED;
		
		// add one collected variable
		AddOneCollected(i, ce, mask);
		
		// add expanded to hashset
		if (ce.IsExpanded())	m_ExpandedTableSet.insert(ce.GetPath());
	}
	
	m_Grid.SetCellDataList(m_CellDataList);
	
	const int	n_items = m_Grid.GetNumRows();
	assert(n_items == m_UsedCnt);
	
	if (IsEditable())
	{	// add empty watch field
		m_Grid.SetCellData(m_UsedCnt, 0, "", VAR_UI_DEFAULT);
	}
	
	m_Grid.ApplyCellData();
	
	if (m_RealizedFlag)
	{
		m_ListCtrl.GetGridImp().OnImmediateRefresh();
	}
	// else NOT REALIZED YET
}

void	VarViewCtrl::OnGridRealized(GridImp &grid)
{
	uLog(UI, "VarViewCtrl::OnGridRealized(%S)", m_Info.m_Label);
	
	m_RealizedFlag = true;
}

bool	VarViewCtrl::IsGridShown(GridImp &grid)
{
	// override BUGGY IsShownOnScreen() for cairo
	return PaneMixIn::IsPaneShown();			// on delete will show "uncovered" panes -- FIXME
}

// implementation
bool	VarViewCtrl::IsGridSelectable(GridImp &grid) const
{
	return IsEditable();
}

//---- Get Row Color (toggle with indent) -------------------------------------

Color	VarViewCtrl::GetGridRowColor(GridImp &grid, const size_t row)
{
	if (IsEditable() && (row == m_CollectedList.size()))
	{
		return Color(RGB_COLOR::NO_COLOR);
	}
	
	assert(row < m_CollectedList.size());
	const Collected	&ce = m_CollectedList[row];
	
	const int	indent = ce.GetIndent();
	
	return (indent & 1) ? COLOR_ALT_TABLE_INDENT : Color(RGB_COLOR::NO_COLOR);
}

//---- Get Font Style ---------------------------------------------------------

int	VarViewCtrl::GetFontStyle(const int id) const
{
	if (id < 0)	return 0;

	const Font	*ft = m_CairoRenderer.GetFont(id, false/*not fatal*/);
	if (!ft)	return 0;
	
	return ft->GetFlags();
}

//---- OnGridEvent IMPLEMENTATION ---------------------------------------------

void	VarViewCtrl::OnGridEvent(GridImp &grid, GridEvent &e)
{
	HitItem	hit = e.GetHitItem();
	const int	row = hit.row();
	const int	col = hit.col();
	const int	hit_font_id = hit.GetFontID();
	const bool	hyperlink_f = (hit_font_id >= 0) && (GetFontStyle(hit_font_id) & FONT_STYLE_UNDERLINED);
	
	const COLUMN_T	col_id = s_ColList[col];
	
	switch (e.Type())
	{	case GRID_EVENT::CELL_HOVER:
		{
			if (hit == m_LastHoverItem)	return;			// no change
			m_LastHoverItem = hit;
			
			grid.OnImmediateRefresh();
			
			if (hit.empty())	m_ListCtrl.SetCursor(wxCURSOR_IBEAM);
			else
			{	m_ListCtrl.SetCursor(hyperlink_f ? wxCURSOR_HAND : wxCURSOR_ARROW);
			
				if (col_id != COLUMN_T::VAL_STR)			return;
				
				HighlightTwinCells(hit.GetText());
			}
		}	break;
		
		case GRID_EVENT::CELL_LEFT_CLICK:
		
			OnListItemClicked(e);
			break;
		
		case GRID_EVENT::CELL_RIGHT_CLICK:
		
			OnListItemRightClick(e);
			break;
		
		case GRID_EVENT::ITEM_SELECT:
		{
			if (IsEditable())	grid.SelectCellItem(row).OnImmediateRefresh();
		}	break;
		
		case GRID_EVENT::ITEM_DELETE:
		
			OnListItemDelete(e);
			break;
		
		case GRID_EVENT::CELL_DOUBLE_CLICK:
			
			if (col_id != COLUMN_T::KEY)		return;		// edit only 1st column
			
			OnListItemActivated(e);
			break;
			
		case GRID_EVENT::ITEM_ACTIVATED:
		
			OnListItemActivated(e);
			break;
			
		case GRID_EVENT::HEADER_LEFT_CLICK:
		case GRID_EVENT::HEADER_RIGHT_CLICK:
		case GRID_EVENT::LEAVE_WINDOW:
		
			// nop for now
			break;
			
		default:
			
			break;
	}
}

//---- Highlight Twin Cells ---------------------------------------------------

void	VarViewCtrl::HighlightTwinCells(const string &base_s)
{
	if (base_s.empty())			return;
	
	auto	&grid = m_ListCtrl.GetGridImp();
	
	const int	row_from = grid.GetTopVisibleRow();
	const int	row_to = grid.GetBottomVisibleRow();
	
	vector<CellPos>	pos_list;
	
	for (int row = row_from; row <= row_to; row++)
	{
		const CellData	*cdata = m_Grid.GetCellDataPtr(row, (int)(COLUMN_T::VAL_STR));
		if (!cdata)	continue;
		
		if (base_s == cdata->GetText())		pos_list.emplace_back(row, (int)(COLUMN_T::VAL_STR));
	}
	
	if (pos_list.size() <= 1)		return;		// no other match
	
	Context	&ctx = grid.GetRenderContext();
	
	ctx.SourceColor(COLOR_TWIN_CELL);
	
	for (const auto &cpos : pos_list)
	{	const Rect	rc = grid.GetCellScreenBoundingBox(cpos.first, cpos.second);
		
		ctx.AddRect(rc);
	}
	
	ctx.Fill();
}

//---- On List Item Clicked ---------------------------------------------------

void	VarViewCtrl::OnListItemClicked(GridEvent &e)
{
	auto	&grid = m_ListCtrl.GetGridImp();
	
	HitItem	hit = e.GetHitItem();
	assert(hit.IsOK());
	
	const int	row = hit.row();
	const int	col = hit.col();
	
	if (IsGridSelectable(grid) && (row == m_UsedCnt))
	{	// select dummy watch
		grid.SelectCellItem(row);
		return;
	}
	
	assert(row < m_CollectedList.size());
	const Collected	&ce = m_CollectedList[hit.row()];
	
	uLog(COLLISION, "@%d:%d hit %S (%s)", row, hit.col(), ce.GetPath(), hit.GetTypeString());
	
	const COLUMN_T	col_id = s_ColList[col];
	
	if ((col_id == COLUMN_T::KEY) && (HIT_TYPE::ICON == hit.GetHitType()) && ce.IsTable())
	{
		// toggle table branch
		const bool	expand_f = !ce.IsExpanded();
		const string	var_path = ce.GetPath();
			
		if (expand_f)
			m_ExpandedTableSet.insert(var_path);		// expand
		else	m_ExpandedTableSet.erase(var_path);		// collapse
		
		if (grid.HasSelectedCellItem())
		{	// don't keep selection below toggle point
			if (grid.GetSelectedCellItem() > row)	grid.UnselectCellItem();
		}
			
		// reload variable view, passing new expanded tables to daemon
		DoReload();
		return;
	}
	
	if ((col_id == COLUMN_T::VAL_STR) && ce.IsLuaFileFunction())
	{	// Lua editor URL
		const string	short_filename = ce.GetVal();
		assert(!short_filename.empty());
		const int	start_ln = ce.GetFunctionLineStart();
		assert(start_ln >= 0);
		
		// m_TopFrame.ShowSourceAndLineIntra(short_filename, start_ln);
		// m_TopFrame.CallAfter(&TopFrame::SetSourceFocus, short_filename);
		m_Controller.ShowSourceAtLine(short_filename, start_ln, true/*focus?*/);		// delay focus
		
		return;
	}
	
	if (ce.IsMemoryURL() && (col_id == COLUMN_T::VAL_STR))
	{	// memory inspector
		uLog(COLLISION, "VarViewCtrl::OnListItemClicked() memory URL %S", ce.GetKey());
		
		m_TopFrame.SetMemoryWatch(ce);
		return;
	}
	
	if (ce.IsMemoryURL() && (col_id == COLUMN_T::VAL_TYPE))
	{
		uErr("metatable hyperlinks not implemented");
	}
	
	// select item if editable (otherwise has no purpose)
	if (IsGridSelectable(grid))
	{	// && (0 == ce.GetIndent()))	// let sub-tables get selected (no harm done & consistent navigation
	
		grid.SelectCellItem(row).OnImmediateRefresh();
	}
}

//---- On List Item Right-Click -----------------------------------------------

void	VarViewCtrl::OnListItemRightClick(GridEvent &e)
{
	// HitItem	hit = e.GetHitItem();
	
	wxMenu	menu("");

	menu.Append(VARVIEW_CONTEXT_MENU_ID_COLLAPSE_ALL_TABLES, "Collapse Tables");
	
	// immediate popup menu (will send FOCUS event)
	const int	id = GetPopupMenuSelectionFromUser(menu);
	if (id < 0)	return;				// canceled
	
	switch (id)
	{
		case VARVIEW_CONTEXT_MENU_ID_COLLAPSE_ALL_TABLES:
			
			m_ExpandedTableSet.clear();
			DoReload();
			break;
		
		default:
		
			assert(false);
			break;
	}
}

//---- On List Item Activated -------------------------------------------------

void	VarViewCtrl::OnListItemActivated(GridEvent &e)
{
	const int	row = e.GetIndex();
	
	if (!IsEditable())
	{	// not an editable ctrl
		assert(row < m_CollectedList.size());
		const Collected	&ce = m_CollectedList[row];
		
		if (ce.IsTable())
		{	// toggle table branch
			const bool	expand_f = !ce.IsExpanded();
			const string	var_path = ce.GetPath();
			
			if (expand_f)
				m_ExpandedTableSet.insert(var_path);		// expand
			else	m_ExpandedTableSet.erase(var_path);		// collapse
			
			// reload variable view, passing new expanded tables to daemon
			DoReload();
		}
		else if (ce.IsLuaFileFunction())
		{	string	sourcename;
			int	ln;
			
			const bool	ok = ce.GetLuaFileFunction(sourcename/*&*/, ln/*&*/);
			assert(ok);
			
			// m_TopFrame.CallAfter(&TopFrame::ShowSourceAndLineIntra, sourcename, ln);
			// m_TopFrame.CallAfter(&TopFrame::SetSourceFocus, sourcename);
			m_Controller.ShowSourceAtLine(sourcename, ln, true/*focus?*/);
		}
		
		return;
	}
	
	if (!IsEditable())					return;			// ignore non-editable
	assert(m_WatchBag);
	
	// if is not the last entry (to add a field)
	if (row != m_UsedCnt)
	{	// edit existing watch
		assert(row < m_CollectedList.size());
		const Collected	&ce = m_CollectedList[row];
		if (ce.GetIndent() > 0)				return;			// don't let subtables get edited
	}
	
// modal edit
	auto	&grid = m_ListCtrl.GetGridImp();
	
	string	edited_s;
	
	const bool	ok = grid.EditCellData(e.GetHitItem(), edited_s/*&*/);
	if (!ok)	return;		// was canceled
	
	// if was last item
	if (row == m_UsedCnt)
	{	// add watch
		m_WatchBag->Add(edited_s);
	}
	else
	{	// else edited or removed existing watch
		assert(row < m_CollectedList.size());
		const Collected	&ce = m_CollectedList[row];
		
		assert(ce.GetIndent() == 0);
		
		m_WatchBag->Edit(ce.GetKey(), edited_s);
	}
	
	DoReload();
}

//---- On List Item Delete ----------------------------------------------------

void	VarViewCtrl::OnListItemDelete(GridEvent &e)
{
	if (!IsEditable())		return;			// ignore non-editable
	assert(m_WatchBag);
	
	const int	row = e.GetIndex();
	if (row == m_UsedCnt)		return;			// don't remove last/dummy entry
	
	assert(m_WatchBag);
	assert(row < m_CollectedList.size());
	const Collected	&ce = m_CollectedList[row];
	if (ce.GetIndent() > 0)		return;			// don't remove sub-table
	
	m_WatchBag->Remove(ce.GetKey());
	
	RequestVarReload();
}

//---- INSTANTIATE ------------------------------------------------------------

// static
IVarViewCtrl*	IVarViewCtrl::Create(IBottomNotePanel &bottom_notebook, int id, TopFrame &tf, Controller &controller, IVarViewFlags &vwf, const bool showscope_f, WatchBag *watch_ptr)
{
	return new VarViewCtrl(bottom_notebook, id, tf, controller, vwf, showscope_f, watch_ptr);
}

// nada mas
