// Styled source control with Lua lexer header

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "lx/juce/jeometry.h"

#include "wx/wx.h"
#include "wx/stc/stc.h"

#include "sigImp.h"

#include "UIConst.h"

class wxWindow;
class wxImageList;
class wxStyledTextCtrl;

namespace juce
{
	class Component;
	
} // namespace juce

namespace DDT_CLIENT
{
enum MARGIN_MARKER_T : int;

class ITopNotePanel;
class ISourceFileClass;

using std::string;
using std::vector;

using LX::iPoint;

enum FIND_FLAG : uint32_t										// should ABSTRACT
{
	NONE		= 0,
	WHOLE_WORD	= wxSTC_FIND_WHOLEWORD,		// #define wxSTC_FIND_WHOLEWORD 0x2
	CASE		= wxSTC_FIND_MATCHCASE,		// #define wxSTC_FIND_MATCHCASE 0x4
	WORD_START	= wxSTC_FIND_WORDSTART,		// #define wxSTC_FIND_WORDSTART 0x00100000
	RE_BASIC	= wxSTC_FIND_REGEXP,		// #define wxSTC_FIND_REGEXP 0x00200000
	RE_POSIX	= wxSTC_FIND_POSIX,		// #define wxSTC_FIND_POSIX 0x00400000
	
	EXT_SHIFT_BASE	= 24,
	EXT_FIND_MASK	= (1ul << EXT_SHIFT_BASE) -1,	// 0x00FFFFFFul,
	EXT_NO_COMMENT	= 1ul << (EXT_SHIFT_BASE + 0),
	EXT_HIGHLIGHT	= 1ul << (EXT_SHIFT_BASE + 1),
};

//---- Search Hit -------------------------------------------------------------

class SearchHit
{
public:
	// CTOR
	SearchHit(const int pos1, const int len, ISourceFileClass *sfc, const int line, const int line_pos);
	
	void			Clear(void);
	int			Pos1(void) const;
	int			Pos2(void) const;
	int			Len(void) const;
	ISourceFileClass&	GetSFC(void) const;
	int			Line(void) const;
	int			GetLinePos(void) const;
	int			LineIndex(void) const;
	
private:
	
	int32_t			m_Pos1;
	int32_t			m_Len;
	ISourceFileClass	*m_Sfc;
	int32_t			m_Line;
	int32_t			m_LinePos;
};

//---- Editor interface -------------------------------------------------------

class IEditorCtrl
{
public:
	virtual ~IEditorCtrl() = default;
	
	virtual wxWindow*		GetWxWindow(void) = 0;
	virtual void			BindToPanel(wxWindow *panel, const size_t retry_ms) = 0;
	
	virtual EditorSignals&		GetSignals(void) = 0;
	
	virtual ISourceFileClass*	GetCurrentSFC(void) const = 0;
	virtual void			LoadFromSFC(ISourceFileClass *sfc) = 0;
	virtual void			SaveToDisk(void) = 0;
	
	virtual iPoint			GetCursorPos(void) const = 0;
	virtual void			SetCursorPos(const iPoint &pt) = 0;
	virtual int			GetTotalLines(void) const = 0;
	
	virtual void			ShowLine(const int ln, const bool focus_f) = 0;
	virtual void			ShowSpan(const int pos1, const int pos2) = 0;
	virtual void			SetFocus(void) = 0;
	
	virtual bool			HasLineMarker(const MARGIN_MARKER_T hl_typ, const int ln) = 0;
	virtual void			ToggleLineMarker(const MARGIN_MARKER_T hl_typ, const int ln, const bool f) = 0;
	virtual void			RemoveLineMarkers(const MARGIN_MARKER_T hl_typ) = 0;
	
	virtual void			RemoveAllHighlights(void) = 0;
	virtual void			RemoveSearchHighlights(void) = 0;
	
	virtual bool			HasLockFlag(void) const = 0;
	virtual void			SetLockFlag(const bool f) = 0;
	
	virtual void			ClearAnnotations(void) = 0;
	virtual void			SetAnnotation(int ln, const string &annotation_s) = 0;
	
	virtual vector<int>		GetBreakpointLines(void) = 0;
	virtual void			SetBreakpointLines(const vector<int> &bp_list) = 0;
	
	virtual bool			IsEditorDirty(void) const = 0;
	virtual void			SetEditorDirty(const bool f) = 0;
	
	virtual int			SearchText(const string &patt, const uint32_t mask, vector<SearchHit> &hit_list) = 0;	// returns [# new hits]
	
	virtual void			SetAllTextLines(const vector<string> &text) = 0;
	virtual vector<string>		GetAllTextLines(void) const = 0;
	virtual string			GetUserSelection(void) = 0;
	
	static
	IEditorCtrl*	CreateStyled(wxWindow *parent_win, int id, ITopNotePanel &topnotebook, wxImageList &img_list);
};

} // namespace DDT_CLIENT

// nada mas
