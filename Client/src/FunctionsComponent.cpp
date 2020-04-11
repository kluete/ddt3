
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

#include "lx/misc/geometry.h"

#include "lx/juce/JuceApp.h"
#include "lx/juce/jeometry.h"
#include "lx/juce/jwxBinder.h"
#include "lx/juce/jfont.h"

#include "BottomNotebook.h"
#include "TopNotebook.h"
#include "Controller.h"

#include "UIConst.h"
#include "SourceFileClass.h"
#include "StyledSourceCtrl.h"

#include "sigImp.h"
#include "logImp.h"

#include "minus.c_raw"
#include "plus.c_raw"

using namespace std;
using namespace juce;
using namespace LX;
using namespace DDT_CLIENT;

const int	TABLE_MARGIN			= 4;

const Color	COLOR_BACKGROUND(RGB_COLOR::WHITE);
const Color	COLOR_BACKGROUND_ALTERNATE(RGB_COLOR::WAFER_BLUE);
const Color	COLOR_SELECTED(RGB_COLOR::WAFER_GREEN);
const Color	TABLE_GRID_COLOR(RGB_COLOR::GRID_GREY);
// const Color	TABLE_TEXT_COLOR(RGB_COLOR::SOFT_BLACK);
const Color	TABLE_TEXT_COLOR(RGB_COLOR::BLACK);

static const
unordered_map<Component::FocusChangeType, string>	s_FocusNameMap
{
	{Component::FocusChangeType::focusChangedByMouseClick,	"ByMouseClick"},
	{Component::FocusChangeType::focusChangedByTabKey,	"ByTabKey"},
	{Component::FocusChangeType::focusChangedDirectly,	"Directly"},
};

static
string	FocusName(const Component::FocusChangeType t)
{
	return s_FocusNameMap.count(t) ? s_FocusNameMap.at(t) : string("<illegal>");
}

enum class COL_ID : int
{
	IDX = 1,
	NAME,
	LINE,
};

static const
unordered_map<COL_ID, string, EnumClassHash>	s_ColTitleMap
{
	{COL_ID::IDX,		"#"},
	{COL_ID::NAME,		"name"},
	{COL_ID::LINE,		"line"},
};

//---- ListCtrl ---------------------------------------------------------------

class ListCtrl : public TableListBox
{
public:
	ListCtrl()
	{
		// setWantsKeyboardFocus(false);
		// setMouseClickGrabsKeyboardFocus(false);
		// setBroughtToFrontOnMouseClick(false);
	}
	
private:

	#if 0
	void	focusGained(FocusChangeType cause) override
	{
		uLog(FOCUS, "FuncListCtrl::focusGained(%s)", FocusName(cause));
	}
	
	void	focusLost(FocusChangeType cause) override
	{
		uLog(FOCUS, "FuncListCtrl::focusLost(%s)", FocusName(cause));		
	}
	
	void	focusOfChildComponentChanged(FocusChangeType cause) override
	{
		const bool	f = hasKeyboardFocus(true/*with children?*/);
		
		uLog(FOCUS, "FuncListCtrl::childFocus(%c)", f);
	}
	#endif
};	

//---- Functions Component ----------------------------------------------------

class FunctionsComponent : public Component, public IPaneComponent, private TableListBoxModel, private AsyncUpdater, private SlotsMixin<CTX_JUCE>
{
public:
	FunctionsComponent(IBottomNotePanel &bottom_book, ITopNotePanel &top_book)
		: m_Controller(bottom_book.GetController()),
		m_TopNotebook(top_book),
		m_FixedFont(bottom_book.GetController().GetFixedFont()),
		m_OneCharWidth(m_FixedFont->GetCharSize().w()),				// hangs if not called from juce thread?
		m_OneCharHeight(m_FixedFont->GetCharSize().h()),
		m_RowHeight(std::floor(m_OneCharHeight * 1.3)),
		m_MaxColWidthMap{	{COL_ID::IDX,		1},
					{COL_ID::NAME,		17},
					{COL_ID::LINE,		7},
				}
	{
		uLog(APP_INIT, "FunctionsComponent::ctor");
		
		m_ListCtrl.setRowHeight(m_RowHeight);
		m_ListCtrl.setHeaderHeight(m_RowHeight * 1.5);
		m_ListCtrl.setMultipleSelectionEnabled(false);
		m_ListCtrl.setModel(this);
		
		auto	&header = m_ListCtrl.getHeader();
		
		const vector<COL_ID>	col_list = {COL_ID::NAME, COL_ID::LINE};
		
		for (const COL_ID id : col_list)
		{
			assert(s_ColTitleMap.count(id));
			const string	title_s = s_ColTitleMap.at(id);
			header.addColumn(title_s, (int)id, 20/*dummy_w*/);
		}
		
		addAndMakeVisible(&m_ListCtrl);
		
		setSize(800, 600);
		setVisible(true);
		
		setWantsKeyboardFocus(false);
		setMouseClickGrabsKeyboardFocus(false);
		setBroughtToFrontOnMouseClick(false);
		// setEnabled(false);
		
		// should WAIT til is realized???
		top_book.RegisterCodeNavigator(m_Signals);
	}
	
	virtual ~FunctionsComponent()
	{
		uLog(DTOR, "FunctionsComponent::DTOR");		
	}
	
	void	BindToPanel(IPaneMixIn &panel, const size_t retry_ms) override
	{
		if (m_Binder)	return;		// already bound
		
		Rebind(panel.GetWxWindow(), retry_ms);
	}
	
private:
	
	void	focusGained(FocusChangeType cause) override
	{
		uLog(FOCUS, "FunctionsComponent::focusGained(%s)", FocusName(cause));
	}
	
	void	focusLost(FocusChangeType cause) override
	{
		uLog(FOCUS, "FunctionsComponent::focusLost(%s)", FocusName(cause));		
	}
	
	void	Rebind(wxWindow *win, const size_t retry_ms)
	{
		assert(win);
		
		if (!IjwxWindowBinder::IsBindable(*win))
		{	
			// not bindable yet, retry later
			MixDelay("", retry_ms, this, &FunctionsComponent::Rebind, win, retry_ms);
			return;
		}
		
		m_Binder.reset(IjwxWindowBinder::Create(*win, *this, AUTO_FOCUS_T::WX, "Functions"));
		
		uLog(REALIZED, "FunctionsComponent::Rebind() ok");
		
		MixConnect(this, &FunctionsComponent::OnEditorFunctionsChanged, m_TopNotebook.GetSignals().OnEditorFunctionsChanged);
	}
	
	void	OnEditorFunctionsChanged(const string shortname, vector<Bookmark> bmks)
	{
		SetEntries(shortname, bmks);
	}
	
	Bookmark	GetEntry(const size_t row) const
	{
		unique_lock<mutex>	lock(m_QueueMutex);
		
		assert(row < m_Bookmarks.size());
		
		return m_Bookmarks[row];
	}
	
	int	getNumRows(void) override
	{
		return m_Bookmarks.size();
	}
	
	void	paintRowBackground(Graphics &g, int row, int w, int h, bool row_selected_f) override
	{
		const Color	bgclr = row_selected_f ? COLOR_SELECTED : ((row & 1) ? COLOR_BACKGROUND : COLOR_BACKGROUND_ALTERNATE);
		g.setOpacity(1.f);
		g.fillAll(bgclr);
		
		// paint HORIZONTAL grid lines (can't paint vertical here)
		g.setColour(TABLE_GRID_COLOR);
		g.fillRect(0, h - 1, w, 1);
	}
	
	void	paintCell(Graphics &g, int row, int col, int w, int h, bool row_selected_f) override
	{
		g.setOpacity(1.f);
		// vertical line
		g.setColour(TABLE_GRID_COLOR);
		g.fillRect(w - 1, 0, 1, h);
		
		const int	y0 = m_RowHeight - m_OneCharHeight;
		const iRect	r(iRect(0, y0, w, h).Inset(TABLE_MARGIN, 0));
		
		const auto	e = GetEntry(row);
		
		g.setColour(TABLE_TEXT_COLOR);
		
		const COL_ID	col_id = (COL_ID)col;
		bool	right_align_f = false;
		
		string	s;
		
		switch (col_id)
		{
			case COL_ID::IDX:
				
				s = to_string(row);
				break;
				
			case COL_ID::NAME:
			
				s = e.m_Name;
				break;
				
			case COL_ID::LINE:
			
				s = to_string(e.m_Line);
				right_align_f = true;
				break;
				
			default:
			
				assert(0);
				break;
		}
		
		m_FixedFont->DrawText(g, s, r, right_align_f);
	}
	
	int	getColumnAutoSizeWidth(int col) override
	{
		if (0 == col)			return 0;
		
		const COL_ID	col_id = (COL_ID)col;
		assert(m_MaxColWidthMap.count(col_id));
		const size_t	n_chars = m_MaxColWidthMap.at(col_id);
		
		const size_t	max_width = n_chars * m_OneCharWidth;
		
		return max_width + (2 * TABLE_MARGIN);
	}
	
	void	cellClicked(int row, int col, const MouseEvent &e) override
	{
		uLog(FOCUS, "FunctionsComponent::cellClicked(row %d)", row);
		
		const auto	bmk = GetEntry(row);
		
		m_Signals.OnShowLocation(CTX_JUCE, m_Shortname, bmk.m_Line);
	}
 
	void	SetEntries(const string shortname, vector<Bookmark> bmks)
	{
		unique_lock<mutex>	lock(m_QueueMutex);
		
		m_Shortname = shortname;
		
		// (avoids const member compile error)
		m_Bookmarks.swap(bmks);
		
		triggerAsyncUpdate();
	}
	
	void	handleAsyncUpdate(void) override
	{
		const size_t	n_tot = m_Bookmarks.size();
		
		m_MaxColWidthMap[COL_ID::IDX] = std::log10(n_tot) + 1;
		m_MaxColWidthMap[COL_ID::NAME] = 1;
		m_MaxColWidthMap[COL_ID::LINE] = 1;
		
		for (size_t row = 0; row < n_tot; row++)
		{
			const Bookmark	e = GetEntry(row);			// (brief lock)
		
			const size_t	line_len = std::log10(e.m_Line) + 1;
			
			m_MaxColWidthMap[COL_ID::NAME] = std::max(m_MaxColWidthMap[COL_ID::NAME], e.m_Name.length());
			m_MaxColWidthMap[COL_ID::LINE] = std::max(m_MaxColWidthMap[COL_ID::LINE], line_len);
		}
		
		m_ListCtrl.updateContent();
		m_ListCtrl.autoSizeAllColumns();
	}
	
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

	Controller			&m_Controller;
	ITopNotePanel			&m_TopNotebook;
	
	ListCtrl			m_ListCtrl;
	shared_ptr<IFixedFont>		m_FixedFont;
	const int			m_OneCharWidth;
	const int			m_OneCharHeight;
	const int			m_RowHeight;
	
	mutable mutex				m_QueueMutex;
	string					m_Shortname;
	vector<Bookmark>			m_Bookmarks;
	unordered_map<COL_ID, size_t, EnumHash>	m_MaxColWidthMap;
	
	unique_ptr<IjwxWindowBinder>		m_Binder;
	LogicSignals				m_Signals;
};

//---- INSTANTIATE ------------------------------------------------------------

// static
IPaneComponent*	IPaneComponent::CreateFunctionsComponent(IBottomNotePanel &bottom_book, ITopNotePanel &top_book)
{
	jAutoMsgPause	lock;
	
	return new FunctionsComponent(bottom_book, top_book);
}

// nada mas
