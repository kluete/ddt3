
#include <cassert>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <thread>
#include <cmath>

#include "JuceHeader.h"

#include "lx/ulog.h"
#include "lx/color.h"

#include "lx/juce/JuceApp.h"
#include "lx/misc/geometry.h"
#include "lx/juce/jeometry.h"
#include "lx/juce/jwxBinder.h"
#include "lx/juce/jfont.h"

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

#include "minus.c_raw"
#include "plus.c_raw"

using namespace std;
using namespace juce;
using namespace LX;
using namespace DDT_CLIENT;

const int	TABLE_MARGIN			= 4;
const int	TABLE_INDENT			= 2;
const int	TABLE_ICON_SZ			= 16;

const Color	COLOR_BACKGROUND(RGB_COLOR::WHITE);
const Color	COLOR_BACKGROUND_ALTERNATE(RGB_COLOR::WAFER_BLUE);
const Color	COLOR_SELECTED(RGB_COLOR::WAFER_GREEN);
const Color	COLOR_TABLE_GRID(RGB_COLOR::GRID_GREY);
const Color	COLOR_TABLE_TEXT(RGB_COLOR::SOFT_BLACK);
const Color	COLOR_LUA_URL(10, 10, 255, 0xff);
const Color	COLOR_MEMORY_URL(10, 120, 10, 0xff);

//---- Load CRAW Buffer -------------------------------------------------------

static
Image	LoadCrawJImage(const uint8_t *p, const size_t sz)
{
	return PNGImageFormat::loadFrom(p, sz);
}

#define	LOAD_CRAW_JIMG(nm)	LoadCrawJImage(nm##_craw.m_Data, nm##_craw.m_Len)

using COL_ID = COLUMN_T;

static const
unordered_map<COL_ID, string, EnumClassHash>	s_ColTitleMap
{
	{COL_ID::NONE,		"#"},
	{COL_ID::KEY,		"key"},
	{COL_ID::VAL_STR,	"val"},
	{COL_ID::VAL_TYPE,	"type"},
	{COL_ID::SCOPE,		"scope"},
	{COL_ID::VAL_PATH,	"path"},
};

static const
vector<COL_ID>	s_ColList {COL_ID::NONE, COL_ID::KEY, COL_ID::VAL_STR, COL_ID::VAL_TYPE, COL_ID::SCOPE, COL_ID::VAL_PATH};

/*
class EditableTextCustomComponent : public Label
{
public:
	EditableTextCustomComponent(TableDemoComponent& td)  : owner (td)
	{
	    // double click to edit the label text; single click handled below
	    setEditable(false, true, false);
	    setColour(textColourId, Colours::black);
	}

	void mouseDown(const MouseEvent& event) override
	{
		// single click on the label should simply select the row
		owner.table.selectRowsBasedOnModifierKeys(row, event.mods, false);

		Label::mouseDown(event);
	}

	void textWasEdited() override
	{
		owner.setText(columnId, row, getText());
	}

	// Our demo code will call this when we may need to update our contents
	void setRowAndColumn(const int newRow, const int newColumn)
	{
		row = newRow;
		columnId = newColumn;
		setText(owner.getText(columnId, row), dontSendNotification);
	}

private:

	TableDemoComponent&	owner;
	int			row, columnId;
};
*/

//---- VarView Component & Model ----------------------------------------------

class VarViewComponent : public Component, public IPaneComponent, private TableListBoxModel, private SlotsMixin<CTX_JUCE>, private AsyncUpdater
{
public:
	VarViewComponent(IBottomNotePanel &bottom_book, const int win_id, ITopNotePanel &top_book, Controller &controller)
		: m_Controller(controller),
		m_TopFrame(controller.GetTopFrame()),
		m_TopNotebook(top_book),
		m_BottomNotebook(bottom_book),
		m_VarViewFlags(m_TopFrame.GetVarViewFlags()),
		m_Info(IVarViewCtrl::GetVarViewInfo(IBottomNotePanel::WinToPaneID(win_id))),
		m_ColIDtoIndex(VarViewComponent::GetInvColMap(s_ColList)),
		m_wxSlotsPtr(Slots::Create(CTX_WX)),
		m_wxSlots(*m_wxSlotsPtr),
		
		m_IconMap(LoadIcons()),
		m_FixedFont(controller.GetFixedFont()),
		m_OneCharWidth(m_FixedFont->GetCharSize().w()),
		m_OneCharHeight(m_FixedFont->GetCharSize().h()),
		m_RowHeight(std::floor(m_FixedFont->GetCharSize().h() * 1.3)),
		m_IndentMul(TABLE_INDENT * m_OneCharWidth)
		
	{
		(void)m_Controller;
		
		uLog(APP_INIT, "VarViewComponent::ctor");
		
		const int	header_h = m_RowHeight * 1.5;
		
		m_ListCtrl.setRowHeight(m_RowHeight);
		m_ListCtrl.setHeaderHeight(header_h);
		m_ListCtrl.setMultipleSelectionEnabled(false);
		m_ListCtrl.setModel(this);
		
		auto	&header = m_ListCtrl.getHeader();
		
		const vector<COL_ID>	col_list = {COL_ID::KEY, COL_ID::VAL_STR, COL_ID::VAL_TYPE, COL_ID::SCOPE};
		
		for (const COL_ID id : col_list)
		{
			const size_t	ind = m_ColIDtoIndex.at(id);
			
			assert(s_ColTitleMap.count(id));
			const string	title_s = s_ColTitleMap.at(id);
			header.addColumn(title_s, ind, 20/*dummy_w*/);
		}
		
		addAndMakeVisible(&m_ListCtrl);
		
		setSize(800, 600);
		setVisible(true);
		
		auto	&varview_ui_sig = m_TopFrame.GetSignals(PANE_ID_ALL_VAR_VIEWS);
		// MixConnect(this, &VarViewCtrl::UpdateVarCheckboxes, varview_ui_sig.OnVarCheckboxes);
		// MixConnect(this, &VarViewCtrl::SetGridOverlays, varview_ui_sig.OnGridOverlays);
		MixConnect(this, &VarViewComponent::ResetColumns, varview_ui_sig.OnVarColumns);			// should only connect on REALIZE ?
	
		const int	requester_id = m_Info.m_ClientID;
	
		MixConnect(this, &VarViewComponent::OnPostedCollectedMessage, m_TopFrame.GetSignals(requester_id).OnCollectedMessage);
		
		setWantsKeyboardFocus(false);
	}
	
	virtual ~VarViewComponent()
	{
		uLog(DTOR, "VarViewComponent::DTOR");		
	}
	
	void	BindToPanel(IPaneMixIn &panel, const size_t retry_ms) override
	{
		if (m_Binder)	return;		// already bound
		
		Rebind(panel.GetWxWindow(), retry_ms);
	}
	
	unordered_map<COL_ID, size_t, EnumHash>	GetInvColMap(const vector<COL_ID> &col_list) const;
	
private:
	
	void	Rebind(wxWindow *win, const size_t retry_ms)
	{
		assert(win);
		
		if (!IjwxWindowBinder::IsBindable(*win))
		{	
			// can't bind until is VISIBLE
			// uWarn("VarViewComponent NOT bindable yet...");
			
			MixDelay("", retry_ms, this, &VarViewComponent::Rebind, win, retry_ms);
			return;
		}
		
		m_Binder.reset(IjwxWindowBinder::Create(*win, *this, AUTO_FOCUS_T::WX, "VarViewComponent"));
		
		uLog(REALIZED, "VarViewComponent::Rebind() ok");
		
		// connect LATE
		m_TopNotebook.RegisterCodeNavigator(m_Signals);
		
		MixConnect(this, &VarViewComponent::OnPanelClearVariables, m_BottomNotebook.GetSignals(m_Info.m_PaneID).OnPanelClearVariables);
		MixConnect(this, &VarViewComponent::OnRequestVarReload, m_BottomNotebook.GetSignals(m_Info.m_PaneID).OnPanelRequestReload);		
		
		// request reload from here is ok
		OnRequestVarReload();
	}
	
	int	getNumRows(void) override
	{
		return m_EntryList.size();
	}
	
	void	paintRowBackground(Graphics &g, int row, int w, int h, bool row_selected_f) override
	{
		const auto	ce = GetEntry(row);
		const size_t	indent = ce.GetIndent();
		
		const Color	bgclr = row_selected_f ? COLOR_SELECTED : ((indent & 1) ? COLOR_BACKGROUND_ALTERNATE : COLOR_BACKGROUND);
		g.fillAll(bgclr);
		
		// paint HORIZONTAL grid lines (can't paint vertical here)
		g.setColour(COLOR_TABLE_GRID);
		g.fillRect(0, h - 1, w, 1);
	}
	
	void	paintCell(Graphics &g, int row, int col, int w, int h, bool row_selected_f) override
	{
		g.setColour(COLOR_TABLE_GRID);
		// vertical line
		g.fillRect(w - 1, 0, 1, h);
		
		assert(col < s_ColList.size());
		const COL_ID	col_id = s_ColList[col];
		
		const auto	ce = GetEntry(row);
		
		g.setColour(COLOR_TABLE_TEXT);
		
		const int	dy = m_RowHeight - m_OneCharHeight;
		const iRect	r(iRect(0, dy, w, h - dy).Inset(TABLE_MARGIN, 0));
		
		switch (col_id)
		{
			case COL_ID::KEY:
			{
				const size_t	indent = ce.GetIndent();
				
				const int	dx = indent * m_IndentMul;
				if (ce.IsTable())
				{
					const int	icon_id = ce.IsExpanded() ? BITMAP_ID_MINUS : BITMAP_ID_PLUS;
					assert(m_IconMap.count(icon_id));
					
					g.drawImageAt(m_IconMap.at(icon_id), dx, 0, false/*fill alpha?*/);
				}
				
				const int	x_offset = TABLE_ICON_SZ + TABLE_MARGIN + dx;
				const string	s = ce.GetDecoratedKey();
				
				m_FixedFont->DrawText(g, s, r.Offset(x_offset, 0), false);
			}	break;
				
			case COL_ID::VAL_STR:
			{
				const bool	url_f = ce.IsLuaFileFunction() || ce.IsMemoryURL();
				
				if (ce.IsLuaFileFunction())	g.setColour(COLOR_LUA_URL);		// Lua source url
				else if (ce.IsMemoryURL())	g.setColour(COLOR_MEMORY_URL);		// memory url
				else				g.setColour(COLOR_TABLE_TEXT);
				
				const string	s = ce.GetDecoratedVal();
				
				m_FixedFont->DrawText(g, s, r, false);
				
				if (url_f)
				{
					m_FixedFont->DrawUnderline(g, s, r, false);
				}
			}	break;
				
			case COL_ID::VAL_TYPE:
			
				m_FixedFont->DrawText(g, Collected::GetTypeS(ce.GetType()), r, false);
				break;
				
			case COL_ID::SCOPE:
			
				m_FixedFont->DrawText(g, ce.GetScopeStr(), r, false);
				break;
				
			default:
			
				assert(0);
				break;
		}
	}
	
	int	getColumnAutoSizeWidth(int col) override
	{
		if (!col)		return 0;
		
		const COL_ID	col_id = s_ColList[col];
		
		assert(m_MaxColWidthMap.count(col_id));
		
		const size_t	n_pix = m_MaxColWidthMap.at(col_id);
		
		return n_pix + (2 * TABLE_MARGIN);
	}
	
	void	SetEntries(vector<Collected> entries, StringSet expanded_tab_set)
	{
		unique_lock<mutex>	lock(m_QueueMutex);
		
		// avoids const member compile error
		m_EntryList.swap(entries);
		m_ExpandedTableSet = expanded_tab_set;
		
		triggerAsyncUpdate();
	}
	
	void	OnPostedCollectedMessage(LX::Message msg)
	{
		LX::MemoryInputStream	mis(msg.GetBuff());
		DataInputStream		dis(mis);
		
		DAEMON_MSG	msg_t;
		uint32_t	requester_id;
	
		dis >> msg_t >> requester_id;
	
		SetVariables(dis);
	}
	
	void	ResetColumns(const uint32_t flags0)
	{
		// const uint32_t	flags = flags0 | (true ? VAR_FLAG_SHOW_SCOPE : 0);
		
		OnRequestVarReload();
	}
	
	void	OnRequestVarReload(void)
	{
		OnReload(m_ExpandedTableSet.ToStringList());		
	}
	
	void	OnReload(vector<string> expanded_tables)
	{
		m_wxSlots.QueueAsync(this, &VarViewComponent::OnWxThread, &m_TopFrame, expanded_tables);
	}
	
	void	OnPanelClearVariables(void)
	{
		SetEntries(vector<Collected>{}, m_ExpandedTableSet);
	}
	
	void	OnWxThread(TopFrame *tf, vector<string> expanded_tables)
	{
		assert(tf);
		
		if (!tf->GetController().IsLuaListening())
		{	uWarn("VarViewComponent::OnWxThread() ignored - lua is not listening!");
			return;
		}
		
		const int32_t	client_stack_level = tf->GetVisibleStackLevel();
		
		VariableRequest	req(m_Info.m_ClientID, client_stack_level, expanded_tables, m_VarViewFlags.GetMask());
		
		LX::MemoryOutputStream	mos;
		DataOutputStream	dos(mos);
		
		dos << m_Info.m_RequestMsg << req;
		
		tf->SendRequestReply(mos);
	}
	
	void	SetVariables(DataInputStream &mis);
	
	void	resized() override
	{
		const iRect	r = getLocalBounds();
		
		m_ListCtrl.setBounds(r.Inset(8));
		
		Component::resized();
	}
	
	void	paint(Graphics &g) override
	{
		const iRect	r = getLocalBounds();
		
		g.fillAll(Color(RGB_COLOR::GTK_GREY));
		
		// border
		g.setColour(Color(RGB_COLOR::GTK_GREY).Darker(80));
		g.drawRect(r.Inset(1), 1);
	}

	void	cellClicked(int row, int col, const MouseEvent &e) override
	{
		assert(col < s_ColList.size());
		const COL_ID	col_id = s_ColList[col];
		
		const auto	ce = GetEntry(row);
		
		if (col_id == COL_ID::KEY)
		{
			if (!ce.IsTable())		return;		// ignored
			
			const bool	expand_f = !ce.IsExpanded();
			const string	var_path = ce.GetPath();
			auto		expanded_tab_set = m_ExpandedTableSet;
			
			if (expand_f)
				expanded_tab_set.insert(var_path);	// expand
			else	expanded_tab_set.erase(var_path);	// collapse
		
			// RELOAD
			OnReload(expanded_tab_set.ToStringList());
			return;
		}
		
		if (col_id == COL_ID::VAL_STR)
		{	
			const bool	url_f = ce.IsLuaFileFunction() || ce.IsMemoryURL();
			if (!url_f)			return;		// ignored
		
			if (ce.IsLuaFileFunction())
			{	// Lua source
				string	shortname;
				int	ln;
				
				const bool	ok = ce.GetLuaFileFunction(shortname/*&*/, ln/*&*/);
				assert(ok);
				
				m_Signals.OnShowLocation(CTX_JUCE, shortname, ln);
				return;
			}
			if (ce.IsMemoryURL())
			{	// memory url
				m_Signals.OnShowMemory(CTX_JUCE, ce.GetKey(), ce.GetVal(), ce.GetTypeString(), ce.IsBinaryData());
				return;
			}
		}
	}
	
	void	cellDoubleClicked(int row, int col, const MouseEvent &e) override
	{
		uWarn("cellDoubleClicked()");
	}
	
	Collected	GetEntry(const size_t row) const
	{
		unique_lock<mutex>	lock(m_QueueMutex);
		
		assert(row < m_EntryList.size());
		
		return m_EntryList[row];
	}
	
	void	handleAsyncUpdate(void) override
	{
		auto	&hdr = m_ListCtrl.getHeader();
		
		const size_t	n_cols = hdr.getNumColumns(true/*onlyCountVisible?*/);
		
		vector<COL_ID>	vis_cols;

		for (int i = 0; i < n_cols; i++)
		{
			const int	col = hdr.getColumnIdOfIndex(i, true/*onlyCountVisible?*/);
			assert(col < s_ColList.size());
			
			const COLUMN_T	col_id = s_ColList[col];
			vis_cols.push_back(col_id);
			
			m_MaxColWidthMap[col_id] = 1;
		}

		const uint32_t	mask = m_VarViewFlags.GetMask();
		
		for (size_t row = 0; row < m_EntryList.size(); row++)
		{
			const Collected	ce = GetEntry(row);			// (brief lock)
			
			const unordered_map<COLUMN_T, string>	ce_t = IVarViewCtrl::GetCollectedFields(ce, mask);
			
			for (const auto &it : ce_t)
			{
				const COL_ID	col_id = it.first;
				const size_t	indent = (col_id == COL_ID::KEY) ? ce.GetIndent() : 0;
				const string	s = it.second;
				
				const size_t	pix_w = (indent * m_IndentMul) + (s.length() * m_OneCharWidth);
				
				m_MaxColWidthMap[col_id] = std::max(m_MaxColWidthMap[col_id], pix_w);			// factor-in indent
			}
		}
		
		m_ListCtrl.updateContent();
		m_ListCtrl.autoSizeAllColumns();
	}
	
	unordered_map<int, Image>	LoadIcons(void) const
	{
		unordered_map<int, Image>	res;
		
		res.emplace(BITMAP_ID_PLUS, LOAD_CRAW_JIMG(plus));
		res.emplace(BITMAP_ID_MINUS, LOAD_CRAW_JIMG(minus));
		
		return res;
	}
	
	Controller				&m_Controller;
	TopFrame				&m_TopFrame;
	ITopNotePanel				&m_TopNotebook;
	IBottomNotePanel			&m_BottomNotebook;
	IVarViewFlags				&m_VarViewFlags;
	
	const varview_info				m_Info;
	const unordered_map<COL_ID, size_t, EnumHash>	m_ColIDtoIndex;
	unique_ptr<Slots>				m_wxSlotsPtr;
	Slots						&m_wxSlots;
	
	TableListBox				m_ListCtrl;
	const unordered_map<int, Image>		m_IconMap;
	shared_ptr<IFixedFont>			m_FixedFont;
	const int				m_OneCharWidth, m_OneCharHeight;
	const int				m_RowHeight;
	const int				m_IndentMul;
	
	mutable mutex				m_QueueMutex;
	vector<Collected>			m_EntryList;
	StringSet				m_ExpandedTableSet;
	unordered_map<COL_ID, size_t, EnumHash>	m_MaxColWidthMap;
	
	unique_ptr<IjwxWindowBinder>		m_Binder;
	LogicSignals				m_Signals;
};

//---- Inv Col Map ------------------------------------------------------------

unordered_map<COL_ID, size_t, EnumHash>	VarViewComponent::GetInvColMap(const vector<COL_ID> &col_list) const
{
	unordered_map<COL_ID, size_t, EnumClassHash>	res;
	
	for (size_t i = 0; i < col_list.size(); i++)
	{
		const COL_ID	col_id = col_list[i];
		assert(!res.count(col_id));
		
		res.emplace(col_id, i);
	}
	
	return res;
}

//---- Set Variables DIRECTLY by deserializing from daemon --------------------

void	VarViewComponent::SetVariables(DataInputStream &mis)
{
	const CollectedList	new_collected(mis);
	
	vector<Collected>	entries;
	
	for (const auto &ce : new_collected)
	{
		entries.emplace_back(ce);
	}
	
	// rebuild expanded hashset
	StringSet	expanded_table_set;
	
	// unroll to listCtrl (???)
	for (int i = 0; i < new_collected.size(); i++)
	{
		const Collected	&ce = new_collected[i];
		const wxString	path = ce.GetPath();
		
		// add expanded to hashset
		if (ce.IsExpanded())	expanded_table_set.insert(ce.GetPath());
	}
	
	SetEntries(entries, expanded_table_set);
}

//---- INSTANTIATE ------------------------------------------------------------

// static
IPaneComponent*	IPaneComponent::CreateVarViewComponent(IBottomNotePanel &bottom_book, const int win_id, ITopNotePanel &top_book)
{
	jAutoMsgPause	lock;
	
	Controller &controller = bottom_book.GetController();
	
	return new VarViewComponent(bottom_book, win_id, top_book, controller);
}

// nada mas
