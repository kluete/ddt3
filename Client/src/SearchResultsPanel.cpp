// search results panel

#include "wx/textctrl.h"

#include "BottomNotebook.h"

#include "logImp.h"
#include "SourceFileClass.h"
#include "StyledSourceCtrl.h"
#include "SearchBar.h"

#include "SearchResultsPanel.h"

using namespace std;
using namespace DDT_CLIENT;

BEGIN_EVENT_TABLE(SearchResultsPanel, wxPanel)

END_EVENT_TABLE()

const long	styl = wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH | wxTE_NOHIDESEL | wxTE_DONTWRAP;

//---- CTOR -------------------------------------------------------------------

	SearchResultsPanel::SearchResultsPanel(IBottomNotePanel &parent, int id, SearchBar &search_bar)
		: wxPanel(parent.GetWxWindow(), id),
		PaneMixIn(parent, id, this),
		m_SearchBar(search_bar),
		m_TextCtrl(this, CONTROL_ID_SEARCH_RESULTS_TEXT, "", wxDefaultPosition, wxDefaultSize, styl)
{
	// (uses COW)
	wxFont	ft(wxFontInfo(9).Family(wxFONTFAMILY_MODERN).Encoding(wxFONTENCODING_DEFAULT));
	SetFont(ft);
	
	wxFont	ft_uri(wxFontInfo(9).Family(wxFONTFAMILY_MODERN).Encoding(wxFONTENCODING_DEFAULT).Underlined());
	
	m_DefaultAttr = wxTextAttr(*wxBLACK, *wxWHITE, ft);
	m_UriAttr = wxTextAttr(*wxBLUE, *wxWHITE, ft_uri);
	m_HighlightAttr = wxTextAttr(*wxBLACK, *wxYELLOW, ft);
	
	wxBoxSizer	*v_sizer = new wxBoxSizer(wxVERTICAL);
	
	v_sizer->Add(&m_TextCtrl, wxSizerFlags(1).Border(wxALL, 2).Expand());
	
	SetSizer(v_sizer);
	
	m_TextCtrl.SetDefaultStyle(m_DefaultAttr);
	
	m_TextCtrl.Bind(wxEVT_LEFT_DOWN, &SearchResultsPanel::OnLeftDownEvent, this);
	m_TextCtrl.Bind(wxEVT_LEFT_DCLICK, &SearchResultsPanel::OnLeftDoubleClickEvent, this);
	
	// register self with search bar
	m_SearchBar.SetSearchResultsPanel(this);
}

//---- DTOR -------------------------------------------------------------------

	SearchResultsPanel::~SearchResultsPanel()
{	
	m_TextCtrl.Unbind(wxEVT_LEFT_DOWN, &SearchResultsPanel::OnLeftDownEvent, this);
	m_TextCtrl.Unbind(wxEVT_LEFT_DCLICK, &SearchResultsPanel::OnLeftDoubleClickEvent, this);
}

//---- Clear Search Results ---------------------------------------------------

void	SearchResultsPanel::ClearResults(void)
{
	m_UriLenLUT.clear();
	m_TextCtrl.ChangeValue("");
}

//---- Set Search Results -----------------------------------------------------

void	SearchResultsPanel::SetResults(const vector<SearchHit> &hit_list)
{
	const size_t	len = hit_list.size();
	/*
	const int	n_digits = ((len > 0) ? log10(len) : 0) + 1;
	const wxString	fmt_s = wxString::Format("%% %dd: ", n_digits);
	*/
	
	struct span
	{	int	m_index;
		int	m_len;
	};
	vector<span>	uris, highlights;
	wxString	buff = "";
	
	m_UriLenLUT.clear();
	
	for (size_t i = 0; i < len; i++) 
	{
		const SearchHit	&hit = hit_list[i];
		
		ISourceFileClass	&sfc = hit.GetSFC();
		
		// save URI span & len
		const wxString	uri = wxString::Format("%s:%d", sfc.GetShortName(), hit.Line());
		const int	uri_pos = buff.size();
		const int	uri_len = uri.length();
		uris.push_back({uri_pos, uri_len});
		m_UriLenLUT.push_back(uri_len);
		
		// write URI
		buff << uri << " ";
		
		// save highlight span (after URI)
		const int	hl_pos = buff.size() + hit.LineIndex();
		highlights.push_back({hl_pos, hit.Len()});
		
		// write result line
		// buff << ed->GetLine(hit.Line() - 1);
	}
	
	m_TextCtrl.Freeze();
	m_TextCtrl.ChangeValue("");
	m_TextCtrl.WriteText(buff);
	
	// apply URI styles
	for (const span &sp : uris)		m_TextCtrl.SetStyle(sp.m_index, sp.m_index + sp.m_len, m_UriAttr);
	
	// apply highlights
	for (const span &sp : highlights)	m_TextCtrl.SetStyle(sp.m_index, sp.m_index + sp.m_len, m_HighlightAttr);
	
	// move caret home
	m_TextCtrl.ShowPosition(1);
	
	m_TextCtrl.Thaw();
}

//---- On Left Mouse Button Click event ---------------------------------------

void	SearchResultsPanel::OnLeftDownEvent(wxMouseEvent &e)
{
	wxTextCoord	col, row;
	
	wxTextCtrlHitTestResult	res = m_TextCtrl.HitTest(e.GetPosition(), &col, &row);
	if (res != wxTE_HT_ON_TEXT)		return;
	if (m_UriLenLUT.size() == 0)		return;
	
	assert((row >= 0) && (row < m_UriLenLUT.size()));
	const int	uri_len = m_UriLenLUT[row];
	
	const bool	f = (col <= uri_len);
	if (!f)		return;
	
	// uLog(UI, "goto result row %d", row);
	
	m_SearchBar.GotoNthResult(row);
}

//---- On Left Mouse Button Double-Click event --------------------------------

void	SearchResultsPanel::OnLeftDoubleClickEvent(wxMouseEvent &e)
{
	wxTextCoord	col, row;
	
	const wxTextCtrlHitTestResult	res = m_TextCtrl.HitTest(e.GetPosition(), &col, &row);
	if (res != wxTE_HT_ON_TEXT)		return;
	
	m_SearchBar.GotoNthResult(row);
}

// nada mas
