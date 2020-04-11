// Lua DDT top Notebook

#pragma GCC diagnostic ignored "-Wunused-private-field"

#include <cassert>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "wx/panel.h"
#include "wx/notebook.h"
#include "wx/splitter.h"
#include "wx/statusbr.h"
#include "wx/statbmp.h"
#include "wx/numdlg.h"					// for wxGetNumberFromUser()
#include "wx/laywin.h"

#include "TopFrame.h"
#include "Controller.h"
#include "logImp.h"

#include "SourceFileClass.h"
#include "StyledSourceCtrl.h"
#include "SearchBar.h"
#include "UIConst.h"

#include "sigImp.h"
#include "logImp.h"

#include "TopNotebook.h"
#include "WatchBag.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

enum V_INDEX : size_t
{
	SEARCHBAR = 0,
	NOTE_TABS = 1,
	EDITOR1 = 2,
};

void	DumpFocusLL(const char *fn, const char *comment)
{	
	#if DDT_LOG_FOCUS_CHANGE
		wxWindow	*fw = wxWindow::FindFocus();
	
		uLog(FOCUS, "focus = %p %d %s @ %s %S", fw, (int) fw->GetId(), fw->GetName(), string(fn), string(comment));
	#endif
}

//---- top placeholder ------------------------------------------------

class TopPlaceholder : public wxPanel
{
public:	
	TopPlaceholder(wxWindow *parent_win, int id, ITopNotePanel &top_notebook)
		: wxPanel(parent_win, id),
		m_TopNotebook(top_notebook)
	{
		Show();
	}
	
	/*
		m_TextCtrl(this, -1, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_RICH | wxTE_NOHIDESEL | wxTE_DONTWRAP)
	{
		wxBoxSizer	*top_v_sizer =  new wxBoxSizer(wxVERTICAL);
		
		top_v_sizer->Add(&m_TextCtrl, 1, wxALL | wxEXPAND, 1);
		
		SetSizer(top_v_sizer);
		
		m_TextCtrl.AppendText("bite au cul 1234\ncaca");
		
		Show();
	}
	*/

	virtual ~TopPlaceholder()
	{	
		uLog(DTOR, "TopPlaceholder::DTOR");
	}
	
	
private:

	ITopNotePanel	&m_TopNotebook;
	
	// wxTextCtrl	m_TextCtrl;
};

//---- top Notepage -----------------------------------------------------------

class Notepage : public wxPanel
{
public:
	Notepage(wxWindow *parent, int id, ISourceFileClass *sfc)
		: wxPanel(parent, id),
		m_Sfc(sfc)
	{
		assert(sfc);
		
		SetClientData(sfc);
	}
	
	virtual ~Notepage()
	{
		SetClientData(nil);
	}
	
	ISourceFileClass*	GetSfc(void)
	{
		return m_Sfc;
	}
	
private:
	
	
	ISourceFileClass	*m_Sfc;
};

//---- Top Notebook imp -------------------------------------------------------

class TopNotebookImp : public wxNotebook, private LX::SlotsMixin<LX::CTX_WX>
{
public:	
	TopNotebookImp(wxWindow *parent, int id, ITopNotePanel &top_notepanel, IEditorCtrl &editor, TopFrame &tf, Controller &controller)
		: wxNotebook(parent, id, wxDefaultPosition, wxSize(-1, -1), wxNB_TOP),
		m_TopNotePanel(top_notepanel),
		m_Editor(editor),
		m_TopFrame(tf),				// used for var solving
		m_Controller(controller)
	{
		m_LastDaemonTick = -1;
		
		// use aux bitmaps (doesn't take ownership)
		SetImageList(&controller.GetImageList());
		
		ReIndex();
		
		MixConnect(this, &TopNotebookImp::OnEditorDirtyChanged, m_Editor.GetSignals().OnEditorDirtyChanged);
	}

	virtual ~TopNotebookImp()
	{
		uLog(DTOR, "TopNotebookImp::DTOR");
	}
	
	EditorSignals&	GetSignals(void)	{return m_Editor.GetSignals();}
	
	bool	CanQueryDaemon(void) const
	{
		if (!m_Controller.IsLuaListening())		return false;
	
		return true;
	}
	
	void	OnEditorDirtyChanged(ISourceFileClass *sfc, bool dirty_f)
	{
		assert(sfc);
		
		sfc->SetDirty(dirty_f);
		
		UpdateEditorTitles();
	}

	// var hover resolution
	bool		SolveVar_ASYNC(const string &key, wxString &res);		// returns [cached_f]
	void		SetSolvedVariable(const Collected &ce);
	bool		AddToWatches(const string &watch_name);
	
	void		NotifyPageChanged(void);
	
	void		OnCursorPosChanged(void);
	
	void		ShowSFC(ISourceFileClass *sfc);
	void		HideSFC(ISourceFileClass *sfc, const bool delete_f);
	
	void	HideAllSFCs(void)
	{
		for (auto &it : m_PageToSFCMap)
		{
			auto	*sfc = it.second;
			assert(sfc);
			
			HideSFC(sfc, false/*delete?*/);
		}
	}

	ISourceFileClass*	GetEditSFC(void) const
	{
		return m_Editor.GetCurrentSFC();
	}
	
	void	EscapeKeyCallback(void);
	
	void	UpdateEditorTitles(void)
	{
		for (auto &it : m_PageToSFCMap)
		{
			auto	*sfc = it.second;
			
			const bool	dirty_f = sfc->IsDirty();
			wxString	title = sfc->GetShortName() + (dirty_f ? "*" : "");
			
			const int	index = m_SFCtoPageMap.at(sfc);
			
			SetPageText(index, title);
		}
		
		// always set ???
		m_Controller.SetDirtyProject();
	}
	
private:
	
	ISourceFileClass*	GetSelectedSFC(void) const
	{
		const int	page_index = GetSelection();
		if (page_index == wxNOT_FOUND)		return nil;
		
		return m_PageToSFCMap.count(page_index) ? m_PageToSFCMap.at(page_index) : nil;
	}
	
	// events
	void	OnMouseLeftClick(wxMouseEvent &e);
	void	OnRightDown(wxMouseEvent &e);
	void	OnNotePageChanging(wxNotebookEvent &e);
	void	OnNotePageChanged(wxNotebookEvent &e);
	
	// PRIVATE overloads
	bool	AddPage(wxWindow *page, const wxString &text, bool bSelect, int imageId) override;
	bool	RemovePage(size_t page_pos) override;
	bool	DeletePage(size_t page_pos) override;
	int	ChangeSelection(size_t page_index) override;
	int	SetSelection(size_t page_index) override;
	
	void		ReIndex(void);
	
	ITopNotePanel	&m_TopNotePanel;
	IEditorCtrl	&m_Editor;
	TopFrame	&m_TopFrame;
	Controller	&m_Controller;
	
	unordered_map<int, ISourceFileClass*>	m_PageToSFCMap;
	unordered_map<ISourceFileClass*, int>	m_SFCtoPageMap;
	
	unordered_map<string, Collected>	m_VarCache;
	unordered_set<string>			m_VarRequestedSet;
	int					m_LastDaemonTick;
	
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(TopNotebookImp, wxNotebook)
	
	EVT_LEFT_DOWN(					TopNotebookImp::OnMouseLeftClick)
	EVT_NOTEBOOK_PAGE_CHANGED(	-1,		TopNotebookImp::OnNotePageChanged)

END_EVENT_TABLE()

void	TopNotebookImp::NotifyPageChanged(void)
{
	const int	selected_page = GetSelection();
	uLog(UI, "TopNotebookImp::NotifyPageChanged(selected page %d)", selected_page);
	
	ISourceFileClass	*sfc = GetSelectedSFC();		// may be nil
	// assert(sfc);
	
	m_TopNotePanel.SetEditSFC(sfc);
	
	// const string	shortname = sfc->GetShortName();
	// m_TopNotePanel.GetSignals().OnEditorSourceChanged(CTX_WX, shortname);
	
	m_TopNotePanel.RequestStatusBarRefresh();
}

void	TopNotebookImp::OnNotePageChanged(wxNotebookEvent &e)
{
	NotifyPageChanged();
	e.Skip();	
}

// PRIVATE OVERLOADS
bool	TopNotebookImp::AddPage(wxWindow *page, const wxString &title, bool bSelect, int imageId)
{
	uLog(UI, "TopNotebookImp::AddPage(%S)", title);
	
	const bool	f = wxNotebook::AddPage(page, title, bSelect, imageId);
	assert(f);
	
	ReIndex();
	
	return f;
}

bool	TopNotebookImp::RemovePage(size_t page_pos)
{
	const bool	f = wxNotebook::RemovePage(page_pos);
	assert(f);
	
	ReIndex();
	
	return f;
}

bool	TopNotebookImp::DeletePage(size_t page_pos)
{
	uLog(DTOR, "TopNotebookImp::DeletePage(%zu)", page_pos);
	
	const bool	f = wxNotebook::DeletePage(page_pos);
	assert(f);
	
	ReIndex();
	
	return f;
}

int	TopNotebookImp::ChangeSelection(size_t page_index)
{
	const int	old_index = wxNotebook::ChangeSelection(page_index);
	
	uLog(UI, "TopNotebookImp::ChangeSelection(%d -> %zu)", old_index, page_index);
	
	NotifyPageChanged();
	
	return old_index;
}

int	TopNotebookImp::SetSelection(size_t page_index)
{
	const int	old_index = wxNotebook::SetSelection(page_index);
	
	uLog(UI, "TopNotebookImp::SetSelection(%d -> %zu)", old_index, page_index);
	
	NotifyPageChanged();
	
	return old_index;
}

//---- re-Index SFC tables ----------------------------------------------------

void	TopNotebookImp::ReIndex(void)
{
	const size_t	sz_bf = m_PageToSFCMap.size();
	
	m_SFCtoPageMap.clear();
	m_PageToSFCMap.clear();
	
	for (int i = 0; i < GetPageCount(); i++)
	{
		wxWindow	*win = GetPage(i);
		assert(win);
		
		Notepage	*np = dynamic_cast<Notepage*>(win);
		assert(np);
		
		ISourceFileClass	*sfc = np->GetSfc();
		
		// cross-map
		m_SFCtoPageMap.emplace(sfc, i);
		m_PageToSFCMap.emplace(i, sfc);
	}
	
	const size_t	sz_af = m_PageToSFCMap.size();
	
	if ((sz_bf == 1) && (sz_af == 0))
	{	// wx bug workaround for last removal
		NotifyPageChanged();
	}
}

//---- Show SFC ---------------------------------------------------------------

void	TopNotebookImp::ShowSFC(ISourceFileClass *sfc)
{
	assert(sfc);
	
	int	index = m_SFCtoPageMap.count(sfc) ? m_SFCtoPageMap.at(sfc) : -1;
	if (-1 == index)
	{	// wasn't visible, create
		auto	*np = new Notepage(this, -1, sfc);
		
		const string	title = sfc->GetShortName();
		
		AddPage(np, title, false/*do NOT select here*/, BITMAP_ID_CLOSE_TAB);
		
		np->Show();
		
		assert(m_SFCtoPageMap.count(sfc));
		
		index = m_SFCtoPageMap.at(sfc);
	}
	
	// *** wxSTC grabs the focus *** UNLESS use calls below

	// bring to front
	ChangeSelection(index);
}

//---- Hide or Delete SFC -----------------------------------------------------

void	TopNotebookImp::HideSFC(ISourceFileClass *sfc, const bool delete_f)
{
	assert(sfc);
	
	if (!m_SFCtoPageMap.count(sfc))
	{	// ignore if is delete op and isn't shown
		if (delete_f)			return;
		
		uErr("TopNotebookImp::HideSFC(%S) wasn't shown", sfc->GetShortName());
		return;
	}
	
	int	index = m_SFCtoPageMap.at(sfc);
	
	if (delete_f)
	{	// deletes associated wxWindow
		const bool	ok = DeletePage(index);
		assert(ok);
	}
	else
	{	// close (don't delete)
		// sfc->GetEditorCtrl()->Hide();
		RemovePage(index);
	}
}

//---- On Mouse Left-Click  ---------------------------------------------------

void	TopNotebookImp::OnMouseLeftClick(wxMouseEvent &e)
{
	// skip event by default BUT unskip if needed
	// e.Skip();
	
	// hit test
	long		mask = 0;
	
	const int	page_index = HitTest(e.GetPosition(), &mask);
	if (wxNOT_FOUND == page_index)		return;
	if (!(mask & wxBK_HITTEST_ONITEM))	return;		// nowhere we care
	
	assert(m_PageToSFCMap.count(page_index));
	
	ISourceFileClass	*sfc = m_PageToSFCMap.at(page_index);
	assert(sfc);
	
	if (mask & wxBK_HITTEST_ONICON)
	{	// close button
		HideSFC(sfc, false/*delete?*/);
		
		// e.Skip(true);
	}
	else if (wxBK_HITTEST_ONLABEL & mask)
	{	// notepage label -> raise
		ChangeSelection(page_index);			// wxSTC may capture FOCUS here?
	
		// sfc->RestoreCursorPos();				
		
		// don't skip or cursor pos won't be restored!
		e.Skip(false);
	}
}

//---- Top Notepanel ----------------------------------------------------------

class DDT_CLIENT::TopNotePanel: public wxPanel, public ITopNotePanel, private SlotsMixin<CTX_WX>
{
public:
	// ctor
	TopNotePanel(wxWindow *parent, int id, TopFrame &tf, Controller &controller)
		: wxPanel(parent, id, wxDefaultPosition, wxSize(0, 0), wxBORDER_SUNKEN),
		m_TopFrame(tf),
		m_Controller(controller),
		m_SplitterTop(new BookSplitter(this, SPLITTER_ID_TOP, 0)),
		
		m_TopPanel(m_SplitterTop.get(), CONTROL_ID_TOP_PANEL, wxDefaultPosition, wxSize(0, 0), wxBORDER_NONE),
		m_EditorPtr(IEditorCtrl::CreateStyled(&m_TopPanel, CONTROL_ID_STC_EDITOR, *this, controller.GetImageList())),
		m_Editor(*m_EditorPtr),
		
		m_TopNotebookImp(&m_TopPanel, CONTROL_ID_TOP_NOTEBOOK_IMP, *this, m_Editor, tf, controller),
		m_SearchBar(&m_TopPanel, CONTROL_ID_SEARCH_PANEL, *this, tf, controller),
		m_EditorStatusBar(&m_TopPanel, CONTROL_ID_EDITOR_STATUSBAR, wxSTB_SHOW_TIPS | wxFULL_REPAINT_ON_RESIZE)		// no size grip!
	{
		m_SearchBar.Show();
		
		wxFont	ft(wxFontInfo(9).Family(wxFONTFAMILY_SWISS).Encoding(wxFONTENCODING_DEFAULT));
		m_EditorStatusBar.SetFont(ft);
		m_EditorStatusBar.SetFieldsCount(NUM_EDIT_STATUS_FIELDS);
		
		// EXPLICITLY set style on sub-fields on Gtk
		int	styles[NUM_EDIT_STATUS_FIELDS] = {0, 0, 0, 0};
		
		m_EditorStatusBar.SetStatusStyles(NUM_EDIT_STATUS_FIELDS, styles);
		m_EditorStatusBar.SetStatusText("dummy", 0);
		m_EditorStatusBar.SetMinHeight(10);
		m_EditorStatusBar.Show();
		
		m_VSizer = new wxBoxSizer(wxVERTICAL);
		m_VSizer->Add(&m_SearchBar, wxSizerFlags(0).Expand());	// .Border(wxALL, 0));
		m_VSizer->Add(&m_TopNotebookImp, wxSizerFlags(0).Expand());
		m_VSizer->Add(m_Editor.GetWxWindow(), wxSizerFlags(1).Expand());
		// m_VSizer->Add(&m_Editor2Panel, wxSizerFlags(1).Expand());
		m_VSizer->Add(&m_EditorStatusBar, wxSizerFlags(0).Border(wxLEFT | wxRIGHT, 1).Expand());
		m_TopPanel.SetSizer(m_VSizer);
		
		Layout();
		Fit();
		
		SetDropTarget(new ControllerDropTarget(controller));
		
		// init unsplit (single pane)
		m_SplitterTop->Initialize(&m_TopPanel);
		
		wxBoxSizer	*top_sizer = new wxBoxSizer(wxVERTICAL);
		
		top_sizer->Add(m_SplitterTop.get(), wxSizerFlags(1).Expand());
		
		SetSizer(top_sizer);
		
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnSaveFileMenu, this, EDIT_MENU_ID_SAVE_FILE);
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnRevertFileMenu, this, EDIT_MENU_ID_REVERT_FILE);
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnGotoLineNumber, this, MENU_ID_GOTO_LINE);
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnSetFindCommandEvent, this, TOOL_ID_SET_FIND_STRING);	// CTRL-E
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnFindCommandEvent, this, TOOL_ID_FIND);			// CTRL-F
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnFindCommandEvent, this, TOOL_ID_FIND_ALL);		// CTRL-SHIFT-F
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnCycleResultCommandEvent, this, TOOL_ID_FIND_PREVIOUS);	// CTRL-SHIFT-G
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnCycleResultCommandEvent, this, TOOL_ID_FIND_NEXT);	// CTRL-G
		
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnToggleBookmark, this, TOOL_ID_BOOKMARK_TOOGLE);
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnPreviousBookmark, this, TOOL_ID_BOOKMARK_PREV);
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnNextBookmark, this, TOOL_ID_BOOKMARK_NEXT);
		
		// m_TopFrame->Bind(wxEVT_CHAR_HOOK, &TopNotePanel::OnEscapeCharHook, this, -1);
		// m_TopFrame->Bind(wxEVT_CHAR_HOOK, &TopNotePanel::OnEscapeCharHook, this);
		tf.Bind(wxEVT_COMMAND_MENU_SELECTED, &TopNotePanel::OnToggleSelectedBreakpoint, this, TOOL_ID_TOGGLE_BREAKPOINT);
		
		Bind(wxEVT_SIZE, &TopNotePanel::OnResizeEvent, this);
		
		// hide search by default
		ShowSearchBar(false);
		
		MixConnect(this, &TopNotePanel::OnShowLocation, controller.GetSignals().OnShowLocation);
		MixConnect(this, &TopNotePanel::OnRefreshSfcBreakpoints, controller.GetSignals().OnRefreshSfcBreakpoints);
		MixConnect(this, &TopNotePanel::OnEditorCursorChanged, GetSignals().OnEditorCursorChanged);
		
		controller.SetTopNotePanel(this);
		
		// shouldn't call virtual in ctor ???
		SetEditSFC(nil);
	}

	// dtor
	virtual ~TopNotePanel()
	{
		uLog(DTOR, "TopNotePanel::DTOR");
		
		Unbind(wxEVT_SIZE, &TopNotePanel::OnResizeEvent, this);
	}
	
	wxWindow*	GetWxWindow(void) override		{return this;}
	
	EditorSignals&	GetSignals(void) override		{return m_TopNotebookImp.GetSignals();}
	
	void	RegisterCodeNavigator(LogicSignals &nav_signals) override
	{
		MixConnect(this, &TopNotePanel::OnShowLocation, nav_signals.OnShowLocation);
		MixConnect(this, &TopNotePanel::OnShowMemory, nav_signals.OnShowMemory);
	}
	
	void	ShowSFC(ISourceFileClass *sfc) override
	{
		m_TopNotebookImp.ShowSFC(sfc);
	}
	
	void	HideSFC(ISourceFileClass *sfc, bool delete_f) override
	{
		m_TopNotebookImp.HideSFC(sfc, delete_f);
	}
	
	void	HideAllSFCs(void) override
	{
		m_TopNotebookImp.HideAllSFCs();
	}

	void	UnhighlightSFCs(void) override
	{
		m_Editor.RemoveAllHighlights();
	
		m_Editor.ClearAnnotations();
	}
	
	bool	CanQueryDaemon(void) const override
	{
		return m_TopNotebookImp.CanQueryDaemon();
	}
	
	bool	SolveVar_ASYNC(const string &key, wxString &res) override		// returns [cached_f]
	{
		return m_TopNotebookImp.SolveVar_ASYNC(key, res);
	}
	
	void	SetSolvedVariable(const Collected &ce) override
	{
		m_TopNotebookImp.SetSolvedVariable(ce);
	}
	
	bool	AddToWatches(const string &watch_name) override
	{
		return m_TopNotebookImp.AddToWatches(watch_name);
	}
	
	SearchBar&	GetSearchBar(void) override
	{
		return m_SearchBar;
	}
	
	ISourceFileClass*	GetEditSFC(void) const override
	{
		return m_TopNotebookImp.GetEditSFC();
	}
	
	void	SetEditSFC(ISourceFileClass *sfc) override;
	
	BookSplitter*		GetSplitterTop(void) override		{return m_SplitterTop.get();}
	
	void	EscapeKeyCallback(void) override;
	
	void	RequestStatusBarRefresh(void) override
	{
		MixDelay("statusbar_refresh", 100/*ms*/, this, &TopNotePanel::OnUpdateStatusBar);
	}
	
	IEditorCtrl&	GetEditor(void) override
	{
		return m_Editor;
	}
	
	TopFrame&	GetTopFrame(void) override
	{
		return m_Controller.GetTopFrame();
	}
	
private:

	// events
	void	OnShowLocation(const string shortname, int ln)
	{
		assert(!shortname.empty());

		ISourceFileClass	*sfc = m_Controller.GetSFC(shortname);
		assert(sfc);
		
		sfc->ShowNotebookPage(true);
	
		m_Editor.ShowLine(ln, true/*focus?*/);
	}
	
	void	OnShowMemory(const string key, const string val, const string type_s, const bool bin_data_f)
	{
		// relay (lame?)
		m_Controller.GetSignals().OnShowMemory(CTX_WX, key, val, type_s, bin_data_f);
	}
	
	void	OnEditorCursorChanged(int col, int row)
	{
		RequestStatusBarRefresh();
	}
	
	void	OnRefreshSfcBreakpoints(ISourceFileClass *sfc, vector<int> bp_list)
	{
		uLog(BREAKPOINT_EDIT, "TopNotePanel::OnRefreshSfcBreakpoints(%zu breakpoints)", bp_list.size());
		
		assert(sfc);
		
		sfc->SetBreakpointLines(bp_list);
		
		m_Editor.SetBreakpointLines(bp_list);
	}
	
	void	OnResizeEvent(wxSizeEvent &e);
	void	OnGotoLineNumber(wxCommandEvent &e);
	void	OnToggleSelectedBreakpoint(wxCommandEvent &e);
	
	void	OnSaveFileMenu(wxCommandEvent &e)
	{
		m_Editor.SaveToDisk();
	}
	
	void	OnRevertFileMenu(wxCommandEvent &e)
	{
		// caca
	}
	
	void	OnSetFindCommandEvent(wxCommandEvent &e);
	void	OnFindCommandEvent(wxCommandEvent &e);
	void	OnCycleResultCommandEvent(wxCommandEvent &e);
	void	OnEscapeCharHook(wxKeyEvent &e);
	void	OnToggleBookmark(wxCommandEvent &e);
	void	OnPreviousBookmark(wxCommandEvent &e);
	void	OnNextBookmark(wxCommandEvent &e);
	
	void	OnUpdateStatusBar(void);
	
	// functions
	void	ShowSearchBar(const bool f = true);
	void	CycleUserBookmark(const int &delta);
	
	TopFrame			&m_TopFrame;
	Controller			&m_Controller;
	unique_ptr<BookSplitter>	m_SplitterTop;
	
	wxPanel				m_TopPanel;
	
	IEditorCtrl			*m_EditorPtr;
	IEditorCtrl			&m_Editor;
	
	TopNotebookImp			m_TopNotebookImp;

	SearchBar			m_SearchBar;
	wxStatusBar			m_EditorStatusBar;
	
	wxBoxSizer			*m_VSizer;
};

//---- Show/Hide Search Bar ---------------------------------------------------

void	TopNotePanel::ShowSearchBar(const bool f)
{
	// use size_t to avoid compiler confusion
	const bool	is_shown_f = m_VSizer->IsShown(V_INDEX::SEARCHBAR);
	
	// if state change
	if (is_shown_f != f)
	{	m_VSizer->Show(V_INDEX::SEARCHBAR, f);
		m_VSizer->Layout();
	}
	
	// may crash (?)
	// m_SearchBar.Reset();
}

//---- Set Edit SFC -----------------------------------------------------------

void	TopNotePanel::SetEditSFC(ISourceFileClass *sfc)
{
	m_Editor.LoadFromSFC(sfc);
	
	const bool	f = (sfc != nil);
	
	m_VSizer->Show(V_INDEX::EDITOR1, f);
	m_VSizer->Layout();
}

//---- On Resize event --------------------------------------------------------

void	TopNotePanel::OnResizeEvent(wxSizeEvent &e)
{
	RequestStatusBarRefresh();
	
	e.Skip();
}

//---- On Update StatusBar ----------------------------------------------------

void	TopNotePanel::OnUpdateStatusBar(void)
{
	string	status_strings[NUM_EDIT_STATUS_FIELDS];
	
	ISourceFileClass	*sfc = GetEditSFC();
	if (sfc)
	{	// cursor position
		const iPoint	pos = m_Editor.GetCursorPos();
		const int	ln = pos.y();
		const int	col = pos.x();
		const int	total_lines = m_Editor.GetTotalLines();
		const bool	writable_f = sfc->IsWritableFile();
		const bool	dirty_f = sfc->IsDirty();
		
		status_strings[0] = xsprintf("line %d, column %d", ln, col);
		
		status_strings[1] = xsprintf("%S: %d lines, %s %s", sfc->GetShortName(), total_lines, writable_f ? "r/w" : "READ-ONLY", dirty_f ? "DIRTY" : "clean");
		
		// any search results
		vector<SearchHit>	hits;
		
		const int	result_ind = m_SearchBar.GetSearchStatus(hits/*&*/);		// (search result NOT NECESSARILY in selected SFC)
		if (result_ind != -1)
		{	status_strings[3] = xsprintf("result %d / %zu", result_ind + 1, hits.size());
		}
	}
	else
	{	// repaint background or leaves smudge when no page shown - doesn't work?
		//   Refresh();
	}
	
	// set text fields
	for (int i = 0; i < NUM_EDIT_STATUS_FIELDS; i++)
		m_EditorStatusBar.SetStatusText(wxString(status_strings[i]), i);
}

//---- On Goto Line -----------------------------------------------------------

void	TopNotePanel::OnGotoLineNumber(wxCommandEvent &e)
{
	ISourceFileClass	*sfc = GetEditSFC();
	if (!sfc)			return;			// none selected?
	
	const int	max_ln = m_Editor.GetTotalLines();
	
	int	line_nbr = ::wxGetNumberFromUser(""/*message*/, "Goto Line:"/*prompt*/, "Goto Line Number", 0, 1/*min*/, max_ln, this/*parent*/);
	if (line_nbr == -1)		return;			// canceled
	
	m_Editor.ShowLine(line_nbr, true/*focus?*/);
}

//---- On Toggle Selected Breakpoint ------------------------------------------

	// (only used on F9)

void	TopNotePanel::OnToggleSelectedBreakpoint(wxCommandEvent &e)
{
	ISourceFileClass	*sfc = GetEditSFC();
	if (!sfc)		return;				// no selected top tab?
	
	const int	ln = m_Editor.GetCursorPos().y();
	if (ln == -1)		return;					// no selected line?
	
	m_Controller.ToggleBreakpoint(*sfc, ln);
}

//---- On Set Find String command event ---------------------------------------

void	TopNotePanel::OnSetFindCommandEvent(wxCommandEvent &e)
{
	// e.Skip();
	
	ISourceFileClass	*sfc = GetEditSFC();
	if (!sfc)		return;				// no selected top tab?
	
	ShowSearchBar(true);		// show
	
	const string	selection_s = m_Editor.GetUserSelection();
	
	m_SearchBar.Reset(true, selection_s);
}

//---- On Keyboard/Menu CTRL-F command ----------------------------------------

void	TopNotePanel::OnFindCommandEvent(wxCommandEvent &e)
{
	// const int	id = e.GetId();
	// const bool	find_all_f = (id == TOOL_ID_FIND_ALL);
	
	// ISourceFileClass	*sfc = GetEditSFC();
	// if (!sfc)		return;				// no selected top tab?
	
	ShowSearchBar(true);		// show

	m_SearchBar.Reset(true);
	
	e.Skip();
}

//---- Escape Key Callback ----------------------------------------------------

void	TopNotePanel::EscapeKeyCallback(void)
{
	ShowSearchBar(false);			// hide
	m_SearchBar.Reset(false);
}

//---- On Keyboard/Menu CTRL-G-(shift) command --------------------------------

void	TopNotePanel::OnCycleResultCommandEvent(wxCommandEvent &e)
{
	const int	id = e.GetId();
	assert((id == TOOL_ID_FIND_PREVIOUS) || (id == TOOL_ID_FIND_NEXT));
	
	const int	delta = (id == TOOL_ID_FIND_PREVIOUS) ? -1 : 1;
	
	m_SearchBar.CycleResultRelative(delta);
	
	// e.Skip();		// not needed?
}

//---- On Toggle Bookmark -----------------------------------------------------

void	TopNotePanel::OnToggleBookmark(wxCommandEvent &e)
{
	ISourceFileClass	*sfc = GetEditSFC();
	if (!sfc)		return;					// no selected source
	
	const int	ln = m_Editor.GetCursorPos().y();
	if (ln == -1)		return;					// no selected line?
	
	#if 0
	const bool	f = sfc->HasLineMarker(ln, MARGIN_MARKER_T::USER_BOOKMARK);
	
	uLog(BOOKMARK, "TopNotePanel::OnToggleBookmark(%s:%d) %c -> %c", sfc->GetShortName(), ln, f, !f);
	
	sfc->SetLineMarker(ln, MARGIN_MARKER_T::USER_BOOKMARK, !f/*toggle*/);
	#endif
}

//---- Cycle User Bookmark ----------------------------------------------------

void	TopNotePanel::CycleUserBookmark(const int &delta)
{
	uLog(BOOKMARK, "TopNotePanel::CycleUserBookmark(delta %d)", delta);
	
	ISourceFileClass	*selected_sfc = GetEditSFC();
	if (!selected_sfc)	return;			// no selected source
	
	const int	selected_ln = m_Editor.GetCursorPos().y();
	if (selected_ln == -1)		return;		// no selected line?
	
	// add bookmarks from project sources
	const vector<ISourceFileClass*>	sfc_list = m_Controller.GetSFCInstances();
	
	#if 0
	vector<Bookmark>		bmks;
	
	// for (ISourceFileClass *sfc : sfc_list)	sfc->AddUserBookmarks(bmks/*&*/);
	
	if (bmks.empty())	return;			// no bookmarks
	
	for (size_t i = 0; i < bmks.size(); i++)
		uLog(BOOKMARK, "  [%d] %s:%d", i, bmks[i].Name(), bmks[i].Line());
	
	const string	fn = selected_sfc->GetShortName();
	
	int	ind = -1;
	bool	fn_crossed_f = false;
	
	for (size_t i = 0; i < bmks.size(); i++)
	{
		const auto	&it = bmks[i];
		
		const bool	fn_f = (it.Name() == fn);
		if (!fn_f && fn_crossed_f)
		{	// new sfc but source file was crossed earlier
			ind = i;
			break;
		}
		
		fn_crossed_f |= fn_f;
		
		if (fn_f && (it.Line() >= selected_ln))
		{	// sfc match, line equal or greater
			ind = i;
			break;
		}
	}
	
	if (ind == -1)
	{
		uLog(BOOKMARK, "no bmk index found, get maxima");
		
		ind = (delta > 0) ? 0 : (bmks.size() - 1);
	}
	else
	{	uLog(BOOKMARK, "found bmk index %d (%s:%d)", ind, bmks[ind].Name(), bmks[ind].Line());
	
		/*
		if ((delta < 0) || selected_sfc->HasLineMarker(selected_ln, MARGIN_MARKER_T::USER_BOOKMARK))		// should show FIRST
		{	ind = (ind + delta + bmks.size()) % bmks.size();
			uLog(BOOKMARK, "  bmk CYCLE");
		}
		*/
	}
	
	uLog(BOOKMARK, "new bmk index %d (%s:%d)", ind, bmks[ind].Name(), bmks[ind].Line());
	
	ISourceFileClass	*sfc = m_Controller.GetSFC(bmks[ind].Name());
	assert(sfc);
	
	// TOO MUCH CRAP?
	sfc->ShowNotebookPage(true);
	// sfc->ShowLine(bmks[ind].Line(), true/*center*/);
	// sfc->SetFocus();
	
	#endif
}

//---- On Previous Bookmark ---------------------------------------------------

void	TopNotePanel::OnPreviousBookmark(wxCommandEvent &e)
{
	CycleUserBookmark(-1/*prev*/);
}

//---- On Next Bookmark -------------------------------------------------------

void	TopNotePanel::OnNextBookmark(wxCommandEvent &e)
{
	CycleUserBookmark(1/*next*/);
}

//---- INSTANTIATE ------------------------------------------------------------

// static
ITopNotePanel*	ITopNotePanel::Create(wxWindow *parent, int id, TopFrame &tf, Controller &controller)
{
	return new TopNotePanel(parent, id, tf, controller);
}

//////////////////////////////////////////

//---- Request Solve Variable -------------------------------------------------

bool	TopNotebookImp::SolveVar_ASYNC(const string &key, wxString &res)
{
	res.clear();
	
	// uMsg("TopNotebookImp::SolveVar(%S)", key);
	
	if (m_Controller.GetDaemonTick() != m_LastDaemonTick)
	{	m_LastDaemonTick = m_Controller.GetDaemonTick();
	
		// uLog(WARNING, "var cache FLUSHED (had %zu entries)", m_VarCache.size());
		
		m_VarCache.clear();
		m_VarRequestedSet.clear();
	}
	
	const bool	chached_f = (m_VarCache.count(key) > 0);
	if (chached_f)
	{	// get cached
		const Collected	&ce = m_VarCache.at(key);
		
		string	k = ce.GetDecoratedKey();
		string	v = ce.GetDecoratedVal();
		
		res = wxString::Format("%s = %s", wxString(k), wxString(v));
	}
	else
	{	// not cached
		if (m_VarRequestedSet.count(key) > 0)		return false;		// already requested
		
		// request
		bool	ok = m_TopFrame.SolveVar_ASYNC(VAR_SOLVE_REQUESTER::SOLVE_REQUESTER_HOVER, key);
		if (ok)		m_VarRequestedSet.insert(key);
		else		uErr("TopNotebookImp::SolveVar_ASYNC() failed");
	}
	
	return chached_f;
}

//---- Receive Solved Variable ------------------------------------------------

void	TopNotebookImp::SetSolvedVariable(const Collected &ce)
{
	const string	key = ce.GetKey();
	
	// uMsg("solved variable %S = %s", key, ce.GetVal());
	
	m_VarCache[key] = ce;
	m_VarRequestedSet.erase(key);
}

//---- Add To Watches ---------------------------------------------------------

bool	TopNotebookImp::AddToWatches(const string &watch_name)
{
	WatchBag	&watches = m_TopFrame.GetWatchBag();
	
	return watches.Add(watch_name);
}

// nada mas
