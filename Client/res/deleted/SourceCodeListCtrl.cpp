// source code list control

#include "wx/imaglist.h"
#include "wx/textfile.h"

#include "SourceFileClass.h"
#include "UIConst.h"

#include "SourceCodeListCtrl.h"

using namespace std;

const int	LINE_NUM_COL_WIDTH = 40;		// line numbers won't be visible if not WIDE enough (or font too large)

	SourceCodeCtrl_ABSTRACT::SourceCodeCtrl_ABSTRACT(const wxString &fpath, TopNotebook *parent, SourceFileClass *sfc)
{
	wxASSERT(parent);
	wxASSERT(sfc);
	wxASSERT(!fpath.IsEmpty());
	
	m_TopNotebook = parent;
	m_OwnerSource = sfc;
}

BEGIN_EVENT_TABLE(SourceCodeListCtrl, wxListCtrl)
	
	EVT_SIZE(							SourceCodeListCtrl::OnWindowResized)
	EVT_LEFT_DOWN(							SourceCodeListCtrl::OnMouseLeftDown)
	
	// synthetic
	EVT_UPDATE_UI(			DBGF_ID_REFRESH_BREAKPOINTS,	SourceCodeListCtrl::OnRefreshBreakpoints_SYNTH)
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

	SourceCodeListCtrl::SourceCodeListCtrl(TopNotebook *TopNotebook, int id, SourceFileClass *sfc, const wxString &fpath)
		: wxListCtrl(TopNotebook, id, wxDefaultPosition, wxSize(-1, -1), wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL)
		, SourceCodeCtrl_ABSTRACT(fpath, TopNotebook, sfc)
{
	
	m_LastHighlightedLine = -1;
	
	wxImageList	*img_list = TopNotebook->GetImageList();
	
	// assign images (list is managed by me)
	SetImageList(img_list, wxIMAGE_LIST_SMALL);
	
	const int	tab_size = TopNotebook->GetTabSize();
	
	// wxBoxSizer	*vsizer = new wxBoxSizer(wxVERTICAL);
	//vsizer->Add(
	
	wxFont	ft(wxFontInfo(9).Family(wxFONTFAMILY_MODERN).Encoding(wxFONTENCODING_DEFAULT/*wxFONTENCODING_UTF8*/));
	
	SetFont(ft);
	
	// breakpoint icon size
	int	icon_width, icon_height;
	
	bool	ok = img_list->GetSize(0/*first column*/, icon_width/*&*/, icon_height/*&*/);
	wxASSERT(ok);
	
	m_win = nil;
	
	m_IconWidth = icon_width;
	InsertColumn(0, "#");
	InsertColumn(1, "ln");
	
	wxTextFile	tf(fpath);
	
	ok = tf.Open();
	wxASSERT(ok);
	
	// characters-per-tab should be in prefs
	const wxString	tab_replacement = wxString(' ', tab_size);
	
	int	LineCount = tf.GetLineCount();
	
	// dump source file to list control
	for (int i = 0; i < LineCount; i++)
	{	
		// increase the list first with dummy
		InsertItem(i, " ");
		
		// (human-readable lines should be 1-based)
		SetItem(i, 0, wxString::Format("%4d", i + 1));		// forces width to '0000' at MINIMUM
		SetItemImage(i, BITMAP_ID_EMPTY_PLACEHOLDER);		// (instead of breakpoint icon)
		
		wxString	ln = tf.GetLine(i);
		
		// replace tabs with spaces
		ln.Replace("\t", tab_replacement);
		
		SetItem(i, 1, ln);
	}
	
	#ifdef __WIN32__
		const int	AUTO_SZ = wxLIST_AUTOSIZE_USEHEADER;
	#else
		const int	AUTO_SZ = wxLIST_AUTOSIZE;
	#endif
	
	// set to auto-size AFTER filled the list (otherwise will be zero)
	SetColumnWidth(0, LINE_NUM_COL_WIDTH + (icon_width + 1));
	
	const int	col0_w = GetColumnWidth(0);
	const int	client_w = GetClientSize().GetWidth();
	
	SetColumnWidth(1, client_w - col0_w);
}

//---- DTOR -------------------------------------------------------------------

	SourceCodeListCtrl::~SourceCodeListCtrl()
{
}

void	SourceCodeListCtrl::ReloadFile(const wxString &fpath)
{
	wxLogWarning("ReloadFile disabled in listCtrl");
}

void	SourceCodeListCtrl::ClearAnnotations(void)
{
	wxLogWarning("Annotations disabled in listCtrl");
}

void	SourceCodeListCtrl::SetAnnotation(int ln, const wxString &annotation_s)
{
	wxLogWarning("Annotations disabled in listCtrl");
}

//---- On Window Resized ------------------------------------------------------

void	SourceCodeListCtrl::OnWindowResized(wxSizeEvent &e)
{
	// extend 2nd column or clips
	const int	client_w = GetClientSize().GetWidth();
	
	const int	col0_w = GetColumnWidth(0);
	const int	col1_w = client_w - col0_w;
	
	SetColumnWidth(1, col1_w);
	
	e.Skip();
}

//---- Get Seelected Line -----------------------------------------------------

int	SourceCodeListCtrl::GetSelectedLine(void)
{
	const int	item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	return item;
}

//---- Remove All Breakpoint Images -------------------------------------------

void	SourceCodeListCtrl::RemoveAllBreakpointImages(void)
{
	// only set image mask; doesn't change other attributes
	wxListItem	item;
	
	item.m_itemId = 0;
	item.SetImage(BITMAP_ID_EMPTY_PLACEHOLDER);
	
	const int	n_items = GetItemCount();
	for (int i = 0; i < n_items; i++)
	{
		item.m_itemId = i;
		
		SetItem(item);
	}
}

//---- Remove Highlight -------------------------------------------------------

void	SourceCodeListCtrl::RemoveHighlight(void)
{	
	// revert old color
	if (m_LastHighlightedLine != -1)
	{	SetItemBackgroundColour(m_LastHighlightedLine, *wxWHITE);
		
		m_LastHighlightedLine = -1;
	}
	
	// check if has (manually) selected line
	if (-1 == GetSelectedLine())	return;
	
	const bool	ok = SetItemState(m_LastHighlightedLine, 0/*state*/, wxLIST_STATE_SELECTED/*mask*/);
	wxASSERT(ok);
}

//---- Get (common) Window Pointer --------------------------------------------

wxWindow*	SourceCodeListCtrl::GetWin(void)
{
	wxWindow	*win = dynamic_cast<wxWindow*> (this);
	wxASSERT(win);
	m_win = win;
	
	return win;
}

//---- On Refresh Breapoints (synthetic) --------------------------------------

void	SourceCodeListCtrl::OnRefreshBreakpoints_SYNTH(wxUpdateUIEvent &e)
{
	const wxString	payload_s = e.GetString();
	
	wxLogMessage("SourceCodeListCtrl::OnRefreshBreakpoints_SYNTH(\"%s\")", payload_s);
	
	DoRefresh();
}

//---- De Refresh -------------------------------------------------------------

void	SourceCodeListCtrl::DoRefresh(void)
{
	wxASSERT(m_OwnerSource);
	
	// updates ALL breakpoints in one go
	RemoveAllBreakpointImages();
	
	const vector<int>	bp_lines = m_OwnerSource->GetBreakpointLines();
	for (const auto ln : bp_lines)
	{
		SetItemImage(ln - 1, BITMAP_ID_RED_DOT);
	}
	
	Refresh();
}

//---- Get Total Lines --------------------------------------------------------

int	SourceCodeListCtrl::GetTotalLines(void)
{
	return GetItemCount();
}

//---- Show Line --------------------------------------------------------------

void	SourceCodeListCtrl::ShowLine(int highlight_ln)
{
	RemoveHighlight();
	
	// line numbers are always 1-based
	highlight_ln = highlight_ln - 1;
	wxASSERT(highlight_ln >= 0);
	
	// get # of rows visible per page
	int	vis_per_page = GetCountPerPage();
	wxASSERT(vis_per_page >= 0);
	
	// adjust position so is in the middle and clamp
	int	top_line = highlight_ln - vis_per_page / 2;
	if (top_line < 0)	top_line = 0;
	
	// compute scroll delta
	int	item_scroll_delta = top_line - GetTopItem();
	
	// get item rect
	wxRect	rc;
	
	bool	ok = GetItemRect(0, rc/*&*/, wxLIST_RECT_BOUNDS);
	wxASSERT(ok);
	
	int	pix_v_delta = item_scroll_delta * rc.height;
	
	// scroll to top position
	ok = ScrollList(0/*# columns*/, pix_v_delta);
	wxASSERT(ok);
	
	// set new background color to cyan
	SetItemBackgroundColour(highlight_ln, wxColor(0, 255, 255));
	
	m_LastHighlightedLine = highlight_ln;
}

//---- On Mouse Left Down -----------------------------------------------------

void	SourceCodeListCtrl::OnMouseLeftDown(wxMouseEvent &e)
{
	int	flags;
	
	int	ln = HitTest(e.GetPosition(), flags/*&*/);
	if (flags & wxLIST_HITTEST_ONITEMICON)
	{	// clicked debug icon (don't update icon here)
		
		wxASSERT(m_OwnerSource);
		
		#if 0
			// disable all UI-based hackery
			wxListItem	info;
		
			info.Clear();
			info.m_itemId = ln;
			info.m_image = -1;
			info.m_mask = wxLIST_MASK_IMAGE;
			
			bool	ok = GetItem(info);
			wxASSERT(ok);
			
			bool	f = (info.GetImage() == DBGF_ID_BITMAP_RED_DOT);
		#endif
		
		// ignore unreliable UI breakpoint state
		m_OwnerSource->OnClickedBreakpoint(ln + 1);
	}
	else	e.Skip();
}

// nada mas