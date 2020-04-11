// wxListCtrl patch so HitTest() returns column ("sub-item")

/* comment in generic wxListCtrl implementation:
	// TODO: sub item hit testing
	long wxGenericListCtrl::HitTest(const wxPoint& point, int& flags, long *) const
	{
	    return m_mainWin->HitTest( (int)point.x, (int)point.y, flags );
	}
*/

/*
! BAD, should NOT compile
	const int	item = e.GetItem();
		
@ listbase.h:323

	// this conversion is necessary to make old code using GetItem() to compile
	operator long() const { return m_itemId; }

- used by /src/generic/filectrlg.cpp
*/

#include <cassert>

#include "wx/wx.h"
#include "wx/bitmap.h"
#include "wx/colour.h"

const wxColour	BG_COLOR1(0xff, 0xff, 0xff, 0xff);
const wxColour	BG_COLOR2(0xf0, 0xf0, 0xff, 0xff);	// light blue

#include "wx/renderer.h"		// include ORDER is critical
// #include "wx/graphics.h"

#include "ClientCore.h"
#include "UIConst.h"

#include "VarViewCtrl.h"
#include "wxListPatch.h"

#include "lx/celldata.h"

using namespace std;
using namespace DDT;
using namespace LX;

	ListPatchRenderer::ListPatchRenderer(wxImageList *img_list)
{
	assert(img_list);
	
	m_ImageList = img_list;
	m_FontLUT.clear();	
}

//---- Create Font ------------------------------------------------------------

void	ListPatchRenderer::CreateFont(const int &id, const std::string &font_family, const double &pts, const Color &fore, const int &style_mask)
{
	assert(m_FontLUT.count(id) == 0);
	
	const bool	bold_f = style_mask & FONT_STYLE_BOLD;
	const bool	italic_f = style_mask & FONT_STYLE_ITALIC;
	const bool	underline_f = style_mask & FONT_STYLE_UNDERLINED;
	
	// uses
	wxFont	ft(wxFontInfo(pts).Family(wxFONTFAMILY_MODERN).Bold(bold_f).Italic(italic_f).Underlined(underline_f));
	
	m_FontLUT.insert({id, font_def{ft, wxColour(fore.r() * 255.0, fore.g() * 255.0, fore.b() * 255.0, 0xff)}});
}

//=============================================================================

//---- Private List -----------------------------------------------------------

class PrivateList : public wxListCtrl, public CellDataStore
{
public:
	// ctor
	PrivateList(ListPatch *parent, int id, const int list_ctrl_flags, wxImageList *img_list, VarViewCtrl &owner)
		: wxListCtrl(parent, id, wxDefaultPosition, wxDefaultSize, list_ctrl_flags)
		, m_ListPatch(*parent), m_Owner(owner), m_DummyCellData(0, 0)
	{
		// assign images (list is managed by me)
		SetImageList(img_list, wxIMAGE_LIST_SMALL/*which*/);
		SetLabel("VarListCtrl");
	
		InitVars();
		
		// also re-fetched in real-time
		m_HeaderHeight = wxRendererNative::GetGeneric().GetHeaderButtonHeight(this);
	}
	
	// dtor
	virtual ~PrivateList()
	{
	}
	virtual void		SetDataDirty(void) override
	{
	}
	virtual void		DeleteAllCells(void)  override
	{
		wxListCtrl::DeleteAllItems();
		InitVars();
		
	}
	virtual	bool		HasCellData(const int &row, const int &col) const override
	{
		uLog(LIST_CTRL, "HasCellData(%d, %d)", row, col);
		
		return true;
	}

	virtual	CellDataStore&	SetCellData(const int &row, const int &col, const string &s, const int &font_id, const int &img_id)  override
	{
		// for watches
		return *this;
	}
	
	virtual	CellDataStore&	SetCellDataList(const vector<CellData> &cell_data_list) override
	{
		// NOT IMPLEMENTED ?
		return *this;
	}
	
	virtual	bool		EraseCellData(const int &row, const int &col)  override
	{
		// for watches
		return true;
	}
	
	virtual	const CellData*	GetCellDataPtr(const int &row, const int &col) override		// used to be const
	{
		uLog(LIST_CTRL, "GetCellDataPtr(%d, %d)", row, col);
		
		return m_Owner.GetCellDataPtr(row, col);
	}
	
	virtual	CellData&	GetCellDataRef(const int &row, const int &col)  override
	{
		return m_DummyCellData;
	}
	
	void		InitVars(void);
	void		DoResizeColumns(void);
	bool		IsEditable(void) const;
	LX::HitItem	HitTest2(const wxPoint org_pt);
	
	int		GetSelectedItem(void)
	{
		return GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	}
	
	int		GetFocusedItem(void);
	void		UnfocusAllItems(void);
	void		DoDrawCellOverlays(void);
	
private:

	bool	PostEvent(GridEvent e);
	
	// implementations
	void	OnItemCellBatchStart(const int from, const int to, const int header_height, const int line_height, const wxPoint orig) override;
	void	OnItemCellLineStart(const int row, const bool cached_f, const wxRect &rc) override;
	void	OnItemCellLineEnd(const int row) override;
	void	OnItemCellDraw(const int row, const int col, const int img_id, const wxRect &rc) override;
	void	OnItemCellBatchEnd(void) override;
	
	// events
	void	OnMouseHover(wxMouseEvent &e);
	
	void	OnLeaveWindowEvent(wxMouseEvent &e)
	{
		e.Skip();
		
		// do NOT log on focus change!
		
		PostEvent(GridEvent(GRID_EVENT::LEAVE_WINDOW));
	}
	
	void	OnMouseLeftDown(wxMouseEvent &e);
	
	void	OnListItemSelected(wxListEvent &e)
	{
		PostEvent(GridEvent(GRID_EVENT::ITEM_SELECT, HitItem::Make(this, e.GetIndex()/*row*/)));
		
		e.Veto();
	}

	void	OnListItemActivated(wxListEvent &e)
	{
		PostEvent(GridEvent(GRID_EVENT::ITEM_ACTIVATED, HitItem::Make(this, e.GetIndex()/*row*/)));
	}
	
	void	OnListItemEditEnd(wxListEvent &e);
	void	OnItemRightClick(wxListEvent &e);
	void	OnKeyDown(wxKeyEvent &e);
	
	void	SaveBackGround(void);
	void	RestoreBackGround(void);
	
	// (debug)
	void	DrawCellOverlays(void);
	
	ListPatch	&m_ListPatch;
	VarViewCtrl	&m_Owner;
	
	int		m_HeaderHeight, m_LineHeight;
	int		m_LineFrom, m_LineTo;
	wxPoint		m_CellOrigin;
	
	unordered_map<int, Rect>		m_RowToLineRect;
	unordered_map<int, vector<HotRect>>	m_RowToColMap, m_OldHotMap;
	
	CellData	m_DummyCellData;
	
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(PrivateList, wxListCtrl)

	EVT_MOTION(							PrivateList::OnMouseHover)
	EVT_LEAVE_WINDOW(						PrivateList::OnLeaveWindowEvent)
	EVT_LEFT_DOWN(							PrivateList::OnMouseLeftDown)
	EVT_LIST_ITEM_RIGHT_CLICK(	-1,				PrivateList::OnItemRightClick)
	
	EVT_LIST_ITEM_SELECTED(		-1,				PrivateList::OnListItemSelected)
	EVT_LIST_ITEM_ACTIVATED(	-1,				PrivateList::OnListItemActivated)
	EVT_KEY_DOWN(							PrivateList::OnKeyDown)
	
	EVT_LIST_END_LABEL_EDIT(	-1,				PrivateList::OnListItemEditEnd)

	// (not implemented)
	// EVT_LIST_COL_CLICK
	// EVT_LIST_COL_RIGHT_CLICK
	
	// useless?
	// EVT_LIST_ITEM_FOCUSED(	LISTCTRL_ID_VAR_VIEW,		PrivateList::OnItemFocused)
	
END_EVENT_TABLE()

//---- Init Vars --------------------------------------------------------------

void	PrivateList::InitVars(void)
{
	m_LineFrom = m_LineTo = -1;
	m_HeaderHeight = m_LineHeight = -1;
	
	m_RowToColMap.clear();
	m_RowToLineRect.clear();
	m_OldHotMap.clear();
}

bool	PrivateList::IsEditable(void) const
{
	const bool	f = HasFlag(wxLC_EDIT_LABELS);
	
	return f;
}

//---- Post grid event --------------------------------------------------------

bool	PrivateList::PostEvent(GridEvent e)
{
	m_Owner.OnWxGridEvent(e);
	
	return e.WasSkipped();
}

//---- On Mouse Hover ---------------------------------------------------------

void	PrivateList::OnMouseHover(wxMouseEvent &e)
{
	wxPoint	pt {e.GetPosition()};
	
	const HitItem	hit = HitTest2(pt);
	
	PostEvent(GridEvent(GRID_EVENT::CELL_HOVER, hit));
}

//---- On Mouse Left Click ----------------------------------------------------

void	PrivateList::OnMouseLeftDown(wxMouseEvent &e)
{
	const wxPoint	pt {e.GetPosition()};
	uLog(LIST_CTRL, "PrivateList::OnMouseLeftDown(%d, %d)", pt.x, pt.y);
	
	const HitItem	hit = HitTest2(pt);
	if (hit.IsOK())
	{	int	pos = GetScrollPos(wxVERTICAL);
		uLog(LIST_CTRL, "  HIT item %d:%d (vistop %d, vscroll %d)", hit.row(), hit.col(), GetTopItem(), pos);
		
		const bool	skip_f = PostEvent(GridEvent(GRID_EVENT::CELL_LEFT_CLICK, hit));
		if (!skip_f)		return;		// don't skip
		
		e.Skip(true);
	}
	
	// let watches get selected
	if (IsEditable())	e.Skip();
}

//---- On Mouse Right Click ---------------------------------------------------

void	PrivateList::OnItemRightClick(wxListEvent &e)
{
	// doesn't work with wx vanilla if no listItem is focused
	const wxPoint	pt {e.GetPoint()};
	
	const HitItem	hit = HitTest2(pt);
	if (!hit.IsOK())	return;
	
	PostEvent(GridEvent(GRID_EVENT::CELL_RIGHT_CLICK, hit));
}

//---- On Key Down ------------------------------------------------------------

void	PrivateList::OnKeyDown(wxKeyEvent &e)
{	
	if (!IsEditable())	return e.Skip();
	
	const int	vk = e.GetKeyCode();
 	if (vk == 0)	return;
	
	const int	row = GetSelectedItem();
	if (row == -1)	return;
	
	switch (vk)
	{	case WXK_DELETE:
			PostEvent(GridEvent(GRID_EVENT::ITEM_DELETE,  HitItem::Make(this, row)));
			break;
		
		default:
		
			// start editing watch?
			break;
	}
	
	e.Skip();
}

//---- On List Item EDIT End --------------------------------------------------

void	PrivateList::OnListItemEditEnd(wxListEvent &e)
{
	GridEvent	ge(GRID_EVENT::CELL_EDITED,  HitItem::Make(this, e.GetIndex()/*row*/));
	ge.SetString(e.GetLabel().ToStdString()).m_EditOK = !e.IsEditCancelled();
	
	PostEvent(ge);
}

//=============================================================================

//---- Cell BATCH start (many rows) -------------------------------------------

void	PrivateList::OnItemCellBatchStart(const int from, const int to, const int header_height, const int line_height, const wxPoint orig)
{
	uLog(LIST_BBOX, "OnItemCellBatch START, rows [%d; %d], line height %d", from, to, line_height);
	
	m_LineFrom = from;
	m_LineTo = to;
	m_HeaderHeight = header_height;
	m_LineHeight = line_height;
	m_CellOrigin = orig;
	
	m_OldHotMap = m_RowToColMap;
	m_RowToColMap.clear();
	m_RowToLineRect.clear();
}

//---- Cell ROW start (one row) ------------------------------------------------

void	PrivateList::OnItemCellLineStart(const int row, const bool cached_f, const wxRect &rc)
{
	uLog(LIST_BBOX, "  OnItemCellLine row %d %s (y %d, height %d)", row, cached_f ? "CACHED" : "NEW", rc.y, rc.height);
	
	if (cached_f)
	{	// cached, copy from old row
		m_RowToColMap[row] = m_OldHotMap[row];
	}
	
	// update line bounding box in both cases
	m_RowToLineRect.insert({row, Rect(rc.x, rc.y, rc.width, rc.height)}); 
}

//---- Cell ROW end -----------------------------------------------------------

void	PrivateList::OnItemCellLineEnd(const int row)
{
	// remove bounding rect if had no col (cell) entries
	if (m_RowToColMap.count(row) == 0)	m_RowToLineRect.erase(row);
}

//---- On Cell entry ----------------------------------------------------------

void	PrivateList::OnItemCellDraw(const int row, const int col, const int img_id, const wxRect &rc)
{
	if (img_id == BITMAP_ID_EMPTY_PLACEHOLDER)	return;		// ignore this (empty) image
	uLog(LIST_BBOX, "    OnItemCellDraw row %d, col %d", row, col);
	
	const HotRect	hr {row, col, (img_id >= 0) ? HIT_TYPE::ICON : HIT_TYPE::TEXT, Rect(rc.x, rc.y, rc.width, rc.height)};
	
	m_RowToColMap[row].push_back(hr);
}

//---- Cell BATCH end ---------------------------------------------------------

void	PrivateList::OnItemCellBatchEnd(void)
{
	assert(m_RowToColMap.size() == m_RowToLineRect.size());
	uLog(LIST_BBOX, "OnItemCellBatch END, %zu lines", m_RowToColMap.size());
}

//---- Hit Test 2 -------------------------------------------------------------

HitItem	PrivateList::HitTest2(const wxPoint org_pt)
{
	// all tests done in LOGICAL coordinates so caching works
	const wxPoint	logical_pt0 = org_pt - m_CellOrigin;
	const Point	logical_pt(logical_pt0.x, logical_pt0.y);
	
	// uLog(LIST_CTRL, "HitTest2() org (%d, %d), logical (%d, %d)", org_pt.x, org_pt.y, logical_pt.x, logical_pt.y);
	
	for (const auto &it : m_RowToColMap)
	{
		const int	row = it.first;
		if (m_RowToLineRect.count(row) == 0)		continue;	// no cells in this line
		
		const Rect	&row_rect = m_RowToLineRect.at(row);
		if (!row_rect.contains(logical_pt))		continue;	// doesn't intersect this line
		
		// test line cols
		const vector<HotRect>	&rect_list = it.second;
		
		// scan columns
		for (const HotRect &hr : rect_list)
		{
			if (!hr.m_rect.contains(logical_pt))	continue;	// doesn't interect col
			// found
			
			HitItem	hit(this, hr.m_row, hr.m_col, hr.m_type);
			
			return hit;
		}
	}
	
	return HitItem();	// not found
}

//---- Focus Clusterfuck ------------------------------------------------------

int	PrivateList::GetFocusedItem(void)
{
	return GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_FOCUSED);
}

void	PrivateList::UnfocusAllItems(void)
{
	size_t	cnt = 0;
	int	item_ind = -1;

	while ((item_ind = GetFocusedItem()) != -1)
	{
		bool	ok = SetItemState(item_ind, 0/*state*/, wxLIST_STATE_FOCUSED/*mask*/);
		assert(ok);
		
		cnt++;
	}
	
	uLog(UI, "unfocused %d items", cnt);
}

//---- Do Resize Columns ------------------------------------------------------

void	PrivateList::DoResizeColumns(void)
{
	// calc col 0 max width & assign background colors
	wxListItem	li;
	
	bool	ok = GetColumn(0, li/*&*/);
	assert(ok);
	
	int	max_col0_w = GetTextExtent(li.GetText()).GetWidth();
	
	for (int row = 0; row < GetItemCount(); row++)
	{	// get text extent
		const int	w = GetTextExtent(GetItemText(row, 0/*col*/)).GetWidth();
		// save maxima
		if (w > max_col0_w)		max_col0_w = w;
		
		// whole row color
		const wxColour	back_clr {(row & 1L) ? BG_COLOR2 : BG_COLOR1};
		// SetItemBackgroundColour(row, back_clr);
	}
	
	ok = SetColumnWidth(0, DDT::ICON_SIZE + 16 + max_col0_w);	// HACK
	assert(ok);
	
	// auto-size remaining columns AFTER filled the list (otherwise will be zero)
	#ifdef __WIN32__
		const int	AUTO_SZ = wxLIST_AUTOSIZE_USEHEADER;
	#else
		const int	AUTO_SZ = wxLIST_AUTOSIZE;
	#endif
	
	for (int col = 1; col < GetColumnCount(); col++)
	{
		ok = SetColumnWidth(col, AUTO_SZ);
		assert(ok);
	}
}

//=============================================================================

//---- CTOR -------------------------------------------------------------------

	ListPatch::ListPatch(VarViewCtrl *parent, const int id, ListPatchRenderer *renderer, const uint32_t &flags)
		: wxWindow(parent, id, wxDefaultPosition, wxDefaultSize, wxNO_BORDER), m_Renderer(*renderer)

{
	assert(parent);
	assert(renderer);
	
	m_DefaultFontID = -1;
	
	wxSizer	*v_sizer = new wxBoxSizer(wxVERTICAL);
	
	const int	list_ctrl_flags = wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES | ((flags & VAR_FLAG_EDITABLE) ? wxLC_EDIT_LABELS : 0);
	
	m_ListCtrl = new PrivateList(this, -1, list_ctrl_flags, m_Renderer.m_ImageList, *parent);
	assert(m_ListCtrl);
	
	v_sizer->Add(m_ListCtrl, wxSizerFlags(1).Expand());
	
	SetSizer(v_sizer);
}

//---- DTOR -------------------------------------------------------------------

	ListPatch::~ListPatch()
{
}

//---- Delete All (grid) Columns ----------------------------------------------

ListPatch&	ListPatch::DeleteAllColumns(void)
{
	uLog(LIST_CTRL, "ListPatch::DeleteAllColumns(), top item = %d", m_ListCtrl->GetTopItem());
	
	m_ListCtrl->ClearAll();		// deletes columns too
	DeleteAllCells();
	
	return *this;
}

//---- Add Column -------------------------------------------------------------

void	ListPatch::AddColumn(const string &title, const COLUMN_STYLE_FLAG &flags)
{
	// can NOT test COL_ALIGN_LEFT since is a non-flag!
	wxListColumnFormat	fmt = (flags & COL_ALIGN_RIGHT) ? wxLIST_FORMAT_RIGHT : wxLIST_FORMAT_LEFT;
	
	m_ListCtrl->AppendColumn(wxString(title), fmt);
}

//---- Delete All Cells -------------------------------------------------------

void	ListPatch::DeleteAllCells(void)
{
	m_ListCtrl->DeleteAllCells();
}

size_t	ListPatch::GetNumRows(void) const
{
	return m_ListCtrl->GetItemCount();
}

size_t	ListPatch::GetNumCols(void) const
{
	return m_ListCtrl->GetColumnCount();
}

int	ListPatch::GetVisibleRowFrom(void)
{
	int	top = m_ListCtrl->GetTopItem();
	
	uLog(LIST_CTRL, "ListPatch::GetVisibleRowFrom() top %d", top);
	
	return top;
}

//---- Get Current Scroll Position --------------------------------------------

Point	ListPatch::GetCurrentScrollPos(void)
{
	const int	pos = m_ListCtrl->GetScrollPos(wxVERTICAL);
	
	return Point(0, pos);
}

//---- Set Cell Data ----------------------------------------------------------

void	ListPatch::SetCellData(const int &row, const int &col, const string &s, const int &font_id, const int &icon_id)
{
	assert(row >= 0);
	assert((col >= 0) && (col < m_ListCtrl->GetColumnCount()));
	
	assert(m_Renderer.m_FontLUT.count(font_id) > 0);
	const font_def	&ft_def = m_Renderer.m_FontLUT.at(font_id);
	
	const int	n_rows = m_ListCtrl->GetItemCount();
	if (row >= n_rows)
	{
		const int n_insert = (n_rows - row) + 1;
		for (int i = 0; i < n_insert; i++)	m_ListCtrl->InsertItem(row, " ");
	}
	
	wxListItem	li;
	
	li.m_itemId = row;
	li.SetColumn(col);
	li.SetText(wxString(s));
	li.SetFont(ft_def.m_Font);
	li.SetTextColour(ft_def.m_ForeClr);
	
	if (icon_id >= 0)	li.SetImage(icon_id);
	
	m_ListCtrl->SetItem(li);
}

//---- Set Cell Data (struct) -------------------------------------------------

void	ListPatch::SetCellData(const CellData &cdata)
{
	SetCellData(cdata.row(), cdata.col(), cdata.GetText(), cdata.GetFontID(), cdata.GetIconID());
}

//---- Set CellData List ------------------------------------------------------

void	ListPatch::SetCellDataList(const vector<CellData> &cell_data_list)
{
	for (const auto &it : cell_data_list)	SetCellData(it);
}

//---- Show Cell --------------------------------------------------------------

bool	ListPatch::ShowCell(const size_t &row, const size_t &col, const bool &center_f)
{
	const int	vis_rows = m_ListCtrl->GetCountPerPage();
	
	int	new_pos = row - vis_rows;
	if (new_pos < 0)	new_pos = 0;
	
	uLog(LIST_CTRL, "ListPatch::ShowCell(), row %d, new pos %d", row, new_pos);
	
	m_ListCtrl->EnsureVisible(new_pos);
	
	return true;
}
	
//---- Request Refresh --------------------------------------------------------

void	ListPatch::RequestRefresh(void)
{
	m_ListCtrl->DoResizeColumns();
	
	m_ListCtrl->Refresh();
}

ListPatch&	ListPatch::FlushScroll(void)
{
	RequestRefresh();
	
	return *this;
}

//---- Get Selected Item ------------------------------------------------------

int	ListPatch::GetSelectedItem(void)
{
	return m_ListCtrl->GetSelectedItem();
}

//---- Select Item ------------------------------------------------------------

void	ListPatch::SelectItem(const int item_ind)
{
	bool	ok = m_ListCtrl->SetItemState(item_ind, 1/*state*/, wxLIST_STATE_SELECTED/*mask*/);
	assert(ok);
}

bool	ListPatch::EditCellData(HitItem hit)
{
	if (hit.empty())	return false;		// illegal pos
	
	// wx can only edit 1st column
	bool	ok = m_ListCtrl->EditLabel(hit.row());
	assert(ok);
	
	return ok;
}

// nada mas
