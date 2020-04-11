// Lua DDT bottom Notebook

#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <sstream>
#include <memory>		// for unique_ptr

#include "wx/dnd.h"
#include "wx/cursor.h"
#include "wx/confbase.h"
#include "wx/image.h"
#include "wx/statbmp.h"

#include "deffile.xpm"

#include "logImp.h"
#include "TopFrame.h"
#include "Controller.h"
#include "UIConst.h"

#include "TopNotebook.h"
#include "BottomNotebook.h"

using namespace std;
using namespace DDT_CLIENT;

const int	PANEBOOK_SPLITTERS_MAX			= 2;

enum PANEBOOK_INDEX : int
{
	PANEBOOK_INDEX_TOP			= 0,				// (top-right)
	PANEBOOK_INDEX_BOTTOM_LEFT		= 1,
	PANEBOOK_INDEX_BOTTOM_RIGHT		= 2
};

const auto	PANEBOOK_INDEX_NON_HIDABLE = PANEBOOK_INDEX_BOTTOM_LEFT;

static const
unordered_map<int, int>	s_ShiftPaneMap
{
	{PANEBOOK_INDEX_BOTTOM_LEFT,	PANEBOOK_INDEX_BOTTOM_RIGHT},
	{PANEBOOK_INDEX_BOTTOM_RIGHT,	PANEBOOK_INDEX_TOP},
	{PANEBOOK_INDEX_TOP,		PANEBOOK_INDEX_BOTTOM_LEFT}
};

const int	PANE_NOT_FOUND				= -1;
const int	PANE_ADD_TO_BACK			= 2001;
const int	PANE_KEEP_AS_IS				= 2002;

enum class SPLIT_T : int
{
	SET = 0,
	DRAG_START,
	DRAG_END,
	UPDATE
};

class NotebookSignals
{
public:
	Signal<PANE_ID>			OnNotepageShown;
};

struct page_init
{
	page_init(const int win_id, const string &name)
		: m_WinID(win_id), m_Name(name)
	{
	}
	
	page_init(const page_init&) = default;
	page_init& operator=(const page_init&) = default;
	
	int	m_WinID;
	string	m_Name;
};

static const
unordered_map<PANE_ID, page_init, EnumHash>	s_PageIDtoInit
{
	{PANE_ID_OUTPUT,		{PANEL_ID_OUTPUT_TEXT_CTRL,		"Output"}},
	{PANE_ID_PROJECT,		{PANEL_ID_PROJECT_LIST,			"Project"}},
	{PANE_ID_LOCALS,		{PANEL_ID_LOCALS_LIST_CONTROL,		"Locals"}},
	{PANE_ID_GLOBALS,		{PANEL_ID_GLOBALS_LIST_CONTROL,		"Globals"}},
	{PANE_ID_WATCHES,		{PANEL_ID_WATCH_LIST_CONTROL,		"Watches"}},
	{PANE_ID_STACK,			{PANEL_ID_TRACE_BACK_LIST_CONTROL,	"Stack"}},
	{PANE_ID_BREAKPOINTS,		{PANEL_ID_BREAKPOINTS_LIST_CONTROL,	"Breakpoints"}},
	{PANE_ID_MEMORY,		{PANEL_ID_MEMORY_VIEW_CONTROL,		"Memory"}},
	{PANE_ID_SEARCH,		{PANEL_ID_SEARCH_RESULTS,		"Searched"}},
	{PANE_ID_LUA_FUNCTIONS,		{PANEL_ID_LUA_FUNCTIONS_CONTROL,	"Functions"}},
	{PANE_ID_JUCE_LOCALS,		{PANEL_ID_JUCE_LOCALS_LIST_CONTROL,	"jLocals"}},
};

// ! redundant -- could collapse? (do pane IDs need to be sequential?)
static const
unordered_map<int, PANE_ID>	s_WinToPaneID
{
	{PANEL_ID_OUTPUT_TEXT_CTRL,		PANE_ID_OUTPUT},
	{PANEL_ID_PROJECT_LIST,			PANE_ID_PROJECT},
	{PANEL_ID_LOCALS_LIST_CONTROL,		PANE_ID_LOCALS},
	{PANEL_ID_GLOBALS_LIST_CONTROL,		PANE_ID_GLOBALS},
	{PANEL_ID_WATCH_LIST_CONTROL,		PANE_ID_WATCHES},
	{PANEL_ID_TRACE_BACK_LIST_CONTROL,	PANE_ID_STACK},
	{PANEL_ID_BREAKPOINTS_LIST_CONTROL,	PANE_ID_BREAKPOINTS},
	{PANEL_ID_MEMORY_VIEW_CONTROL,		PANE_ID_MEMORY},
	{PANEL_ID_SEARCH_RESULTS,		PANE_ID_SEARCH},
	{PANEL_ID_LUA_FUNCTIONS_CONTROL,	PANE_ID_LUA_FUNCTIONS},
	{PANEL_ID_JUCE_LOCALS_LIST_CONTROL,	PANE_ID_JUCE_LOCALS},
};

static
string	GetPaneIDName(const PANE_ID id)
{
	return s_PageIDtoInit.count(id) ? s_PageIDtoInit.at(id).m_Name : string("<ILLEGAL>");
}

static
string	GetPaneIDName(const int id)
{
	return GetPaneIDName((const PANE_ID)id);
}

class SingleNotebook;

//---- Pege Info --------------------------------------------------------------

class DDT_CLIENT::PageInfo
{
public:
	// CTOR
	PageInfo(BottomNotePanel &parent);
	virtual ~PageInfo();
	
	bool		IsUsed(void) const	{return (m_PageIndex >= 0) && (m_CurrentBook != nil);}
	wxWindow*	GetWin(void) const;
	
	IPaneMixIn*	GetMixIn(void) const	{return m_MixIn;}		// may be nil if is not shown / has no associated window
	void		SetMixIn(IPaneMixIn *mix)
	{
		m_MixIn = mix;
	}
	
	bool	IsStale(void) const	{return (nil == m_MixIn);}
	void	RemoveBook(void)
	{
		m_CurrentBook = nil;
		m_PageIndex = -1;
	}

	SingleNotebook*	GetBook(void) const
	{
		return m_CurrentBook;	// may be nil
	}

	int	GetBookID(void) const;

	int	GetPos(void) const	{return m_PageIndex;}	// may be -1
	
	void	SetBook(const int book_index, const int page_index);
	
private:

	BottomNotePanel		&m_Parent;
	
	IPaneMixIn		*m_MixIn;
	SingleNotebook		*m_CurrentBook;
	int			m_PageIndex;
};

//---- Single Notebook --------------------------------------------------------

class SingleNotebook: public wxPanel
{
public:	
	SingleNotebook(const int book_index, wxWindow *parent, const int styl, BottomNotePanel &bnp, TopFrame &tf);
	virtual ~SingleNotebook();
	
	void	ReIndex(void);
	
	int	AddPageID(const PANE_ID page_id, const int at_pos = PANE_ADD_TO_BACK);		// returns new index
	bool	RemovePageFromBook(const PANE_ID page_id, const bool light_f);			// returns [was_removed]
	bool	SelectPageID(const PANE_ID page_id);
	PANE_ID	GetSelectedPageID(void) const;
	int	GetBookID(void) const;
	int	DragHitTest(const PANE_ID drag_page_id, const wxPoint &global_pos);
	
	void	PrepareBitmap(void);
	void	HighLightEmpty(void);
	void	SetHighlight(const bool f);
	bool	HadHighlight(void) const;
	
	bool		HasPages(void) const	{return (GetNumPages() > 0);}


	size_t			GetNumPages(void) const;
	vector<PANE_ID>		GetPageList(void) const;
	
	NotebookSignals&	GetSignals(void)	{return m_Signals;}
		
private:
	
	PANE_ID	PagePosToID(const int pos) const;
	const IPaneMixIn*	PagePosToMixIn(const int page_index);
	
	void	OnRightDown(wxMouseEvent &e);
	void	OnNotePageChanged(wxNotebookEvent &e);
	
	BottomNotePanel	&m_Parent;
	const int	m_BookID;
	
	wxNotebook	m_Notebook;
	wxStaticBitmap	m_StatBitmap;
	
	wxSizer		*m_VSizer;
	vector<PANE_ID>	m_PageIDList;
	
	NotebookSignals	m_Signals;
};

//=============================================================================

// static
vector<wxWindow*>	BookSplitter::s_WindowList;

//---- CTOR -------------------------------------------------------------------

	PaneMixIn::PaneMixIn(IBottomNotePanel &bnp, const int win_id, wxWindow *win)
		: m_Parent(bnp),
		m_ClientWin(win),
		m_PageID(bnp.RegisterPage(win_id, this))		// CIRCULAR REFERENCE ?
{
	assert(win);
	
	win->SetClientData(this);
}

//---- DTOR -------------------------------------------------------------------

	PaneMixIn::~PaneMixIn()
{
	uLog(DTOR, "PaneMixIn::DTOR");
	
	// m_Parent.UnregisterPage(this);		// crashes
	
	// m_ClientWin->SetClientData(nil);
}

bool	PaneMixIn::IsPaneShown(void) const
{
	const PageInfo	&info = m_Parent.GetPageInfo(m_PageID);
	if (!info.IsUsed())	return false;		// not used
	
	const SingleNotebook	*book = info.GetBook();
	assert(book);
	
	const int	selected_id = book->GetSelectedPageID();
	return (selected_id == m_PageID);
}

//=============================================================================

	PageInfo::PageInfo(BottomNotePanel &parent)
		: m_Parent(parent), 
		m_MixIn(nil), m_CurrentBook(nil), m_PageIndex(-1)
{	
}

	PageInfo::~PageInfo()
{
	uLog(DTOR, "PageInfo::DTOR");
}

wxWindow*	PageInfo::GetWin(void) const
{
	if (!m_MixIn)	return nil;
	
	wxWindow	*win = m_MixIn->GetWxWindow();
	// may NOT be nil
	
	return win;
}

int	PageInfo::GetBookID(void) const
{
	if (!m_CurrentBook)		return -1;
	
	return m_CurrentBook->GetBookID();
}

//---- Bottom Notepanel -------------------------------------------------------

class DDT_CLIENT::BottomNotePanel: public wxPanel, public IBottomNotePanel, private SlotsMixin<CTX_WX>
{
	friend class SingleNotebook;
	friend class PageInfo;
	friend class PaneMixIn;
public:
	BottomNotePanel(wxWindow *parent, int id, ITopNotePanel &top_notepanel, TopFrame &tf, Controller &controller);
	virtual ~BottomNotePanel();
	
	wxWindow*	GetWxWindow(void) override	{return this;}
	
	NotepageSignals&	GetSignals(const PANE_ID page_id) override
	{
		// return m_DynSigs.Get((int)page_id);
		const size_t	i = (size_t)page_id;
		
		assert(i < m_ArraySigs.size());
		
		return m_ArraySigs.at(i);
	}
	
	Controller&	GetController(void) override	{return m_Controller;}
	
	
	PANE_ID	RegisterPage(const int win_id, IPaneMixIn *page_mixin) override;		// returns [page_id]
	void	UnregisterPage(IPaneMixIn *page_mixin) override;
	
	void	DestroyDanglingPanes(void) override;
	
	void	LoadUI(const wxConfigBase &config) override;
	bool	SaveUI(wxConfigBase &config) override;
	
	void	ClearVarViews(void) override;
	bool	RaisePageID(const PANE_ID page_id, const bool reload_f) override;
	void	UpdateVisiblePages(void) override;
	
	bool	IsNotePaneShown(const PANE_ID page_id) const override
	{
		const PageInfo	&pinfo = GetPageInfo(page_id);
		
		return pinfo.IsUsed();
	}
	
	void	ResetNotesPanes(void) override;
	
	void	PreDragContextMenu(const PANE_ID selected_pane_id, const int book_id);
	const PageInfo&	GetPageInfo(const PANE_ID id) const override
	{
		assert(m_PageInfoMap.count(id));
		return m_PageInfoMap.at(id);
	}
	
	PageInfo&	GetPageInfoRW(const PANE_ID id) override
	{
		assert(m_PageInfoMap.count(id));
		return m_PageInfoMap.at(id);
	}
	
private:
	
	void	AddPageToNotebook(const PANE_ID page_id, const int notebook_id, const int at_pos = PANE_ADD_TO_BACK);
	void	RemovePageID(const PANE_ID page_id);
	
	vector<int>	GetSplitterWidths(void) const;
	vector<int>	GetDragSplitterWidths(void) const;
	void	RepositionPaneBooks(const SPLIT_T &mode, const vector<int> &splitter_widths);
	
	SingleNotebook*	GetBook(const int book_id) const
	{
		assert((book_id >= 0) && (book_id < m_Books.size()));
		
		return m_Books[book_id];
	}
	
	void	OnChildPageEmerged(PANE_ID pane_id);
	
	bool	IsDraggingPane(void) const;
	void	ReIndexPages(void);
	void	UpdateOnePageID(const PANE_ID page_id);
	
	void	OnMouseDragMove(wxMouseEvent &e);
	void	OnLeftMouseClick(wxMouseEvent &e);
	
	TopFrame			&m_TopFrame;
	Controller			&m_Controller;
	
	wxWindow			*m_TopPanel;
	BookSplitter			*m_SplitterTop, *m_SplitterBottom;
	
	vector<SingleNotebook*>		m_Books;
	
	unordered_map<int, PANE_ID>	m_WinToPageID;
	unordered_map<string, PANE_ID>	m_PageNameToID;
	
	unordered_map<PANE_ID, PageInfo, EnumHash>	m_PageInfoMap;
	
	PANE_ID				m_DraggedPageID;
	wxPoint				m_LastMousePos;
	int				m_LastHitBook;
	int				m_LastHitPagePos;
	wxCursor			m_DragCursor;
	
	array<NotepageSignals, (size_t)PANE_ID_MAX>		m_ArraySigs;
	
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(BottomNotePanel, wxPanel)

	EVT_MOTION(						BottomNotePanel::OnMouseDragMove)
	EVT_LEFT_DOWN(						BottomNotePanel::OnLeftMouseClick)
	
END_EVENT_TABLE()

void	PageInfo::SetBook(const int book_id, const int page_index)
{
	assert((book_id >= 0) && (book_id < m_Parent.m_Books.size()));
	
	m_CurrentBook = m_Parent.m_Books[book_id];
	
	m_PageIndex = page_index;
}

//---- Bottom Notepanel CTOR --------------------------------------------------

	BottomNotePanel::BottomNotePanel(wxWindow *parent, int id, ITopNotePanel &top_notepanel, TopFrame &tf, Controller &controller)
		: wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_SUNKEN),
		m_TopFrame(tf), m_Controller(controller),
		m_DragCursor{wxImage(deffile_xpm)}
{
	m_SplitterTop = top_notepanel.GetSplitterTop();
	assert(m_SplitterTop);
	
	m_TopPanel = m_SplitterTop->GetWindow1();
	
	m_SplitterBottom = new BookSplitter(this, SPLITTER_ID_BOTTOM, 1);
	
	m_DraggedPageID = PANE_ID_NONE;
	m_WinToPageID.clear();
	m_PageNameToID.clear();
	
	for (const auto &it : s_PageIDtoInit)
	{
		const PANE_ID	page_id = it.first;
		const page_init	pinit = it.second;
		
		m_WinToPageID.insert({pinit.m_WinID, page_id});
		m_PageNameToID.insert({pinit.m_Name, page_id});
		
		// dynamically PRE-ALLOCATE a signal specific for this notepage ID ?			// but won't get DTORed - FIXME
		// m_DynSigs.Add(page_id);
	}
	
	m_Books.clear();
	
	const int	book_styl = wxNB_TOP;			// wxNB_BOTTOM (fixed width only works on Windows)
	
	m_Books.push_back(new SingleNotebook(0, m_SplitterTop, book_styl, *this, tf));
	m_Books.push_back(new SingleNotebook(1, m_SplitterBottom, book_styl, *this, tf));
	m_Books.push_back(new SingleNotebook(2, m_SplitterBottom, book_styl, *this, tf));
	
	BookSplitter::s_WindowList = {m_TopPanel, m_Books[0], m_Books[1], m_Books[2]};
	
	/*
	m_Books[0]->SetBackgroundColour(*wxGREEN);
	m_Books[1]->SetBackgroundColour(*wxYELLOW);
	m_Books[2]->SetBackgroundColour(*wxRED);
	m_Books[3]->SetBackgroundColour(wxColour(0x40, 0xa0, 0xff));
	*/
	
	wxBoxSizer	*v_sizer = new wxBoxSizer(wxHORIZONTAL);
	
	v_sizer->Add(m_SplitterBottom, wxSizerFlags(1).Border(wxALL, 2).Expand());
	
	SetSizer(v_sizer);
	
	SetDropTarget(new ControllerDropTarget(controller));
	
	for (const auto &it: s_PageIDtoInit)
	{	
		const PANE_ID	pane_id = it.first;
		// const page_init	pinit = it.second;
		
		m_PageInfoMap.emplace(pane_id, PageInfo(*this));
	}
	
	controller.SetBottomNotePanel(this);
	
	MixConnect(this, &BottomNotePanel::OnChildPageEmerged, m_Books[0]->GetSignals().OnNotepageShown);
	MixConnect(this, &BottomNotePanel::OnChildPageEmerged, m_Books[1]->GetSignals().OnNotepageShown);
	MixConnect(this, &BottomNotePanel::OnChildPageEmerged, m_Books[2]->GetSignals().OnNotepageShown);
}

//---- DTOR -------------------------------------------------------------------

	BottomNotePanel::~BottomNotePanel()
{
	uLog(DTOR, "BottomNotePanel::DTOR");
}

//---- Destroy Dangling/Orphan Panes ------------------------------------------

void	BottomNotePanel::DestroyDanglingPanes(void)
{
	uLog(DTOR, "BottomNotePanel::DestroyDanglingPanes()");
	
	// manually delete either all panes / or only non-hosted panes
	for (int i = 0; i < PANE_ID_MAX; i++)
	{
		const PANE_ID	id = (PANE_ID)i;
		
		const string	name = GetPaneIDName(id);
		
		const PageInfo	&pinfo = GetPageInfo(id);
		
		const bool	used_f = pinfo.IsUsed();
		
		uLog(DTOR, "  pane %S is %s", name, used_f ? "still used" : "UNUSED");
		assert(!pinfo.IsStale());
		
		// NOTE: works better if ignores used panes, since may trigger events
		if (used_f)
		{	// remove from NotePane
			//   RemovePageID(i);
			// ... then destroy
			continue;
		}
		
		wxWindow	*win = pinfo.GetWin();
		assert(win);
		
		win->Destroy();
	}
}

//---- Save UI ----------------------------------------------------------------

bool	BottomNotePanel::SaveUI(wxConfigBase &config)
{
	uLog(UI, "BottomNotePanel::SaveUI()");
	
	if (IsDraggingPane())		return false;		// ignore while dragging
	if (IsBeingDeleted())		return false;		// ignore during DTOR
	
	config.Write("/Window/topbook_left", false);
	
	uLog(UI, "  save splitter widths");
	
	const vector<int>	splitter_widths = GetSplitterWidths();
	
	for (int i = 0; i < PANEBOOK_SPLITTERS_MAX; i++)
	{	const wxString	key = wxString::Format("/Window/splitter%d", i);
		const int	w = splitter_widths.at(i);
		
		config.Write(key, w);
	}
	
	// keep sorted so comes out the same
	ostringstream	ss(ios_base::trunc);
	
	for (SingleNotebook *bnb : m_Books)
	{	
		const int		book_id = bnb->GetBookID();
		const vector<PANE_ID>	page_list = bnb->GetPageList();
		
		for (const int id : page_list)
			ss << " " << GetPaneIDName(id) << " " << book_id;		// (prepend space)
	}
	
	const wxString	buff_s {ss.str()};
	
	config.Write("/Window/page_list", buff_s);
	
	return true;
}

//---- Load UI ----------------------------------------------------------------

void	BottomNotePanel::LoadUI(const wxConfigBase &config)
{
	uLog(UI, "BottomNotePanel::LoadUI()");
	
	vector<int>	splitter_widths;
	
	for (int i = 0; i < PANEBOOK_SPLITTERS_MAX; i++)
	{	const wxString	key = wxString::Format("/Window/splitter%d", i);
		
		long	w;
		
		config.Read(key, &w, 500);
		
		splitter_widths.push_back(w);
	}
	
	// default buffer if no pref found (all goes to book1
	wxString	default_buff;
	
	for (int i = 0; i < PANE_ID_MAX; i++)
		default_buff << " " << GetPaneIDName(i) << " " << 1;			// (prepend space)
	
	wxString	buff;
	
	config.Read("/Window/page_list", &buff, default_buff);
	
	istringstream	iss(buff.ToStdString(), ios_base::in);
	
	string	page_name;
	int	book_id;
		
	for (int i = 0; !iss.fail() && !iss.eof(); i++)
	{
		iss >> page_name >> book_id;
		assert(!page_name.empty());
		assert((book_id >= 0) && (book_id < PANEBOOK_MAX));
		
		if (!m_PageNameToID.count(page_name))
		{
			uLog(WARNING, "ignoring unknown pane name %S", page_name);
			continue;
		}
		
		const PANE_ID	page_id = m_PageNameToID.at(page_name);
		// no dupes
		assert(!GetPageInfo(page_id).IsUsed());
		
		AddPageToNotebook(page_id, book_id);
	}
	
	RepositionPaneBooks(SPLIT_T::SET, splitter_widths);
	
	ReIndexPages();
}

//---- Reset Notes Panes ------------------------------------------------------

void	BottomNotePanel::ResetNotesPanes(void)
{
	uLog(WARNING, "BottomNotePanel::ResetNotesPanes() -- kills juce bindings !!!");
	
	// removes ALL, then re-adds
	for (int i = 0; i < PANE_ID_MAX; i++)
	{
		const PANE_ID	id = (PANE_ID)i;
		if (!GetPageInfo(id).IsUsed())		continue;
		
		RemovePageID(id);
	}
	
	for (int i = 0; i < PANE_ID_MAX; i++)
	{
		const PANE_ID	id = (PANE_ID)i;
		
		AddPageToNotebook(id, PANEBOOK_INDEX_BOTTOM_LEFT);
	}
	
	RepositionPaneBooks(SPLIT_T::UPDATE, GetSplitterWidths());
	
	ReIndexPages();
}

//---- Get Splitter Widths ----------------------------------------------------

vector<int>	BottomNotePanel::GetSplitterWidths(void) const
{
	assert(m_SplitterBottom);
	assert(m_SplitterTop);
	assert(m_Books[0]);
	assert(m_Books[2]);
	
	const int	client_w = m_SplitterTop->GetFullWidth();
	
	if (m_Books[0]->HasPages() && !m_SplitterTop->IsSplit())
	{
		m_SplitterTop->Split(m_TopPanel, m_Books[0], client_w / 5);
	}
	
	if (m_Books[2]->HasPages() && !m_SplitterBottom->IsSplit())
	{
		m_SplitterBottom->Split(m_Books[1], m_Books[2], client_w / 2);
	}
	
	vector<int>	res;
	
	res.push_back(m_SplitterTop->GetPos());
	res.push_back(m_SplitterBottom->GetPos());
	
	return res;
}

//---- Get (full) Drag Splitter Width -----------------------------------------

vector<int>	BottomNotePanel::GetDragSplitterWidths(void) const
{
	assert(m_SplitterBottom);
	assert(m_SplitterTop);
	assert(m_Books[0]);
	assert(m_Books[2]);
	
	const int	client_w = m_SplitterTop->GetFullWidth();
		
	vector<int>	res;
	
	if (m_SplitterTop->IsSplit())		res.push_back(m_SplitterTop->GetPos());
	else					res.push_back(client_w / 5);
	
	if (m_SplitterBottom->IsSplit())	res.push_back(m_SplitterBottom->GetPos());
	else					res.push_back(client_w / 2);
	
	return res;
}

//---- Reposition Pane Books --------------------------------------------------

void	BottomNotePanel::RepositionPaneBooks(const SPLIT_T &mode, const vector<int> &splitter_widths)
{
	uLog(PANE, "BottomNotePanel::RepositionPaneBooks()");
	
	assert(m_SplitterTop);
	assert(m_SplitterBottom);
	
	assert(splitter_widths.size() == PANEBOOK_SPLITTERS_MAX);
	
	switch (mode)
	{
		case SPLIT_T::DRAG_START:
		
			m_SplitterTop->Split(m_TopPanel, m_Books[0], splitter_widths[0], false);
			m_SplitterBottom->Split(m_Books[1], m_Books[2], splitter_widths[1]);
		
			m_Books[0]->PrepareBitmap();
			m_Books[2]->PrepareBitmap();
		
			m_Books[0]->HighLightEmpty();
			m_Books[2]->HighLightEmpty();
			break;
	
		case SPLIT_T::DRAG_END:
	
			m_Books[0]->SetHighlight(false);
			m_Books[2]->SetHighlight(false);
		
			// fall-through!
	
		case SPLIT_T::SET:
		case SPLIT_T::UPDATE:
		{
			if (m_Books[0]->HasPages() && !m_SplitterTop->IsSplit())
				m_SplitterTop->Split(m_TopPanel, m_Books[0], splitter_widths[0], false); 
			else
			{	m_SplitterTop->Split(m_TopPanel, nil, splitter_widths[0]);
				m_Books[0]->Hide();
			}
		
			if (m_Books[2]->HasPages() && !m_SplitterBottom->IsSplit())
				m_SplitterBottom->Split(m_Books[1], m_Books[2], splitter_widths[1]);
			else
			{	m_SplitterBottom->Split(m_Books[1], nil, splitter_widths[1]);
				m_Books[2]->Hide();
			}
		}	break;
	}
}

//---- Is Dragging Pane ? -----------------------------------------------------

bool	BottomNotePanel::IsDraggingPane(void) const
{
	return (m_DraggedPageID != -1);
}

//---- Register SingleNotebook ------------------------------------------------------

	// returns [page_id]

PANE_ID	BottomNotePanel::RegisterPage(const int win_id, IPaneMixIn *page_mixin)
{
	assert(page_mixin);
	
	assert(m_WinToPageID.count(win_id));
	const PANE_ID	page_id = m_WinToPageID.at(win_id);
	
	const string	name = GetPaneIDName(page_id);
	
	GetPageInfoRW(page_id).SetMixIn(page_mixin);
	
	uLog(PANE, "registered Notepage(%S, %d)", name, page_id);
	
	wxWindow	*win = page_mixin->GetWxWindow();
	assert(win);
	
	// orphan by default, otherwise stays parent of (default) BottomNotePanel
	win->Reparent(nil);
	win->Hide();
	
	return (PANE_ID) page_id;
}

//---- Unregister Page --------------------------------------------------------

void	BottomNotePanel::UnregisterPage(IPaneMixIn *page_mixin)
{	
	uLog(PANE, "BottomNotePanel::UnregisterPage()");
	assert(page_mixin);
	
	const PANE_ID	page_id = page_mixin->GetPageID_LL();
	
	const string	name = GetPaneIDName(page_id);
	
	uLog(PANE, "  PaneMixIn::DTOR(%S)", name);
	uLog(PANE, "  page_id %d", page_id);
	
	// make it stale
	GetPageInfoRW(page_id).SetMixIn(nil);
}

//---- Add Page To Notebook ---------------------------------------------------

void	BottomNotePanel::AddPageToNotebook(const PANE_ID page_id, const int book_index, const int at_pos)
{
	uLog(PANE, "BottomNotePanel::AddPageToNotebook()");
	
	SingleNotebook	*book = m_Books.at(book_index);
	assert(book);
	
	book->AddPageID(page_id, at_pos);
}

//---- Remove Page ID ---------------------------------------------------------

void	BottomNotePanel::RemovePageID(const PANE_ID page_id)
{
	uLog(PANE, "BottomNotePanel::RemovePageID()");
	
	const PageInfo	&pinfo = GetPageInfo(page_id);
	if (!pinfo.IsUsed())		return;			// silent return - should be error?
	
	SingleNotebook	*book = pinfo.GetBook();
	assert(book);
	
	book->RemovePageFromBook(page_id, false/*light?*/);
}

//---- Reindex Pages ----------------------------------------------------------

void	BottomNotePanel::ReIndexPages(void)
{
	uLog(PANE, "BottomNotePanel::ReIndexPages()");
	
	for (SingleNotebook *book : m_Books)		book->ReIndex();
}

//---- Update Visible Pages ---------------------------------------------------

void	BottomNotePanel::UpdateVisiblePages(void)
{
	uLog(PANE, "BottomNotePanel::UpdateVisiblePages()");
	
	ReIndexPages();
	
	for (SingleNotebook *book : m_Books)
	{
		const PANE_ID	selected_id = book->GetSelectedPageID();
		if (selected_id == PANE_ID_NONE)	continue;
		
		UpdateOnePageID(selected_id);
	}
}

//---- Reload Selected Page ID ------------------------------------------------

void	BottomNotePanel::UpdateOnePageID(const PANE_ID page_id)
{
	uLog(PANE, "BottomNotePanel::UpdateOnePageID(%s)", GetPaneIDName(page_id));
	
	// trigger notebook-specific signal (doesn't reach others)
	GetSignals(page_id).OnPanelRequestReload(CTX_WX);
}

//---- On Child Notepane Emerged ----------------------------------------------

void	BottomNotePanel::OnChildPageEmerged(PANE_ID page_id)
{
	uLog(PANE, "BottomNotePanel::OnChildPageEmerged(%s)", GetPaneIDName(page_id));
	
	UpdateOnePageID(page_id);
}

//---- Pre-Drag Context Menu --------------------------------------------------

void	BottomNotePanel::PreDragContextMenu(const PANE_ID selected_pane_id, const int book_id)
{
	const string	pane_name = GetPaneIDName(selected_pane_id);
	uLog(PANE, "context menu with selected pane %S", pane_name);
	
	assert(s_ShiftPaneMap.count(book_id));
	
	SingleNotebook	*book = GetBook(book_id);
	
	// find / add hidden panes to submenu
	vector<PANE_ID>	hidden_panes;
	
	for (int i = 0; i < PANE_ID_MAX; i++)
	{
		const PANE_ID	id = (PANE_ID)i;
		
		const PageInfo	&pinfo = GetPageInfo(id);
		if (pinfo.IsUsed() || (selected_pane_id == id))	continue;
		
		hidden_panes.push_back(id);
	}
	
	wxMenu	menu;
	
	menu.Append(POPUP_MENU_ID_CLOSE_PANE, "close");
	menu.Append(POPUP_MENU_ID_DRAG_PANE, "drag");
	menu.Append(POPUP_MENU_ID_SHIFT_PANE_BOOK, "shift");
	
	// prevent book1 from going empty
	if ((book_id == PANEBOOK_INDEX_NON_HIDABLE) && (m_Books[book_id]->GetNumPages() == 1))
	{	menu.Enable(POPUP_MENU_ID_DRAG_PANE, false);
		menu.Enable(POPUP_MENU_ID_CLOSE_PANE, false);
	}
	
	unordered_map<int, PANE_ID>	FakeToPaneIDs;
	
	if (!hidden_panes.empty())
	{	// apparently worth the trouble (?)
		menu.AppendSeparator();
		wxMenu	*submenu = new wxMenu();
		
		for (const auto &id : hidden_panes)
		{
			const int	fake_id = PANE_ID_BODY_DOUBLE_START + FakeToPaneIDs.size();
		
			FakeToPaneIDs.emplace(fake_id, id);
			
			submenu->Append(fake_id, GetPaneIDName(id));
		}
		
		menu.AppendSubMenu(submenu, "show");
	}
	
	// immediate popup menu (sends FOCUS event)
	const int	menu_id = GetPopupMenuSelectionFromUser(menu);
	if (menu_id < 0)	return;		// canceled
	
	// hide selected pane	
	if (menu_id == POPUP_MENU_ID_CLOSE_PANE)
	{	RemovePageID(selected_pane_id);
		RepositionPaneBooks(SPLIT_T::UPDATE, GetSplitterWidths());
		m_TopFrame.DirtyUIPrefs();
		return;
	}
	
	if (menu_id == POPUP_MENU_ID_SHIFT_PANE_BOOK)
	{
		const int	new_book_id = s_ShiftPaneMap.at(book_id);
		
		uLog(PANE, "shift book %d -> %d", book_id, new_book_id);
		
		book->RemovePageFromBook(selected_pane_id, true/*light?*/);
		
		AddPageToNotebook(selected_pane_id, new_book_id);
		RepositionPaneBooks(SPLIT_T::UPDATE, GetSplitterWidths());
		m_TopFrame.DirtyUIPrefs(true/*immediate*/);
		return;
	}
	
	if (FakeToPaneIDs.count(menu_id))
	{	// drag a previously invisible pane
		m_DraggedPageID = FakeToPaneIDs.at(menu_id);
	}
	else if (menu_id == POPUP_MENU_ID_DRAG_PANE)
	{	// drag selected pane
		m_DraggedPageID = selected_pane_id;
	}
	else
	{	m_DraggedPageID = PANE_ID_NONE;
		uErr("illegal drag pane");
		return;
	}
	
// start drag

	m_LastHitBook = m_LastHitPagePos = -1;
	
	// show all books during drag
	RepositionPaneBooks(SPLIT_T::DRAG_START, GetDragSplitterWidths());
	
	// DO NEED mouse capture!
	CaptureMouse();
	
	// change cursor to "drop-like" image
	wxSetCursor(m_DragCursor);
}

//--- On Mouse Move/Drag ------------------------------------------------------

void	BottomNotePanel::OnMouseDragMove(wxMouseEvent &e)
{
	if (!IsDraggingPane())			return;
	// if (!e.LeftIsDown())			return;
	
	const wxPoint	pt = ::wxGetMousePosition();		// global coords
	if (pt == m_LastMousePos)		return;
	m_LastMousePos = pt;
	
	int	hit_book = -1, hit_page_pos = -1;
	
	for (SingleNotebook *book : m_Books)
	{	
		hit_page_pos = book->DragHitTest(m_DraggedPageID, pt);
		if (PANE_NOT_FOUND == hit_page_pos)		continue;
		if (PANE_KEEP_AS_IS == hit_page_pos)		return;
		
		hit_book = book->GetBookID();
		break;
	}
	
	if ((hit_book == m_LastHitBook) && (hit_page_pos == m_LastHitPagePos))		return;
	m_LastHitBook = hit_book;
	m_LastHitPagePos = hit_page_pos;
	
	// remove from old book
	RemovePageID(m_DraggedPageID);
	
	if (-1 != hit_book)
	{	/*
		// move page to notebook
		AddPageToNotebook(m_DraggedPageID, hit_book, hit_page_pos);
		// doesn't work
		//   need to reindex?
		//   need low-level function to update?
		RaisePageID((PANE_ID)m_DraggedPageID, false);
		*/
		
		SingleNotebook	*book = m_Books.at(hit_book);
		assert(book);
		book->AddPageID(m_DraggedPageID, hit_page_pos);
		book->SelectPageID(m_DraggedPageID);
		
		book->HighLightEmpty();
		
		// int	sel_id = book->GetSelectedPageID();
		// uLog(UI, "select %d, selected %d", m_DraggedPageID, sel_id);
	}
	
	m_Books[0]->HighLightEmpty();
	m_Books[2]->HighLightEmpty();
}

//---- On Left Click / Drag Done ----------------------------------------------

void	BottomNotePanel::OnLeftMouseClick(wxMouseEvent &e)
{
	if (-1 == m_DraggedPageID)		return e.Skip();	// wasn't dragging
	m_DraggedPageID = PANE_ID_NONE;
	
	// if (HasCapture())	ReleaseMouse();
	ReleaseMouse();
	
	wxSetCursor(wxNullCursor);
	
	// hide empty books
	RepositionPaneBooks(SPLIT_T::DRAG_END, GetDragSplitterWidths());
	
	m_TopFrame.DirtyUIPrefs();
}

//---- Raise Page ID ----------------------------------------------------------

bool	BottomNotePanel::RaisePageID(const PANE_ID page_id, const bool reload_f)
{
	const PageInfo	&pinfo = GetPageInfo(page_id);
	if (!pinfo.IsUsed())		return false;
	
	SingleNotebook	*book = pinfo.GetBook();
	assert(book);
	
	// lower-level handles only visual, not logic?
	bool	f = book->SelectPageID(page_id);
	if (!f)		return false;
	
	if (reload_f)
		UpdateOnePageID(page_id);		// risks infinite loops?
	else	Refresh();

	return true;
}

//---- Clear Bottom SingleNotebook --------------------------------------------------

void	BottomNotePanel::ClearVarViews(void)
{
	for (SingleNotebook *book : m_Books)
	{
		const PANE_ID	selected_id = book->GetSelectedPageID();
		if (selected_id == PANE_ID_NONE)	continue;
		
		GetSignals(selected_id).OnPanelClearVariables(CTX_WX);
	}
}

//---- CTOR -------------------------------------------------------------------

	SingleNotebook::SingleNotebook(const int nb_index, wxWindow *parent, const int styl, BottomNotePanel &bnp, TopFrame &tf)
		: wxPanel(parent, CONTROL_ID_PANE_BOOK_BASE + nb_index, wxDefaultPosition, wxSize(0, 0)),
		m_Parent(bnp),
		m_BookID(nb_index),
		m_Notebook(this, CONTROL_ID_BOTTOM_NOTEBOOK_BASE + nb_index, wxDefaultPosition, wxSize(0, 0), styl | wxSUNKEN_BORDER),
		m_StatBitmap(this, CONTROL_ID_PANEBOOK_STATIC_BITMAP, wxBitmap(), wxDefaultPosition, wxSize(0, 0))
{
	SetLabel(wxString("notebook") + to_string(nb_index));
	
	m_Notebook.SetPadding(wxSize(0, 0));
	
	m_VSizer = new wxBoxSizer(wxVERTICAL);
	
	m_VSizer->Add(&m_Notebook, wxSizerFlags(1).Expand());
	m_VSizer->Add(&m_StatBitmap, wxSizerFlags(1).Expand());
	
	SetSizer(m_VSizer);
	
	m_VSizer->Show((size_t)0, true);
	m_VSizer->Show((size_t)1, false);			// ???
	m_VSizer->Layout();
	
	m_Notebook.Bind(wxEVT_RIGHT_DOWN, &SingleNotebook::OnRightDown, this, -1);
	m_Notebook.Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &SingleNotebook::OnNotePageChanged, this, -1);
}

//---- DTOR -------------------------------------------------------------------

	SingleNotebook::~SingleNotebook()
{
	uLog(DTOR, "SingleNotebook[%d]::DTOR()", m_BookID);
	
	m_Notebook.Unbind(wxEVT_RIGHT_DOWN, &SingleNotebook::OnRightDown, this);
	m_Notebook.Unbind(wxEVT_NOTEBOOK_PAGE_CHANGED, &SingleNotebook::OnNotePageChanged, this);
}

//---- Get Book ID ------------------------------------------------------------

int	SingleNotebook::GetBookID(void) const
{
	return m_BookID;
}

size_t	SingleNotebook::GetNumPages(void) const
{
	return m_PageIDList.size();
}

vector<PANE_ID>	SingleNotebook::GetPageList(void) const
{
	return m_PageIDList;
}

void	SingleNotebook::PrepareBitmap(void)
{
	const wxSize	sz = GetClientSize();
	
	wxBitmap	bm(sz, 32);
	wxMemoryDC	dc(bm);
	
	dc.SetPen(*wxTRANSPARENT_PEN);
	
	dc.SetBrush(wxBrush(GetBackgroundColour(), wxBRUSHSTYLE_SOLID));
	dc.DrawRectangle(0, 0, sz.x, sz.y);
	
	dc.SetBrush(wxBrush(wxColour(0x40, 0x80, 0xff), wxBRUSHSTYLE_BDIAGONAL_HATCH));
	dc.DrawRectangle(0, 0, sz.x, sz.y);
	
	dc.SelectObject(wxNullBitmap);
	
	m_StatBitmap.SetBitmap(bm);
}

void	SingleNotebook::SetHighlight(const bool f)
{
	m_VSizer->Show((size_t)0, !f);
	m_VSizer->Show((size_t)1, f);
	m_VSizer->Layout();
}

void	SingleNotebook::HighLightEmpty(void)
{
	bool	f = !HasPages();
	
	SetHighlight(f);
}

bool	SingleNotebook::HadHighlight(void) const
{
	bool	f = m_VSizer->IsShown((size_t)1);
	
	return f;
}

//---- Hit Test during Drag ---------------------------------------------------

int	SingleNotebook::DragHitTest(const PANE_ID drag_page_id, const wxPoint &global_pos)				// DRAG = CRAP
{
	//  dragging to this notebook rect?
	const wxRect	&rc = m_Notebook.GetScreenRect();
	if (!rc.Contains(global_pos))		return PANE_NOT_FOUND;		// not inside global rect
	
	if (HadHighlight())	SetHighlight(false);
	
	// if has NO pages, don't hittest
	if (m_PageIDList.size() == 0)		return PANE_ADD_TO_BACK;
	
	// if only page is self, ignore
	if ((m_PageIDList.size() == 1) && (drag_page_id == m_PageIDList[0]))	return PANE_KEEP_AS_IS;
	
	const PageInfo	&pinfo = m_Parent.GetPageInfo(drag_page_id);
	const int	existing_pos = (pinfo.GetBookID() == GetBookID()) ? pinfo.GetPos() : -2;		// (-1 would conflict with wxNOT_FOUND)
	
	const wxPoint	client_pt = m_Notebook.ScreenToClient(global_pos);

	long		mask = 0;
	const int	page_pos = m_Notebook.HitTest(client_pt, &mask);
	
	// don't test against self
	if (page_pos == existing_pos)						return PANE_KEEP_AS_IS;
	
	// if hit nowhere, ignore if was already in book, add otherwise
	if ((wxBK_HITTEST_ONPAGE | wxBK_HITTEST_NOWHERE) & mask)		return (existing_pos >= 0) ? PANE_KEEP_AS_IS : PANE_ADD_TO_BACK;
	
	if (page_pos == wxNOT_FOUND)						return PANE_KEEP_AS_IS;		// inconclusive
	if (!(wxBK_HITTEST_ONITEM & mask))					return PANE_KEEP_AS_IS;		// inconclusive
	
	return page_pos;
}

//---- On Mouse Right Click popup menu ----------------------------------------

void	SingleNotebook::OnRightDown(wxMouseEvent &e)
{
	wxPoint	pt = m_Notebook.ScreenToClient(::wxGetMousePosition());		// get GLOBAL coords
	
	long	mask = 0;
	
	const int	pos = m_Notebook.HitTest(pt, &mask);
	if (wxNOT_FOUND == pos)			return;
	if (!(mask & wxBK_HITTEST_ONITEM))	return;		// nowhere we care(?)
	
	const PANE_ID	selected_page_id = PagePosToID(pos);
	assert(selected_page_id != PANE_ID_NONE);
	
	m_Parent.PreDragContextMenu(selected_page_id, GetBookID());
}

//---- On SingleNotebook ChangED ----------------------------------------------------

void	SingleNotebook::OnNotePageChanged(wxNotebookEvent &evt)
{
	evt.Skip();
	if (IsBeingDeleted())	return;
	
	const int	selected_page = m_Notebook.GetSelection();
	if (selected_page == wxNOT_FOUND)	return;
	
	const PANE_ID	page_id = PagePosToID(selected_page);
	
	m_Signals.OnNotepageShown(CTX_WX, page_id);
}

//---- Re-Index ---------------------------------------------------------------

void	SingleNotebook::ReIndex(void)							// bullshit ?
{
	m_PageIDList.clear();
	
	for (int pos = 0; pos < m_Notebook.GetPageCount(); pos++)
	{
		const IPaneMixIn	*mix_in1 = PagePosToMixIn(pos);
		assert(mix_in1);
		
		const PANE_ID	page_id = mix_in1->GetPageID_LL();
		
		m_PageIDList.push_back(page_id);
		
		// update this page's host notebook & position
		m_Parent.GetPageInfoRW(page_id).SetBook(m_BookID, pos);
	}
}

//---- Select Page ID ---------------------------------------------------------
	
bool	SingleNotebook::SelectPageID(const PANE_ID page_id)
{	
	const PageInfo	&pinfo = m_Parent.GetPageInfo(page_id);
	assert(pinfo.GetBook() == this);		// ERROR - isn't in this book!
	
	const int	page_pos = pinfo.GetPos();
	assert((page_pos >= 0) && (page_pos < m_PageIDList.size()));
	assert(m_PageIDList.at(page_pos) == page_id);
	
	// m_Notebook->SetSelection(page_pos);		// (triggers event)
	m_Notebook.ChangeSelection(page_pos);
	
	ReIndex();
	
	// m_Notebook->Update();
	
	return true;
}

//---- Get Selected ID --------------------------------------------------------

PANE_ID	SingleNotebook::GetSelectedPageID(void) const
{
	const int	selected_pos = m_Notebook.GetSelection();
	if (selected_pos == wxNOT_FOUND)	return PANE_ID_NONE;
	
	return PagePosToID(selected_pos);
}

//---- Add Page ID ------------------------------------------------------------

int	SingleNotebook::AddPageID(const PANE_ID page_id, const int at_pos)
{
	uLog(PANE, "SingleNotebook::AddPageID(%s)", GetPaneIDName(page_id));
	
	const PageInfo	&pinfo = m_Parent.GetPageInfo(page_id);
	assert(!pinfo.IsUsed());
	
	const int	page_index = (at_pos == PANE_ADD_TO_BACK) ? m_PageIDList.size() : at_pos;
	assert((page_index >= 0) && (page_index <= m_PageIDList.size()));
	
	wxWindow	*win = pinfo.GetWin();
	assert(win);
	
	// pane window's parent must be notebook BEFORE adding
	win->Reparent(&m_Notebook);
	win->Show();
	
	const bool	ok = m_Notebook.InsertPage(page_index, win, wxString(GetPaneIDName(page_id)), false/*don't select*/, -1/*imageId*/);
	assert(ok);
	
	m_Parent.GetPageInfoRW(page_id).SetBook(m_BookID, page_index);
	
	ReIndex();
	
	return page_index;
}

//---- Remove Page ID ---------------------------------------------------------

bool	SingleNotebook::RemovePageFromBook(const PANE_ID page_id, const bool light_f)
{
	uLog(PANE, "SingleNotebook::RemovePageFromBook(%s, light = %c)", GetPaneIDName(page_id), light_f);
	
	const PageInfo	&pinfo = m_Parent.GetPageInfo(page_id);
	assert(pinfo.IsUsed());
	assert(!pinfo.IsStale());
	
	const int	page_pos = pinfo.GetPos();
	
	const size_t	num_pages = m_PageIDList.size();
	assert(page_pos < num_pages);
	
	assert(page_id == m_PageIDList[page_pos]);
	
	wxWindow	*client_win = pinfo.GetWin();
	assert(client_win);
	
	const bool	ok = m_Notebook.RemovePage(page_pos);
	assert(ok);
	
	if (!light_f)
	{
		// reparent to nil
		client_win->Reparent(nil);							// HACKERY !! -- crashes juce
		client_win->Hide();
	}
	
	m_Parent.GetPageInfoRW(page_id).RemoveBook();
	
	ReIndex();
	
	return true;
}

//---- Page Position to MixIn Client ------------------------------------------

const IPaneMixIn*	SingleNotebook::PagePosToMixIn(const int page_index)
{
	if (page_index == wxNOT_FOUND)		return nil;
	
	wxWindow	*win = m_Notebook.GetPage(page_index);
	assert(win);
	
	void	*client_data = win->GetClientData();
	assert(client_data);
	
	const IPaneMixIn	*mix_in = static_cast<const IPaneMixIn*>(client_data);
	assert(mix_in);
	
	return mix_in;
}

//---- Page Position to ID ----------------------------------------------------

PANE_ID	SingleNotebook::PagePosToID(const int pos) const
{
	if (pos == wxNOT_FOUND)		return PANE_ID_NONE;
	
	assert((pos >= 0) && (pos < m_PageIDList.size()));
	
	const PANE_ID	selected_id = m_PageIDList[pos];
	
	return selected_id;
}

// ctor
	BookSplitter::BookSplitter(wxWindow *parent, int id, int index)
		: wxSplitterWindow(parent, id, wxDefaultPosition, wxSize(-1, -1), wxNO_BORDER | wxSP_3D | wxSP_LIVE_UPDATE)		// wxSP_3DSASH
{
	Show();
}

// dtor
	BookSplitter::~BookSplitter()
{
	uLog(DTOR, "BookSplitter::DTOR");
}

void	BookSplitter::Split(wxWindow *w1, wxWindow *w2, int pos, const bool swap_f)
{
	assert(w1);
	assert(w2 || !swap_f);
	
	// should get sash width from NativeRenderParams ?
	const int	full_w = GetWindowSize();
	
	// uLog(UI, "BookSplitter::Split(pos %d, swap_f %s, full_width %d)", pos, swap_f ? "true" : "false", full_w);
	
	if (w2)
	{	if (swap_f)
		{	SetSashGravity(0);
		}	
		else
		{	pos = full_w - pos;
			SetSashGravity(1.0);			// fixes pixel loss due to changing GetWindowSize();
		}
	
		// uLog(UI, "  processed pos %d", pos);
	}
	
	if (!w2)
	{	// unsplit (single pane)
		m_splitMode = wxSPLIT_VERTICAL;
		m_windowOne = w1;
		m_windowTwo = nil;
		m_sashPosition = 0;
		SizeWindows();
		
		w1->Show();
	}
	else
	{	// split
		assert(w1 && w2);
		
		m_splitMode = wxSPLIT_VERTICAL;
		m_windowOne = swap_f ? w2 : w1;
		m_windowTwo = swap_f ? w1 : w2;
		m_sashPosition = pos;
		SizeWindows();
		
		w1->Show();
		w2->Show();
	}
}

int	BookSplitter::GetPos(void) const
{
	const int	full_w = GetWindowSize();
	
	int	pos = GetSashPosition();
	
	if (GetWindow2() == s_WindowList[1])		// i.e. if is a swap
		pos = full_w - pos;
		
	return pos;
}

int	BookSplitter::GetFullWidth(void) const
{
	const int	full_w = GetWindowSize();
	
	return full_w;
}

//---- INSTANTIATE ------------------------------------------------------------

// static
IBottomNotePanel*	IBottomNotePanel::Create(wxWindow *parent, int id, ITopNotePanel &top_notepanel, TopFrame &tf, Controller &controller)
{
	return new BottomNotePanel(parent, id, top_notepanel, tf, controller);
}

// static
PANE_ID		IBottomNotePanel::WinToPaneID(const int win_id)
{
	assert(s_WinToPaneID.count(win_id));
	
	return s_WinToPaneID.at(win_id);
}

//---- Placeholder Panel ------------------------------------------------------

class PlaceholderPanel : public wxWindow, public PaneMixIn
{
public:
	PlaceholderPanel(IBottomNotePanel &parent, int id)
		: wxWindow(parent.GetWxWindow(), id),
		PaneMixIn(parent, id, this)
	{
	}

	virtual ~PlaceholderPanel()
	{	
		uLog(DTOR, "PlaceholderPanel::DTOR");
	}
	
	/*
	bool	Reparent(wxWindow *new_parent) override
	{
		uWarn("PlaceholderPanel::Reparent(%p)", new_parent);
		
		return wxWindow::Reparent(new_parent);
	}
	*/
};

//---- INSTANTIATE ------------------------------------------------------------

// static
IPaneMixIn*	IPaneMixIn::CreatePlaceholderPanel(IBottomNotePanel &bottom_book, int id, ITopNotePanel &top_book)
{
	return new PlaceholderPanel(bottom_book, id);
}


// nada mas
