// ddt search bar

#include <cassert>
#include <sstream>
#include <unordered_map>

#include "wx/tglbtn.h"
#include "wx/confbase.h"

#include "logImp.h"

#include "TopFrame.h"
#include "Controller.h"
#include "TopNotebook.h"
#include "UIConst.h"
#include "StyledSourceCtrl.h"
#include "SourceFileClass.h"
#include "SearchResultsPanel.h"

#include "SearchBar.h"

using namespace std;
using namespace DDT_CLIENT;

struct button_info
{
	string		m_Label;
	string		m_ToolTip;
	wxString	m_PrefTag;
};

const
unordered_map<int, button_info>	s_IDtoLabelMap
{
	{BTN_INDEX_CASE,		button_info{"C",	"case-sensitive",		"case"}},
	{BTN_INDEX_WHOLE_WORD,		button_info{"W",	"whole word",			"whole_word"}},
	{BTN_INDEX_WILDCARD,		button_info{"*",	"wildcards",			"wildcard"}},
	{BTN_INDEX_REGEX,		button_info{"R",	"RegEx",			"regex"}},
	{BTN_INDEX_LIVE_CODE,		button_info{"L",	"live code (not comment)",	"no_comment"}},
	{BTN_INDEX_ALL_FILES,		button_info{"A",	"all files",			"all_files"}},
	{BTN_INDEX_HIGHLIGHT,		button_info{"H",	"highlight",			"highlight"}}
};

BEGIN_EVENT_TABLE(SearchBar, wxPanel)
	
	EVT_COMMAND_RANGE(	BUTTON_ID_BASE,	BUTTON_ID_LAST - 1,	wxEVT_TOGGLEBUTTON,	SearchBar::OnToggleButton)
	
	// EVT_KILL_FOCUS(										SearchBar::OnKillFocusEvent)
	
	EVT_THREAD(		BROADCAST_ID_RE_SEARCH,						SearchBar::OnReSearch_SYNTH)
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

	SearchBar::SearchBar(wxWindow *parent, int win_id, ITopNotePanel &top_note_panel, TopFrame &tf, Controller &controller)
		: wxPanel(parent, win_id, wxDefaultPosition, wxDefaultSize),	// size can't be (0, 0) or will collapse
		m_TopNotePanel(top_note_panel),
		m_TopFrame(tf),
		m_Controller(controller)
{
	m_SearchResultsPanel = nil;
	
	m_ToggleButtonList.clear();
	m_PrefTagToID.clear();
	
	wxFont	ft(wxFontInfo(10).Family(wxFONTFAMILY_SWISS).Encoding(wxFONTENCODING_DEFAULT));
	SetFont(ft);

	wxBoxSizer *h_sizer = new wxBoxSizer(wxHORIZONTAL);

	for (int i = 0; i < BTN_INDEX_TOTAL; i++)
	{
		assert(s_IDtoLabelMap.count(i) > 0);
		const button_info	&binfo = s_IDtoLabelMap.at(i);
		
		const int	id = BUTTON_ID_BASE + i;
		
		wxToggleButton	*button = new wxToggleButton(this, id, wxString(binfo.m_Label), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
		
		button->SetToolTip(wxString{binfo.m_ToolTip});
		
		m_PrefTagToID.insert({binfo.m_PrefTag, i});
		
		// (disable focus outline)
		button->SetCanFocus(false);
		
		m_ToggleButtonList.push_back(button);
		
		h_sizer->Add(button, wxSizerFlags(0).Border(wxALL, 2).Align(wxALIGN_CENTER_VERTICAL));
	}
	
	m_SearchEditCtrl = new wxTextCtrl(this, CONTROL_ID_SEARCH_EDIT_CONTROL, "search edit ctrl", wxDefaultPosition, wxDefaultSize, wxTE_RICH | wxTE_NOHIDESEL | wxTE_PROCESS_ENTER);
	
	// sunken border doesn't work!
	h_sizer->Add(new wxStaticText(this, -1, "Find:", wxDefaultPosition, wxDefaultSize, /*wxBORDER_SUNKEN |*/ wxBORDER_STATIC), wxSizerFlags(0).Border(wxLEFT | wxRIGHT, 4).Align(wxALIGN_CENTER_VERTICAL));
	h_sizer->Add(m_SearchEditCtrl, wxSizerFlags(4).Border(wxLEFT | wxRIGHT, 4).Align(wxALIGN_CENTER_VERTICAL));
	
	SetSizer(h_sizer);
	
	Layout();
	Fit();
	
	// event ID order is IMPORTANT (low->high)
	tf.Bind(wxEVT_CHAR_HOOK, &SearchBar::OnSearchCtrlCharENTERHook, this, CONTROL_ID_SEARCH_PANEL, CONTROL_ID_SEARCH_EDIT_CONTROL);
	tf.Bind(wxEVT_TEXT, &SearchBar::OnSearchCtrlChar, this, CONTROL_ID_SEARCH_EDIT_CONTROL);
}

//---- DTOR -------------------------------------------------------------------

	SearchBar::~SearchBar()
{
	uLog(DTOR, "SearchBar::DTOR");
	
	m_SearchResultsPanel = nil;
	
	// FIXME - unbind events
}

//---- Set Search Results Panel -----------------------------------------------

void	SearchBar::SetSearchResultsPanel(SearchResultsPanel *srp)
{
	assert(srp);
	
	m_SearchResultsPanel = srp;
}

//---- On Kill Focus event ----------------------------------------------------

void	SearchBar::OnKillFocusEvent(wxFocusEvent &e)
{
	DumpFocus("SearchBar");
		
	e.Skip();
}

//---- Load UI ----------------------------------------------------------------

void	SearchBar::LoadUI(const wxConfigBase &config)
{
	wxConfigPathChanger	chg(&config, "/Search/");
	
	for (const auto &it : m_PrefTagToID)
	{	const string	tag = it.first;
		const int	id = it.second;
		
		wxToggleButton	*button = m_ToggleButtonList[id];
		assert(button);
		
		bool	f;
		
		config.Read(tag, &f, false);
		
		button->SetValue(f);
	}
}

//---- Save UI ----------------------------------------------------------------

void	SearchBar::SaveUI(wxConfigBase &config) const
{
	wxConfigPathChanger	chg(&config, "/Search/");
	
	for (int i = 0; i < BTN_INDEX_TOTAL; i++)
	{
		assert(s_IDtoLabelMap.count(i) > 0);
		const button_info	&binfo = s_IDtoLabelMap.at(i);
		
		wxToggleButton	*button = m_ToggleButtonList[i];
		assert(button);
		
		const bool	f = button->GetValue();
	
		config.Write(binfo.m_PrefTag, f);
	}
}

//---- On Toggle Button event -------------------------------------------------

void	SearchBar::OnToggleButton(wxCommandEvent &e)
{
	const int	id = e.GetId();
	const int	index = id - BUTTON_ID_BASE;
	assert((index >= 0) && (index < BTN_INDEX_TOTAL));
	
	// const bool	f = e.IsChecked();
	
	// post to self
	wxThreadEvent	evt(wxEVT_THREAD,BROADCAST_ID_RE_SEARCH);
	
	wxQueueEvent(this, evt.Clone());
	
	// save UI
	m_TopFrame.DirtyUIPrefs();
}

//---- On Re-Search (synthetic) event -----------------------------------------

void	SearchBar::OnReSearch_SYNTH(wxThreadEvent &e)
{
	DoSearch();
}

//---- Get All Buttons States -------------------------------------------------

vector<bool>	SearchBar::GetAllButtonStates(void) const
{
	vector<bool>	states;
	
	for (int i = 0; i < BTN_INDEX_TOTAL; i++)
	{
		wxToggleButton	*button  = m_ToggleButtonList[i];
		assert(button);
		
		const bool	f = button->GetValue();
		
		states.push_back(f);
	}
	
	return states;
}

//---- On Char Hook event -----------------------------------------------------

void	SearchBar::OnSearchCtrlCharENTERHook(wxKeyEvent &e)
{
	const int	k = e.GetKeyCode();
	if (((k == WXK_NUMPAD_ENTER) || (k == WXK_RETURN)) && (m_HitList.size() > 0))
	{
		DumpFocus("ENTER/shift-ENTER start");
		
		// check SHIFT for up/down movement
		const int	delta = (e.GetModifiers() == wxMOD_SHIFT) ? -1 : 1;
		
		CycleResultRelative(delta);
		
		DumpFocus("ENTER/shift-ENTER end");
		
		#if DDT_RESTORE_FOCUS
			m_SearchEditCtrl->SetFocus();
		#endif
	}
	else	e.Skip();
}

//---- Cycle Result Relative --------------------------------------------------

void	SearchBar::CycleResultRelative(const int &delta)
{
	const size_t	n_hits = m_HitList.size();
	if (!n_hits)	return;				// avoid div by zero
	
	const int	new_cycler = (m_HitCycler + delta + n_hits) % n_hits;
	
	bool	ok = GotoNthResult(new_cycler);
	assert(ok);	
}

//---- On Character/Key pressed -----------------------------------------------

void	SearchBar::OnSearchCtrlChar(wxCommandEvent &e)
{
	DumpFocus("START");
	
	// wxWindow	*focus_bf = wxWindow::FindFocus();
	
	DoSearch();
	
	DumpFocus("END");
	
	#if DDT_RESTORE_FOCUS
		m_SearchEditCtrl->SetFocus();
	#endif
}

//---- Reset ------------------------------------------------------------------

void	SearchBar::Reset(const bool &show_f, const string &initial_string)
{
	assert(m_SearchEditCtrl);
	assert(m_SearchResultsPanel);
	
	DumpFocus("");
	
	// (doesn't trigger event)
	m_SearchEditCtrl->ChangeValue(initial_string);
	m_SearchEditCtrl->SetBackgroundColour(*(SEARCH_RESULTS_COLOR_DEFAULT.ToWxColor()));
	
	ClearHits();
	
	if (show_f)	m_SearchEditCtrl->SetFocus();
	
	m_SearchResultsPanel->ClearResults();

	m_TopNotePanel.RequestStatusBarRefresh();
	
	m_TopNotePanel.GetEditor().RemoveSearchHighlights();
	m_TopNotePanel.GetEditor().RemoveLineMarkers(MARGIN_MARKER_T::SEARCH_RESULT_LINE);
	
	if (!initial_string.empty())
	{
		DoSearch();
	}
}

//---- Do Search --------------------------------------------------------------

void	SearchBar::DoSearch(void)
{
	assert(m_SearchEditCtrl);
	
	wxString	s = m_SearchEditCtrl->GetValue();
	
	ClearHits();
	
	if (s.Len() == 0)
	{
		DumpFocus("empty string");
		Reset();
		return;
	}
	
	#if 0
	
	// remove highlights from ALL sfc
	for (ISourceFileClass *sfc : m_Controller.GetSFCInstances())
	{
		sfc->RemoveSearchHighlights();
		sfc->RemoveLineMarkers(MARGIN_MARKER_T::SEARCH_RESULT_LINE);
	}
	
	const vector<bool>	states = GetAllButtonStates();
	
	uint32_t	mask = 0;
	
	// basic flags
	if (states[SEARCH_TOGGLE_INDEX::BTN_INDEX_CASE])		mask |= FIND_FLAG::CASE;
	if (states[SEARCH_TOGGLE_INDEX::BTN_INDEX_WHOLE_WORD])		mask |= FIND_FLAG::WHOLE_WORD;
	if (states[SEARCH_TOGGLE_INDEX::BTN_INDEX_REGEX])		mask |= FIND_FLAG::RE_BASIC;
	
	// my extensions
	if (states[SEARCH_TOGGLE_INDEX::BTN_INDEX_LIVE_CODE])		mask |= FIND_FLAG::EXT_NO_COMMENT;
	if (states[SEARCH_TOGGLE_INDEX::BTN_INDEX_HIGHLIGHT])		mask |= FIND_FLAG::EXT_HIGHLIGHT;
	
	if (states[SEARCH_TOGGLE_INDEX::BTN_INDEX_WILDCARD])
	{
		s.Replace("*", ".*", true/*replace all*/);
		mask |= RE_BASIC;
	}
	
	const string	patt = s.ToStdString();
	
	vector<ISourceFileClass*>	sfc_list;
	
	if (states[SEARCH_TOGGLE_INDEX::BTN_INDEX_ALL_FILES])
	{	// all project files
		sfc_list = m_Controller.GetSFCInstances();
		
		if (m_TopNotePanel.GetSelectedSFC())
		{	// search any selected SFC first
			ISourceFileClass	*selected = m_TopNotePanel.GetSelectedSFC();
			
			auto	it = find(sfc_list.begin(), sfc_list.end(), selected);
			assert(sfc_list.end() != it);
	
			// remove
			sfc_list.erase(it);
			
			// put in front
			sfc_list.insert(sfc_list.begin(), selected);
		}
	}
	else
	{	// one selected SFC
		if (m_TopNotePanel.GetSelectedSFC())
			sfc_list.push_back(m_TopNotePanel.GetSelectedSFC());
	}
	
	for (ISourceFileClass *sfc : sfc_list)
	{
		// EditorCtrl	*styled_editor = sfc->GetEditorCtrl();
		// assert(styled_editor);
		
		// const int	new_hits = styled_editor->SearchText(patt, mask, m_HitList/*&*/);
		// (void)new_hits;
	}
	
	const int	n_hits = m_HitList.size();
	if (n_hits > 0)
	{	// some results
		m_SearchEditCtrl->SetBackgroundColour(*(SEARCH_RESULTS_COLOR_DEFAULT.ToWxColor()));
		
		GotoNthResult(0);
	}
	else
	{	// no result
		m_SearchEditCtrl->SetBackgroundColour(*(SEARCH_RESULTS_COLOR_FAIL.ToWxColor()));
	}
	
	SearchResultsPanel	*results_panel = m_TopFrame.GetSearchResultsPanel();
	assert(results_panel);
	
	results_panel->SetResults(m_HitList);
	
	#endif
}

//---- Clear Hits -------------------------------------------------------------

void	SearchBar::ClearHits(void)
{
	m_HitCycler = 0;
	m_HitList.clear();
	
	// ed->RemoveLineMarkers(MARGIN_MARKER_T::SEARCH_RESULT_LINE);
}

//---- Reset Last Cycler ------------------------------------------------------

void	SearchBar::ResetLastCycler(void)
{
	if (m_HitList.size() == 0)	return;
	
	// const SearchHit	hit = m_HitList.at(m_HitCycler);
	
	// ISourceFileClass	&sfc = hit.GetSFC();
	// sfc.RemoveLineMarkers(MARGIN_MARKER_T::SEARCH_RESULT_LINE);
}

//---- Goto Nth Result --------------------------------------------------------

bool	SearchBar::GotoNthResult(const int &result_ind)
{
	uLog(UI, "SearchBar::GotoNthResult(%d)", result_ind);
	
	DumpFocus("START");
	
	const size_t	n_hits = m_HitList.size();
	if (n_hits == 0)
	{	m_HitCycler = 0;
		return false;		// ignore
	}
	
	ResetLastCycler();
	
	m_HitCycler = result_ind;
	
	const SearchHit	hit = m_HitList.at(m_HitCycler);
	
	ISourceFileClass	&sfc = hit.GetSFC();
	sfc.ShowNotebookPage(true);		// may lose focus when adding new notepage due to wxSTC ?
	
	#if 0
	const int	pos1 = hit.Pos1();
	const int	pos2 = hit.Pos2();
	const int	ln = hit.Line();
	
	const wxString	fname = sfc->GetShortName();
	
	sfc->SetLineMarker(ln, MARGIN_MARKER_T::SEARCH_RESULT_LINE);
	sfc->ShowSpan(pos1, pos2);
	
	m_TopNotePanel.RequestStatusBarRefresh();
	
	DumpFocus("END");
	
	#endif
	
	return true;
}

//---- Get Search Status ------------------------------------------------------

int	SearchBar::GetSearchStatus(vector<SearchHit> &hits) const
{
	hits = m_HitList;
	if (hits.size() == 0)		return -1;
	
	assert((m_HitCycler >= 0) && (m_HitCycler < m_HitList.size()));
	
	return m_HitCycler;
}

/*
//---- On New Selected SFC ----------------------------------------------------

void	SearchBar::OnSelectedSFC_SYNTH(wxThreadEvent &e)
{
	uLog(SYNTH, "SearchBar::OnSelectedSFC_SYNTH()");
	
	// too late here!!
	UpdateLuaFunctions(true);
}
*/



// nada mas
