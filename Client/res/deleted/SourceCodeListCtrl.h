// source list control header
//   to deprecate for wxStyledTextCtrol -- FIXME

#ifndef LUA_DDT_SOURCE_CODE_LIST_CONTROL_H_INCLUDED
#define LUA_DDT_SOURCE_CODE_LIST_CONTROL_H_INCLUDED

#include "wx/wx.h"
#include "wx/listctrl.h"

#include "TopNotebook.h"
#include "SourceFileClass.h"

//---- Source Code ListCtrl ---------------------------------------------------

class SourceCodeListCtrl: public wxListCtrl, public SourceCodeCtrl_ABSTRACT
{
public:
	// ctor
	SourceCodeListCtrl(TopNotebook *parent, int id, SourceFileClass *sfc, const wxString &fpath);
	// dtor
	virtual ~SourceCodeListCtrl();
	
	// implementations
	void	ShowLine(int highlight_ln);
	int	GetSelectedLine(void);
	void	RemoveHighlight(void);
	void	DoRefresh(void);
	int	GetTotalLines(void);
	void	ClearAnnotations(void);
	void	SetAnnotation(int ln, const wxString &annotation_s);	// nop with listCtrl ?
	
	void	ReloadFile(const wxString &fpath);		// dummy
	
	wxWindow*	GetWin(void);
	
private:
	
	// events
	void	OnMouseLeftDown(wxMouseEvent &e);
	void	OnWindowResized(wxSizeEvent &e);
	void	OnRefreshBreakpoints_SYNTH(wxUpdateUIEvent &e);
	
	void	RemoveAllBreakpointImages(void);
	
	int		m_LastHighlightedLine;
	int		m_IconWidth;
	wxWindow	*m_win;
	
	DECLARE_EVENT_TABLE()
};

#endif // LUA_DDT_SOURCE_CODE_LIST_CONTROL_H_INCLUDED

// nada mas