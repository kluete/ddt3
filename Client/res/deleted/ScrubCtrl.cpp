// Lua DDT Scrub Control

#include <memory>		// for unique_ptr

#include "TopFrame.h"
#include "Controller.h"
#include "TopNotebook.h"

#include "SourceFileClass.h"
#include "StyledSourceCtrl.h"
#include "SearchBar.h"

#include "ScrubCtrl.h"

using namespace std;
using namespace DDT;

const wxColour	SCRUB_COLOR_VISIBLE(0xc0, 0xff, 0xc0, 0xff);	// pale green
const wxColour	SCRUB_COLOR_OFFSCREEN(0xff, 0xc0, 0xc0, 0xff);	// pale red
// const wxColour	SCRUB_COLOR_HIT(255, 240, 50, 0xff);		// yellow
const wxColour	SCRUB_COLOR_HIT(255, 128, 0, 0xff);		// yellow

BEGIN_EVENT_TABLE(ScrubCtrl, wxControl)

	EVT_SIZE(			ScrubCtrl::OnResizeEvent)
	// EVT_MOUSE_EVENTS(		ScrubCtrl::OnMouseEvent)
	
	EVT_PAINT(			ScrubCtrl::OnPaintEvent)

END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

	ScrubCtrl::ScrubCtrl(wxWindow *parent, int id, TopNotePanel *top_note_panel, TopFrame *tf, Controller *controller)
		: wxControl(parent, id, wxDefaultPosition, wxSize(SCRUBBER_WIDTH_PIXELS, -1))
{
	assert(parent);
	assert(top_note_panel);
	assert(tf);
	assert(controller);
	
	m_TopNotePanel = top_note_panel;
	m_TopFrame = tf;
	m_Controller = controller;
	
	m_OwnerSFC = nil;
	m_RefreshRequestedFlag = false;
	
	// bullshit?
	const wxColour	bkg = *wxBLUE;	// wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK);
	const uint8_t	r = bkg.Red();
	const uint8_t	g = bkg.Green();
	const uint8_t	b = bkg.Blue();
	const uint8_t	a = wxALPHA_OPAQUE;
	
	m_DefaultColor = wxColour(r, g, b, a);
	
	m_Bitmap = wxBitmap(SCRUBBER_WIDTH_PIXELS, 20/*dummy*/, 32/*depth*/);
	
	SetBackgroundStyle(wxBG_STYLE_PAINT/*same as custom*/);
}

//---- DTOR -------------------------------------------------------------------

	ScrubCtrl::~ScrubCtrl()
{
}

//---- On Resize Event --------------------------------------------------------

void	ScrubCtrl::OnResizeEvent(wxSizeEvent &e)
{
	e.Skip();
	
	const wxSize	sz = GetClientSize();
	
	m_Bitmap = wxBitmap(sz.x, sz.y, 32/*depth*/);
	
	ClearBitmap();
}

//---- Clear Bitmap -----------------------------------------------------------

void	ScrubCtrl::ClearBitmap(void)
{
	// clear bitmap as transparent
	wxMemoryDC	dc(m_Bitmap);
	
	dc.SetBrush(wxBrush(wxColour(0, 0, 0, 0), wxBRUSHSTYLE_SOLID));
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.DrawRectangle(0, 0, m_Bitmap.GetWidth(), m_Bitmap.GetHeight());
	
	dc.SelectObject(wxNullBitmap);
}

//---- On Mouse Event ---------------------------------------------------------

void	ScrubCtrl::OnMouseEvent(wxMouseEvent &e)
{
	assert(m_Controller);
	
	e.Skip();
	if (!e.LeftIsDown())		return;
	if (!m_OwnerSFC)		return;
	
	double		y = e.GetPosition().y;
	const double	h = GetClientSize().GetHeight();
	
	const stcPos	pos = m_OwnerSFC->GetStcPosition();
	const double	viz_range_ratio = (double) pos.TotalVisibleLines() / pos.TotalLines();
	
	// clamp
	if (y < 0)		y = 0;
	else if (y > h)		y = h;
	
	double	ratio = ((double) y / h) - (viz_range_ratio / 2.0);
	if (ratio < 0)		ratio = 0;
	else if (ratio > 1)	ratio = 1;
	
	// m_OwnerSFC->ScrubToRatio(ratio);
	
	// UpdateScrubBitmap(m_OwnerSFC);
}

//---- Request Refresh --------------------------------------------------------

void	ScrubCtrl::RequestRefresh(void)
{
	if (m_RefreshRequestedFlag)	return;		// already requested
	m_RefreshRequestedFlag = true;
	
	Refresh();
}

//---- Update Scrub Bitmap ----------------------------------------------------

void	ScrubCtrl::UpdateScrubBitmap(SourceFileClass *sfc, const vector<SearchHit> &hit_list)
{
	m_OwnerSFC = sfc;
	if (!sfc)	return;
	
	const int	pix_w = GetClientSize().GetWidth();
	const int	pix_h = GetClientSize().GetHeight();
	
	const stcPos	pos = sfc->GetStcPosition();
	
	const int	total_ln = pos.TotalLines();
	
	const int	first_vis_ln = pos.FirstVisibleLine() - 1;
	assert(first_vis_ln >= 0);
	int	last_vis_ln = first_vis_ln + pos.TotalVisibleLines();
	if (last_vis_ln >= total_ln)	last_vis_ln = total_ln;		// clamp (if source shorter than visible lines)
	
	const double	ln_to_pix = (double) pix_h / total_ln;
	const double	pix_to_ln = (double) total_ln / (double)  pix_h;
	
	wxMemoryDC	dc(m_Bitmap);
	
	unique_ptr<wxGraphicsContext>	gc {wxGraphicsContext::Create(dc)};
	
	// clear bitmap as transparent
	gc->SetBrush(wxBrush(wxColour(0, 0, 0, 0), wxBRUSHSTYLE_SOLID));
	gc->SetPen(*wxTRANSPARENT_PEN);
	gc->DrawRectangle(0, 0, pix_w, pix_h);
	
	gc->SetBrush(wxBrush(SCRUB_COLOR_HIT, wxBRUSHSTYLE_SOLID));
	
	for (const auto &hit : hit_list)
	{
		if (hit.GetSFC() != sfc)	continue;		// result is in other sfc
		
		const int	y_pix = ln_to_pix * hit.Line();
		
		gc->DrawRectangle(0, y_pix, pix_w, 1);
	}
	
	// (will release)
	gc = nil;
	
	dc.SelectObject(wxNullBitmap);
	
	RequestRefresh();
}

//---- On Paint Event ---------------------------------------------------------

void	ScrubCtrl::OnPaintEvent(wxPaintEvent &e)
{
	unique_ptr<wxGraphicsContext>	gc {wxGraphicsContext::Create(this)};
	assert(gc);
	
	wxDouble	w = 0, h = 0;
	gc->GetSize(&w, &h);
	
	// clear rect
	gc->SetBrush(wxBrush(m_DefaultColor, wxBRUSHSTYLE_SOLID));
	gc->SetPen(*wxTRANSPARENT_PEN);
	gc->DrawRectangle(0, 0, w, h);
	
	// gc->SetBrush(wxBrush(*wxBLACK, wxBRUSHSTYLE_SOLID));
	// gc->SetPen(*wxTRANSPARENT_PEN);
	
	// composite-BLIT bitmap
	const wxSize	bm_sz = m_Bitmap.GetSize();
	const double	bm_w = bm_sz.GetWidth();
	const double	bm_h = bm_sz.GetHeight();
	
	// gc->SetCompositionMode(wxCOMPOSITION_ADD);
	gc->SetCompositionMode(wxCOMPOSITION_SOURCE);
	// gc->SetCompositionMode(wxCOMPOSITION_OVER);		// (alpha blend)
	gc->DrawBitmap(m_Bitmap, 0, 0, bm_w, bm_h);
	gc->SetCompositionMode(wxCOMPOSITION_OVER);		// (alpha blend)
	
	m_RefreshRequestedFlag = false;
}

// nada mas