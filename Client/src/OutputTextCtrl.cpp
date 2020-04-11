// Lua ddt output textCtrl

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <thread>
#include <cmath>

#include "wx/frame.h"

#include "JuceHeader.h"

#include "lx/ulog.h"
#include "lx/misc/geometry.h"

#include "lx/juce/jeometry.h"
#include "lx/juce/jwxBinder.h"
#include "lx/juce/JuceApp.h"
#include "lx/juce/jfont.h"

#include "sigImp.h"

#include "logImp.h"
#include "BottomNotebook.h"

#include "OutputTextCtrl.h"

using namespace std;
using namespace juce;
using namespace LX;
using namespace DDT_CLIENT;

const int	TABLE_MARGIN			= 4;

const Color	COLOR_BACKGROUND(RGB_COLOR::WHITE);
const Color	COLOR_BACKGROUND_ALTERNATE(RGB_COLOR::WAFER_BLUE);
const Color	COLOR_ELAP(RGB_COLOR::WAFER_GREEN);
const Color	TABLE_GRID_COLOR(RGB_COLOR::GRID_GREY);

enum class COL_ID : int
{
	IDX = 1,
	STAMP,
	D_US,
	THREAD,
	LEVEL,
	MSG,
};

static const
unordered_map<COL_ID, string, EnumClassHash>	s_ColTitleMap
{
	{COL_ID::IDX,		"#"},
	{COL_ID::STAMP,		"time"},
	{COL_ID::D_US,		"dus"},
	{COL_ID::THREAD,	"th"},
	{COL_ID::LEVEL,		"level"},
	{COL_ID::MSG,		"message"},
};

struct ddt_log_evt
{
public:

	ddt_log_evt(const timestamp_t stamp, const LogLevel level, const string &msg, const size_t thread_index, const bool deamon_f, const size_t dus)
		: m_Stamp(stamp), m_Lvl(level), m_Msg(msg), m_ThreadIndex(thread_index), m_DaemonFlag(deamon_f), m_DUS(dus)
	{
	}
	
	const timestamp_t	m_Stamp;
	const LogLevel		m_Lvl;
	const string		m_Msg;
	const size_t		m_ThreadIndex;
	const bool		m_DaemonFlag;
	const size_t		m_DUS;
};

//---- Log List Model ---------------------------------------------------------

class LogListModel : public TableListBoxModel, public LogSlot, private AsyncUpdater
{
public:	
	LogListModel(TableListBox &tableCtrl, shared_ptr<IFixedFont> ff)
		: m_TableCtrl(tableCtrl), m_FixedFont(ff),
		m_OneCharWidth(ff->GetCharSize().w()),			// hangs if not called from juce thread?
		m_OneCharHeight(ff->GetCharSize().h()),
		m_RowHeight(std::floor(m_OneCharHeight * 1.285)),
		m_MaxColWidthMap{	{COL_ID::IDX,		1},
					{COL_ID::STAMP,		17},
					{COL_ID::D_US,		7},
					{COL_ID::THREAD,	1},
					{COL_ID::LEVEL,		12},
					{COL_ID::MSG,		1},
				}
	{
	}
	
	virtual ~LogListModel()
	{
		LogSlot::DisconnectSelf();

		uLog(DTOR, "LogListModel::DTOR");
	}
	
	int	getNumRows(void) override
	{
		return m_LogEvents.size();
	}
	
	void	ResetRenderPass(void)
	{
		m_PassRows.clear();
	}
	
	void	paintRowBackground(Graphics &g, int row, int w, int h, bool row_selected_f) override
	{
		const auto	e = GetEntry(row);
		
		// const Color	bgclr = row_selected_f ? COLOR_SELECTED : ((e.m_ThreadIndex & 1) ? COLOR_BACKGROUND_ALTERNATE : COLOR_BACKGROUND);
		const Color	bgclr = ((e.m_ThreadIndex & 1) ? COLOR_BACKGROUND_ALTERNATE : COLOR_BACKGROUND);
		g.setOpacity(1.f);
		g.fillAll(bgclr);
		
		// paint HORIZONTAL grid lines (can't paint vertical here)
		g.setColour(TABLE_GRID_COLOR);
		g.fillRect(0, h - 1, w, 1);
		
		m_PassRows.push_back(row);
	}
	
	void	paintCell(Graphics &g, int row, int col, int w, int h, bool row_selected_f) override
	{
		g.setOpacity(1.f);
		// vertical line
		g.setColour(TABLE_GRID_COLOR);
		g.fillRect(w - 1, 0, 1, h);
		
		const COL_ID	col_id = (COL_ID)col;
		if (col_id == COL_ID::STAMP)
		{
			const double	elap_secs = std::min(GetEntryElap(row), 80.0);
			if (elap_secs > 1.0)
			{
				g.setOpacity(0.1f);
				g.setColour(COLOR_ELAP);
				
				const int	elap_w = w * elap_secs / 80.0;
				const iRect	clip_r(g.getClipBounds());
				
				g.fillRect(clip_r.with_w(elap_w).Inset(1));
				
				g.setOpacity(1.f);
			}			
		}
		
		const auto	e = GetEntry(row);
		
		const log_def	def = ISoloLogFrame::GetLevelDef(e.m_Lvl);
		g.setColour(Color(def.m_Color));
		
		const int	y0 = (m_RowHeight - 1) - m_OneCharHeight;
		const iRect	r(iRect(0, y0, w, h).Inset(TABLE_MARGIN, 0));		// (don't care about rect height)
		
		string	s;
		bool	right_align_f = false;
		
		switch (col_id)
		{
			case COL_ID::IDX:
				
				s = to_string(row);
				right_align_f = true;
				break;
				
			case COL_ID::STAMP:
			
				s = e.m_Stamp.str(STAMP_FORMAT::MICROSEC);
				break;
				
			case COL_ID::D_US:
			
				s = e.m_DaemonFlag ? to_string(e.m_DUS) : string("");
				break;
				
			case COL_ID::THREAD:
			
				s = to_string(e.m_ThreadIndex);
				break;
			
			case COL_ID::LEVEL:
			
				s = def.m_Label;
				break;
				
			case COL_ID::MSG:
			
				s = e.m_Msg;
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
		
		return (m_OneCharWidth * n_chars) + (2 * TABLE_MARGIN);
	}
	
	void	LogEntry(const timestamp_t stamp, const LogLevel level, const string &s, const size_t thread_index, const bool deamon_f, const size_t d_us)
	{
		unique_lock<mutex>	lock(m_QueueMutex);
		
		m_LogEvents.emplace_back(stamp, level, s, thread_index, deamon_f, d_us);
		
		m_MaxColWidthMap[COL_ID::MSG] = std::max(m_MaxColWidthMap[COL_ID::MSG], s.length());
		
		triggerAsyncUpdate();
	}
	
	int	GetRowHeight(void) const	{return m_RowHeight;}
	
	void	Refresh(void)
	{
		triggerAsyncUpdate();
	}
	
private:
	
	ddt_log_evt	GetEntry(const size_t row) const
	{
		unique_lock<mutex>	lock(m_QueueMutex);
		
		assert(row < m_LogEvents.size());
		
		return m_LogEvents[row];
	}
	
	double	GetEntryElap(const size_t row) const
	{
		unique_lock<mutex>	lock(m_QueueMutex);
		
		assert(row < m_LogEvents.size());
		
		if (row == 0)		return 0.0;
		
		return m_LogEvents[row].m_Stamp.delta_secs(m_LogEvents[row - 1].m_Stamp);
	}
	
	void	LogAtLevel(const timestamp_t stamp, const LogLevel level, const string &s) override
	{
		const auto	tid = this_thread::get_id();
		
		if (!m_ThreadIdMap.count(tid))	m_ThreadIdMap.emplace(tid, m_ThreadIdMap.size());
		
		const size_t	thread_index = m_ThreadIdMap.at(tid);
		
		const size_t	d_us = stamp.delta_us(m_LastClientStamp);
		m_LastClientStamp = stamp;
		
		LogEntry(stamp, level, s, thread_index, false, d_us);
	}
	
	void	handleAsyncUpdate(void) override
	{
		const size_t	n_tot = m_LogEvents.size();
		assert(n_tot);
		
		m_MaxColWidthMap[COL_ID::IDX] = std::log10(n_tot) + 1;
		m_MaxColWidthMap[COL_ID::THREAD] = std::log10(m_ThreadIdMap.size()) + 1;
		
		m_TableCtrl.updateContent();
		m_TableCtrl.autoSizeAllColumns();
		
		m_TableCtrl.scrollToEnsureRowIsOnscreen(n_tot - 1);
	}
	
	TableListBox				&m_TableCtrl;
	shared_ptr<IFixedFont>			m_FixedFont;
	const int				m_OneCharWidth;
	const int				m_OneCharHeight;
	const int				m_RowHeight;
	
	vector<int>				m_PassRows;
	
	mutable mutex				m_QueueMutex;
	vector<ddt_log_evt>			m_LogEvents;
	unordered_map<thread::id, size_t>	m_ThreadIdMap;
	unordered_map<COL_ID, size_t, EnumHash>	m_MaxColWidthMap;
	
	timestamp_t				m_LastClientStamp;
};

//---- Log Component ----------------------------------------------------------

class LogComponent : public Component, public ILogOutput, private SlotsMixin<CTX_JUCE>
{
public:
	LogComponent(shared_ptr<IFixedFont> ff)
		: m_ListCtrl{},
		m_ListModel(m_ListCtrl, ff)
	{
		uLog(APP_INIT, "LogComponent::ctor");
		
		const int	row_h = m_ListModel.GetRowHeight();
		const int	header_h = row_h * 1.5;
		
		m_ListCtrl.setRowHeight(row_h);
		m_ListCtrl.setHeaderHeight(header_h);
		m_ListCtrl.setMultipleSelectionEnabled(false);
		m_ListCtrl.setModel(&m_ListModel);
		
		auto	&header = m_ListCtrl.getHeader();
		
		const vector<COL_ID>	col_list = {COL_ID::IDX, COL_ID::STAMP, COL_ID::THREAD, COL_ID::LEVEL, COL_ID::MSG};
		
		for (const COL_ID id : col_list)
		{
			assert(s_ColTitleMap.count(id));
			const string	title_s = s_ColTitleMap.at(id);
			header.addColumn(title_s, (int)id, 20/*dummy_w*/);
		}
		
		addAndMakeVisible(&m_ListCtrl);
		
		setSize(800, 600);
		setVisible(true);
		
		uMsg("vanilla log from ui thread");
		
		rootLog::Get().Connect(&m_ListModel);
	}
	
	virtual ~LogComponent()
	{
		uLog(DTOR, "LogComponent::DTOR");		
	}
	
	void	BindToPanel(IPaneMixIn &panel, const size_t retry_ms) override
	{
		if (m_Binder)	return;		// already bound
		
		Rebind(panel.GetWxWindow(), retry_ms);
	}
	
	void	DaemonLog(const timestamp_t stamp, const LogLevel level, const string &s) override
	{
		const size_t	d_us = stamp.delta_us(m_LastDaemonStamp);
		m_LastDaemonStamp = stamp;
		
		m_ListModel.LogEntry(stamp, level, s, 0/*thread index*/, true/*daemon?*/, d_us);
	}
	
private:
	
	void	Rebind(wxWindow *win, const size_t retry_ms)
	{
		assert(win);
		
		if (!IjwxWindowBinder::IsBindable(*win))
		{	
			// not yet bindable, retry later
			MixDelay("", retry_ms, this, &LogComponent::Rebind, win, retry_ms);
			return;
		}
		
		m_Binder.reset(IjwxWindowBinder::Create(*win, *this, AUTO_FOCUS_T::WX, "Log"));
		
		uLog(REALIZED, "LogComponent::Rebind() ok");
		
		m_ListModel.Refresh();
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

	TableListBox			m_ListCtrl;
	LogListModel			m_ListModel;
	
	timestamp_t			m_LastDaemonStamp;
	
	unique_ptr<IjwxWindowBinder>	m_Binder;
};

//---- INSTANTIATE ------------------------------------------------------------

// static
ILogOutput*	ILogOutput::Create(shared_ptr<IFixedFont> ff)
{
	jAutoMsgPause	lock;
		
	uLog(APP_INIT, "ILogOutput::Create()");
	
	return new LogComponent(ff);
}

// nada mas
