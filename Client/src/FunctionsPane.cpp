// Lua functions panel

#include <sstream>

#include "BottomNotebook.h"
#include "TopNotebook.h"

#include "UIConst.h"
#include "SourceFileClass.h"
#include "StyledSourceCtrl.h"

using namespace std;
using namespace DDT_CLIENT;
using namespace	LX;

//---- Search Results Panel ---------------------------------------------------

class FunctionsPanel: public wxPanel, public PaneMixIn, private SlotsMixin<CTX_WX>
{
public:
	FunctionsPanel(IBottomNotePanel &parent, int id, ITopNotePanel &topnotepanel);
	virtual ~FunctionsPanel();
	
private:

	void	OnPanelClearVariables(void);
	void	OnPanelRequestReload(void);
	
	void	OnSfcSelection(ISourceFileClass *sfc);
	void	OnUpdateSfcLuaFunctions(ISourceFileClass *sfc);
	void	OnUpdateSfcCursor(ISourceFileClass *sfc, int ln);
	
	void	OnRedrawEvent(wxCommandEvent &e);
	void	OnLeftDownEvent(wxMouseEvent &e);
	
	void	OnMouseScrubEvent(wxMouseEvent &e)
	{
		if (!e.LeftIsDown())	return;
	
		OnLeftDownEvent(e);
	}
	
	void	ColorMe(void);
	
	ITopNotePanel	&m_TopNotePanel;
	wxRadioButton	m_LineSortRadio, m_AlphaSortRadio;
	wxTextCtrl	m_TextCtrl;
	wxTextAttr	m_BlackAttr, m_HighlightAttr;
	
	ISourceFileClass		*m_SFC;					// only used for mouse-button down
	vector<Bookmark>		m_FunctionBookmarks;
	vector<int>			m_LineToSortedTab, m_SortedToLineTab;
	
	using Span = std::pair<size_t, size_t>;
	
	vector<Span>			m_LineToPosLUT;
	int				m_SelectedRow;
	
	DECLARE_EVENT_TABLE()
};

const wxColour	COLOR_SELECTED_FUNCTION(0xe0, 0xe0, 0xff);
	
const long	styl = wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH | wxTE_NOHIDESEL | wxTE_DONTWRAP;

BEGIN_EVENT_TABLE(FunctionsPanel, wxPanel)

	EVT_RADIOBUTTON(	RADIO_ID_FUNCTIONS_SORT_ALPHA,		FunctionsPanel::OnRedrawEvent)
	EVT_RADIOBUTTON(	RADIO_ID_FUNCTIONS_SORT_LINE,		FunctionsPanel::OnRedrawEvent)
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

	FunctionsPanel::FunctionsPanel(IBottomNotePanel &bottom_book, int id, ITopNotePanel &top_book)
		: wxPanel(bottom_book.GetWxWindow(), id),
		PaneMixIn(bottom_book, id, this),
		m_TopNotePanel(top_book),
		m_LineSortRadio(this, RADIO_ID_FUNCTIONS_SORT_LINE, "Line", wxDefaultPosition, wxDefaultSize, wxRB_GROUP),
		m_AlphaSortRadio(this, RADIO_ID_FUNCTIONS_SORT_ALPHA, "Alpha", wxDefaultPosition, wxDefaultSize),
		m_TextCtrl(this, CONTROL_ID_FUNCTIONS_TEXT, "", wxDefaultPosition, wxDefaultSize, styl)
{
	// (uses COW)
	wxFont	ft(wxFontInfo(9).Family(wxFONTFAMILY_MODERN).Encoding(wxFONTENCODING_DEFAULT));
	SetFont(ft);
	
	m_BlackAttr = wxTextAttr(*wxBLACK, *wxWHITE, ft);
	
	m_HighlightAttr = wxTextAttr(*wxBLACK, COLOR_SELECTED_FUNCTION, ft);
	
	wxBoxSizer	*top_hsizer = new wxBoxSizer(wxHORIZONTAL);
	
	top_hsizer->Add(&m_LineSortRadio, wxSizerFlags(0).Align(wxALIGN_CENTER_VERTICAL));
	top_hsizer->Add(&m_AlphaSortRadio, wxSizerFlags(0).Align(wxALIGN_CENTER_VERTICAL));
	
	wxBoxSizer	*v_sizer = new wxBoxSizer(wxVERTICAL);
	
	v_sizer->Add(top_hsizer, wxSizerFlags(0).Border(wxALL, 2).Expand());
	v_sizer->Add(&m_TextCtrl, wxSizerFlags(1).Border(wxALL, 2).Expand());
	
	SetSizer(v_sizer);
	
	m_TextCtrl.SetCursor(wxCURSOR_ARROW);
	
	m_FunctionBookmarks.clear();
	m_LineToPosLUT.clear();
	m_SFC = nil;
	m_SelectedRow = -1;
	
	MixConnect(this, &FunctionsPanel::OnPanelClearVariables, bottom_book.GetSignals().OnPanelClearVariables);
	MixConnect(this, &FunctionsPanel::OnPanelRequestReload, bottom_book.GetSignals().OnPanelRequestReload);
	
	// MixConnect(this, &FunctionsPanel::OnSfcSelection, top_book.GetSignals().OnSfcSelectionChanged);
	// MixConnect(this, &FunctionsPanel::OnUpdateSfcLuaFunctions, top_book.GetSignals().OnSfcContentChanged);
	// MixConnect(this, &FunctionsPanel::OnUpdateSfcCursor, top_book.GetSignals().OnSfcCursorChanged);
	
	m_TextCtrl.Bind(wxEVT_LEFT_DOWN, &FunctionsPanel::OnLeftDownEvent, this);
	m_TextCtrl.Bind(wxEVT_LEFT_DCLICK, &FunctionsPanel::OnLeftDownEvent, this);
	m_TextCtrl.Bind(wxEVT_MOTION, &FunctionsPanel::OnMouseScrubEvent, this);
}

//---- DTOR -------------------------------------------------------------------

	FunctionsPanel::~FunctionsPanel()
{
	m_TextCtrl.Unbind(wxEVT_LEFT_DOWN, &FunctionsPanel::OnLeftDownEvent, this);
	m_TextCtrl.Unbind(wxEVT_MOTION, &FunctionsPanel::OnMouseScrubEvent, this);
}

//---- On Redraw event --------------------------------------------------------

void	FunctionsPanel::OnRedrawEvent(wxCommandEvent &e)
{
	ISourceFileClass	*sfc = m_TopNotePanel.GetSelectedSFC();			// not thread-safe?
	
	OnUpdateSfcLuaFunctions(sfc);
}

void	FunctionsPanel::OnPanelClearVariables(void)
{
	m_FunctionBookmarks.clear();
	m_TextCtrl.ChangeValue("");
	m_LineToPosLUT.clear();
}

void	FunctionsPanel::OnPanelRequestReload(void)
{
	ISourceFileClass	*sfc = m_TopNotePanel.GetSelectedSFC();			// not thread-safe?
	
	OnUpdateSfcLuaFunctions(sfc);
}

//---- ON SFC Selection event ------------------------------------------------

void	FunctionsPanel::OnSfcSelection(ISourceFileClass *sfc)
{
	uLog(UI, "FunctionsPanel::OnSfcSelection(%p)", sfc);
	
	OnUpdateSfcLuaFunctions(sfc);
}

//---- Update SFC Lua Functions -----------------------------------------------
	
void	FunctionsPanel::OnUpdateSfcLuaFunctions(ISourceFileClass *sfc)
{	
	m_SFC = sfc;
	m_FunctionBookmarks.clear();
	m_SelectedRow = -1;
	m_LineToPosLUT.clear();
	
	if (!sfc)
	{	// no selected SFC
		m_TextCtrl.ChangeValue("");
		return;
	}
	
	// if (!m_TopNotePanel.HasValidLuaFunctions())		return;			// functions not updated yet
	
	// get Lua functions
	m_FunctionBookmarks = m_TopNotePanel.GetEditor().GetLuaFunctions();
	
	m_LineToSortedTab.clear();
	
	for (int i = 0; i < m_FunctionBookmarks.size(); i++)
		m_LineToSortedTab.push_back(i);
	
	const bool	alpha_sort_f = m_AlphaSortRadio.GetValue();
	if (alpha_sort_f && (m_LineToSortedTab.size() > 0))
	{	sort(m_LineToSortedTab.begin(), m_LineToSortedTab.end(), [&](const int &i1, const int &i2)
				{
					return (m_FunctionBookmarks[i1].Name() < m_FunctionBookmarks[i2].Name());
				});
	}
	
	m_SortedToLineTab.resize(m_LineToSortedTab.size(), -1);
	
	for (int i = 0; i < m_LineToSortedTab.size(); i++)
	{
		const int	sorted_ln = m_LineToSortedTab[i];
		
		m_SortedToLineTab[sorted_ln] = i;
	}
	
	stringstream	ss;
	
	for (const auto &i : m_LineToSortedTab)
	{	
		const size_t	pos1 = ss.tellp();
		
		const auto	&bmk = m_FunctionBookmarks[i];
		
		ss << bmk.Name() << "() : " << bmk.Line();
		
		m_LineToPosLUT.push_back({pos1, ss.tellp()});
		
		ss << "\n";
	}
	
	m_TextCtrl.Freeze();
	
	m_TextCtrl.ChangeValue(ss.str());
	
	ColorMe();
	
	m_TextCtrl.Thaw();
	
	m_SelectedRow = -1;
	
	// OnUpdateSfcCursor(sfc);
}

//---- On Update SFC Cursor ---------------------------------------------------

void	FunctionsPanel::OnUpdateSfcCursor(ISourceFileClass *sfc, int carret_ln)
{	
	if (!sfc)	return;
	
	for (int i = 0; i < m_FunctionBookmarks.size(); i++)
	{
		const Bookmark	bmk = m_FunctionBookmarks[i];
		
		if (bmk.Line() < carret_ln)		continue;
		
		const int	ind = (bmk.Line() == carret_ln) ? i : (i - 1);
		if (ind < 0)
		{
			m_SelectedRow = -1;		// before 1st function
			break;
		}
		
		assert(ind < m_SortedToLineTab.size());
		m_SelectedRow = m_SortedToLineTab[ind];
		break;
	}
	
	m_TextCtrl.Freeze();
	
	ColorMe();
	
	m_TextCtrl.Thaw();
}

//---- Color Me ---------------------------------------------------------------

void	FunctionsPanel::ColorMe(void)
{
	if (m_LineToPosLUT.empty())		return;
	
	const size_t	n_lines = m_LineToPosLUT.size();
	
	const size_t	n_chars = m_TextCtrl.GetValue().size();
	if (!n_chars)
	{	assert(n_lines == 0);
		return;
	}
	
	// set all to black
	m_TextCtrl.SetStyle(0, n_chars - 1, m_BlackAttr);
		
	if (m_SelectedRow < 0)		return;
	
	assert(m_SelectedRow < m_LineToPosLUT.size());
		
	const Span	&span = m_LineToPosLUT[m_SelectedRow];
	
	const size_t	pos1 = span.first;
	const size_t	pos2 = span.second;
	
	assert(pos1 < n_chars);
	assert(pos2 < n_chars);

	m_TextCtrl.SetStyle(pos1, pos2, m_HighlightAttr);
	
	m_TextCtrl.ShowPosition(pos1);						// loses focus - FIXME?
}

//---- On Left Mouse Button Click event ---------------------------------------

void	FunctionsPanel::OnLeftDownEvent(wxMouseEvent &e)
{
	if (!m_SFC)					return;		// no selected SFC
	// if (!m_SFC->HasValidLuaFunctions())		return;		// functions not updated yet
	
	wxTextCoord	col, row;
	
	const wxTextCtrlHitTestResult	res = m_TextCtrl.HitTest(e.GetPosition(), &col, &row);
	if (res != wxTE_HT_ON_TEXT)			return;		// didn't click on text
	
	if (row >= m_LineToSortedTab.size())		return;		// click overflow (?)
	assert(row < m_LineToSortedTab.size());
	
	const int	sorted_row = m_LineToSortedTab[row];
	
	const Bookmark	&bmk = m_FunctionBookmarks[sorted_row];
	
	m_SFC->ShowLine(bmk.Line(), true/*center*/);
	m_SFC->SetFocus();
}

//---- INSTANTIATE ------------------------------------------------------------

// static
IPaneMixIn*	IPaneMixIn::CreateFunctionListCtrl(IBottomNotePanel &bottom_book, int id, ITopNotePanel &top_book)
{
	return new FunctionsPanel(bottom_book, id, top_book);
}


// nada mas
