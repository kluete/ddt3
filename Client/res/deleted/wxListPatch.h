// wxListCtrl patch so HitTest() returns column ("sub-item")

#ifndef DDT_WX_LIST_CONTROL_PATCH_H_INCLUDED
#define	DDT_WX_LIST_CONTROL_PATCH_H_INCLUDED

#include <string>
#include <unordered_map>
#include <vector>

#include "wx/wx.h"
#include "wx/bitmap.h"
#include "wx/colour.h"
#include "wx/listctrl.h"

#include "ddt/CommonClient.h"

#include "lx/geometry.h"
#include "lx/font.h"
#include "lx/column.h"

#include "lx/gridevent.h"

// forward references
class VarViewCtrl;
class PrivateList;

namespace DDT
{
struct font_def
{
	wxFont		m_Font;
	wxColour	m_ForeClr;
};

//---- List Patch Renderer ----------------------------------------------------

class ListPatchRenderer
{
public:
	// ctor
	ListPatchRenderer(wxImageList *img_list);
	// dtor
	virtual ~ListPatchRenderer()	{}
	
	void	CreateFont(const int &id, const std::string &font_family, const double &pts, const LX::Color &fore, const int &style_mask = LX::FONT_STYLE_NONE);
	
// private:
	
	wxImageList				*m_ImageList;
	std::unordered_map<int, DDT::font_def>	m_FontLUT;
};

} // namespace DDT

//---- ListPatch --------------------------------------------------------------

class ListPatch: public wxWindow
{
public:
	// ctor
	ListPatch(VarViewCtrl *m_Parent, const int id, DDT::ListPatchRenderer *renderer, const uint32_t &flags);
	// dtor
	virtual ~ListPatch();
	
	ListPatch&	DeleteAllColumns(void);
	void	AddColumn(const std::string &title, const LX::COLUMN_STYLE_FLAG &flags = LX::COL_ALIGN_LEFT);

	void	DeleteAllCells(void);
	
	void	SetCellData(const int &row, const int &col, const std::string &s = std::string(""), const int &font_id = -1, const int &icon_id = -1);
	void	SetCellData(const LX::CellData &cdata);
	void	SetCellDataList(const std::vector<LX::CellData> &cell_data_list);
	
	size_t	GetNumRows(void) const;
	size_t	GetNumCols(void) const;
	
	int	GetVisibleRowFrom(void);
	
	int	GetSelectedItem(void);
	void	SelectItem(const int item_ind);
	bool	ShowCell(const size_t &row, const size_t &col, const bool &center_f = false);
	
	bool	EditCellData(LX::HitItem hit);
	
	ListPatch&	FlushScroll(void);
	
	void	RequestRefresh(void);

	void	SetDefaultFontID(const int &font_id)
	{
		m_DefaultFontID = font_id;
	}
	
	int	GetDefaultFontID(void) const
	{
		return m_DefaultFontID;
	}
	
	LX::Point	GetCurrentScrollPos(void);
	
	// NOPs
	void		SetHeaderFontID(int)			{}
	void		SetHeaderHeight(int)			{}
	void		SetRowHeight(int)			{}
	ListPatch&	ApplyCellData(void)			{return *this;}
	ListPatch&	ScrollTo_IMP(const int &, const int &)	{return *this;}
	ListPatch&	SetCellOverlayFlag(bool)		{return *this;}
	
private:

	PrivateList		*m_ListCtrl;
	DDT::ListPatchRenderer	&m_Renderer;
	
	int	m_DefaultFontID;
};

#endif // DDT_WX_LIST_CONTROL_PATCH_H_INCLUDED

// nada mas


