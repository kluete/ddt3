// Scintilla styled source control with Lua lexer

/* 	Notes:

	- STC lines are ZERO-based in logic but DISPLAYED 1-based
	- can't use mouse "dwelling" because needs FOCUS
		
	int	styl_bits = GetStyleBits();		// USED to be limited to 5 bits, but not anymore
		
	- "Set the style number for the text margin for a line"
	MarginSetStyle(ln, style)
		
	- ignored by STC
	SetIndicatorValue(int);
	
	- nameless Lua functions EXIST
	- 3rd party lexer is always a HACK with many corner cases
	- Lua keywords 5 are FREE
*/

#include <sstream>
#include <utility>
#include <unordered_set>
#include <unordered_map>
#include <regex>

#include "wx/imaglist.h"
#include "wx/stringops.h"

#include "lx/misc/stopwatch.h"

#include "sigImp.h"
#include "logImp.h"

#include "TopFrame.h"
#include "TopNotebook.h"
#include "SourceFileClass.h"

#include "StyledSourceCtrl.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

// stdPos ctor

//---- STC position -----------------------------------------------------------

class stcPos
{
public:
	stcPos(const wxStyledTextCtrl &stc)
	{
		m_rawPos = stc.GetCurrentPos();
		m_Line = stc.LineFromPosition(m_rawPos);
		m_Col = stc.GetColumn(m_rawPos);
	}
	
	virtual ~stcPos() = default;
	
	int	Line(void) const	{return m_Line;}
	int	Col(void) const		{return m_Col;}
	int	RawPos(void) const	{return m_rawPos;}
	
private:
	
	int	m_rawPos;
	int	m_Line;
	int	m_Col;
};

//---- Mute STC ---------------------------------------------------------------

class AutoMuteStc
{
public:
	AutoMuteStc(IEditorCtrl &ed, wxStyledTextCtrl &stc)
		: m_Editor(ed),
		m_stc(stc),
		m_SavedMask(stc.GetModEventMask()),
		m_SavedLock(ed.HasLockFlag())
	{
		stc.SetModEventMask(0);
		ed.SetLockFlag(false);
	}
	
	~AutoMuteStc()
	{
		m_stc.SetModEventMask(m_SavedMask);
		m_Editor.SetLockFlag(m_SavedLock);
	}
	
private:

	IEditorCtrl		&m_Editor;
	wxStyledTextCtrl	&m_stc;
	
	const int	m_SavedMask;
	const bool	m_SavedLock;
};

//==== Search Hit =============================================================

	SearchHit::SearchHit(const int pos1, const int len, ISourceFileClass *sfc, const int line, const int line_pos)
		: m_Pos1(pos1),
		m_Len(len),
		m_Sfc(sfc),
		m_Line(line),
		m_LinePos(line_pos)
{
	assert(sfc);
}

void	SearchHit::Clear(void)
{	
	m_Pos1 = -1;
	m_Len = 0;
	m_Sfc = nil;
	m_Line = -1;
	m_LinePos = -1;
}

int	SearchHit::Pos1(void) const
{
	return m_Pos1;
}

int	SearchHit::Len(void) const
{
	return m_Len;
}

int	SearchHit::Pos2(void) const
{
	return m_Pos1 + Len();
}

ISourceFileClass&	SearchHit::GetSFC(void) const
{
	return *m_Sfc;
}

int	SearchHit::Line(void) const
{
	return m_Line;
}

int	SearchHit::GetLinePos(void) const
{
	return m_LinePos;
}

int	SearchHit::LineIndex(void) const
{
	return Pos1() - GetLinePos();
}

// colors
const wxColour	COLOR_CARRET_SELECTED_LINE		(215, 243, 243);	// light blue
const wxColour	COLOR_VERTICAL_EDIT_EDGE		(194, 235, 194);	// faded green
const wxColour	COLOR_CURRENT_EXECUTION_LINE		( 40, 255, 255);	// cyan

const wxColour	COLOR_LUA_ERROR_ANNOTATION		(250, 100, 100);	// light red

const wxColour	COLOR_STROKE_BREAKPOINT			(0,     0,   0);
const wxColour	COLOR_FILL_BREAKPOINT			(255,   0,   0);	// red

const wxColour	COLOR_STROKE_SEARCH_HIGHLIGHT		(0,     0,   0);
const wxColour	COLOR_FILL_SEARCH_HIGHLIGHT		(255, 240,  50);	// yellow

const wxColour	COLOR_STROKE_TOOLTIP			(  0,   0,   0);
const wxColour	COLOR_FILL_TOOLTIP			(255, 240,  80);	// bright yellow

const wxColour	COLOR_STROKE_USER_BOOKMARK		(255,   0, 128);	// darker purple
const wxColour	COLOR_FILL_USER_BOOKMARK		(255, 102, 254);	// bright purple

// transparencies
const int	SEARCH_HIGHLIGHT_BRUSH_ALPHA =		130;
const int	SEARCH_HIGHLIGHT_PEN_ALPHA =		150;

const string	STC_FUNCTION_REFRESH_GROUP		= "stc_function_refresh";

// function NAME()
//   const regex	re { "\\bfunction[[:space:]]*([[:alpha:]_][[:alnum:]_\\.\\:]*)\\(" };
// NAME = function(
//   const regex	re { "([[:alpha:]_][[:alnum:]_\\.\\:]*)[[:space:]]*=[[:space:]]*function[[:space:]]*\\(" };

// combo
static const regex	LUA_FUNCTION_RE
{
	"\\bfunction[[:space:]]*([[:alpha:]_][[:alnum:]_\\.\\:]*)\\("
		"|"
	"([[:alpha:]_][[:alnum:]_\\.\\:]*)[[:space:]]*=[[:space:]]*function[[:space:]]*\\(",
	
	regex::ECMAScript | regex::optimize
};

#define lex_def(t)	LUA_LEX_##t = wxSTC_LUA_##t

enum LEXER_T : uint8_t
{
	lex_def(DEFAULT),
	lex_def(COMMENT),
	lex_def(COMMENTLINE),
	lex_def(COMMENTDOC),
	lex_def(NUMBER),
	lex_def(WORD),			// core Lua keywords, word group 0 (enum 5)
	lex_def(STRING),
	lex_def(CHARACTER),
	lex_def(LITERALSTRING),
	lex_def(PREPROCESSOR),
	lex_def(OPERATOR),
	lex_def(IDENTIFIER),		// script variables, including functions (enum 11)
	lex_def(STRINGEOL),
	lex_def(WORD2),
	lex_def(WORD3),
	lex_def(WORD4),
	lex_def(WORD5),
	lex_def(WORD6),			// scipt user functions
	lex_def(WORD7),
	lex_def(WORD8),
	lex_def(LABEL),
	
	LUA_LEX_ILLEGAL = 255
};
#undef lex_def

const int	DDT_USER_VARIABLE	= wxSTC_LUA_IDENTIFIER;
const int	DDT_USER_LUA_FUNCTION	= wxSTC_LUA_WORD6;

enum MARGIN_TYPES : int					// REDUNDANT with enum below!
{
	MARGIN_LINE_NUMBERS		= 0,
	MARGIN_BREAKPOINTS,
	MARGIN_CURRENT_EXECUTION_LINE,
	MARGIN_SELECTED_SEARCH_RESULT,
	MARGIN_USER_BOOKMARK,
};

enum class MARKER_INDEX : int
{
	BREAKPOINT			= 1,		// start at 1 so matches above margins -- or is completely REDUNDANT?
	CURRENT_EXECUTION_LINE,
	SELECTED_SEARCH_RESULT,
	USER_BOOKMARK,
};

const int	SEARCH_INDICATOR_ID = (int) MARKER_INDEX::SELECTED_SEARCH_RESULT;

static const
unordered_map<MARGIN_MARKER_T, MARKER_INDEX, hash<int>>	s_HighlightToMarkerMap
{
	{MARGIN_MARKER_T::EXECUTION_LINE, 	MARKER_INDEX::CURRENT_EXECUTION_LINE},
	{MARGIN_MARKER_T::SEARCH_RESULT_LINE,	MARKER_INDEX::SELECTED_SEARCH_RESULT},
	{MARGIN_MARKER_T::USER_BOOKMARK,	MARKER_INDEX::USER_BOOKMARK},
};

#if 0
static const
unordered_map<int, string>	s_LuaStyleLUT
{
	{wxSTC_LUA_DEFAULT,		"default"},
	{wxSTC_LUA_COMMENT,		"comment"},
	{wxSTC_LUA_COMMENTLINE,		"commentline"},
	{wxSTC_LUA_COMMENTDOC,		"commentdoc"},
	{wxSTC_LUA_NUMBER,		"number"},
	{wxSTC_LUA_WORD,		"word"},			// word group 0 (enum 5)
	{wxSTC_LUA_STRING,		"string"},
	{wxSTC_LUA_CHARACTER,		"character"},
	{wxSTC_LUA_LITERALSTRING,	"litteralstring"},
	{wxSTC_LUA_PREPROCESSOR,	"preprocessor"},
	{wxSTC_LUA_OPERATOR,		"operator"},
	{wxSTC_LUA_IDENTIFIER,		"identifier"},			// Lua variable (enum 11)
	{wxSTC_LUA_STRINGEOL,		"stringeol"},
	{wxSTC_LUA_WORD2,		"word2"},
	{wxSTC_LUA_WORD3,		"word3"},
	{wxSTC_LUA_WORD4,		"word4"},
	{wxSTC_LUA_WORD5,		"word5"},
	{wxSTC_LUA_WORD6,		"word6"},
	{wxSTC_LUA_WORD7,		"word7"},
	{wxSTC_LUA_WORD8,		"word8"},
	{wxSTC_LUA_LABEL,		"label"}
};
#endif

static const
unordered_set<LEXER_T, EnumClassHash>	s_CommentStyleSet
{
	LUA_LEX_COMMENT,
	LUA_LEX_COMMENTLINE,
	LUA_LEX_COMMENTDOC,
	LUA_LEX_LITERALSTRING,						// multiline string/comment
};

static const
unordered_set<LEXER_T, EnumClassHash>	s_DormantStyleSet
{
	LUA_LEX_STRING, LUA_LEX_LITERALSTRING
};

static const
unordered_set<string>	s_IgnoreFunctionsSet
{
	"__index"
};

// wx/stc/stc.h:345
static const
unordered_map<int, string>	s_ModMap =
{
	{wxSTC_MOD_INSERTTEXT,		"MOD_INSERTTEXT"},
	{wxSTC_MOD_DELETETEXT,		"MOD_DELETETEXT"},
	{wxSTC_MOD_CHANGESTYLE,		"MOD_CHANGESTYLE"},
	{wxSTC_MOD_CHANGEFOLD,		"MOD_CHANGEFOLD"},
	{wxSTC_PERFORMED_USER,		"PERFORMED_USER"},
	{wxSTC_PERFORMED_UNDO,		"PERFORMED_UNDO"},
	{wxSTC_PERFORMED_REDO,		"PERFORMED_REDO"},
	{wxSTC_MULTISTEPUNDOREDO,	"MULTISTEPUNDOREDO"},
	{wxSTC_LASTSTEPINUNDOREDO,	"LASTSTEPINUNDOREDO"},
	{wxSTC_MOD_CHANGEMARKER,	"MOD_CHANGEMARKER"},		// on breakpoint edit
	{wxSTC_MOD_BEFOREINSERT,	"MOD_BEFOREINSERT"},
	{wxSTC_MOD_BEFOREDELETE,	"MOD_BEFOREDELETE"},
	{wxSTC_MULTILINEUNDOREDO,	"MULTILINEUNDOREDO"},
	{wxSTC_STARTACTION,		"STARTACTION"},
	{wxSTC_MOD_CHANGEINDICATOR,	"MOD_CHANGEINDICATOR"},		// on search highlights
	{wxSTC_MOD_CHANGELINESTATE,	"MOD_CHANGELINESTATE"},
	{wxSTC_MOD_CHANGEMARGIN,	"MOD_CHANGEMARGIN"},
	{wxSTC_MOD_CHANGEANNOTATION,	"MOD_CHANGEANNOTATION"},	// on Lua errors
	{wxSTC_MOD_CONTAINER,		"MOD_CONTAINER"},
	{wxSTC_MOD_LEXERSTATE,		"MOD_LEXERSTATE"}		// is 0x80000 = 19 bits
};

const int	MAX_MOD_BIT_INDEX = 19;

static const
unordered_set<int>	s_SilentModSet =
{
	wxSTC_MOD_CHANGEINDICATOR,
	wxSTC_MOD_CHANGEANNOTATION,
	wxSTC_MOD_CHANGEMARKER,
	
	wxSTC_PERFORMED_USER
};

const int	ANNOTATION_STYLE = 24;

inline
string	GetModString(const uint32_t mod_flags)
{
	ostringstream	ss;
	
	for (int i = 0; i <= MAX_MOD_BIT_INDEX; i++)
	{
		const uint32_t	flag = (1L << i);
		
		assert(s_ModMap.count(flag));
		
		if (!(mod_flags & flag))		continue;	// flag not set
		
		ss << s_ModMap.at(flag) << " ";
	}
	
	return ss.str();
}

//---- Source Code List Control Class -----------------------------------------

class EditorCtrl: public wxStyledTextCtrl, public IEditorCtrl, private SlotsMixin<CTX_WX>
{
public:
	EditorCtrl(wxWindow *parent_win, int id, ITopNotePanel &topnotebook, wxImageList &img_list);
	virtual ~EditorCtrl();
	
	wxWindow*		GetWxWindow(void) override		{return this;}
	void			BindToPanel(wxWindow *panel, const size_t retry_ms) override
	{
	}
	
	ISourceFileClass*	GetCurrentSFC(void) const override	{return m_CurrentSFC;}
	EditorSignals&		GetSignals(void) override		{return m_Signals;}
	
	void	LoadFromSFC(ISourceFileClass *sfc) override;
	
	void	SaveToDisk(void) override
	{
		if (!m_CurrentSFC)		return;			// should be error?
		
		assert(m_CurrentSFC->IsWritableFile());
		
		const bool	ok = m_CurrentSFC->SaveToDisk(*this);
		assert(ok);
		
		// save point in UNDO buffer
		SetSavePoint();
	}
	
	int	GetTotalLines(void) const override
	{
		return GetLineCount();
	}
	
	iPoint	GetCursorPos(void) const override
	{
		const stcPos	pos(*this);
		
		// uMsg("GetCursorPos(pos = [%d; %d], raw = %d)", pos.Col(), pos.Line(), pos.RawPos());
		
		return iPoint(pos.Col(), pos.Line());
	}
	
	void	SetCursorPos(const iPoint &pt) override
	{
		const int	raw_pos = FindColumn(pt.y(), pt.x());
		
		GotoPos(raw_pos);
		
		EnsureCaretVisible();
		
		SetFocus();
	}
	
	void	ShowLine(const int ln, const bool focus_f) override;
	void	ShowSpan(const int pos1, const int pos2) override;
	
	void	SetFocus(void) override
	{
		wxStyledTextCtrl::SetFocus();
	}
	
	bool	HasLineMarker(const MARGIN_MARKER_T hl_typ, const int ln) override;
	void	ToggleLineMarker(const MARGIN_MARKER_T hl_typ, const int ln, const bool f) override;
	void	RemoveLineMarkers(const MARGIN_MARKER_T hl_typ) override;
	
	void	RemoveAllHighlights(void) override;
	
	bool	HasLockFlag(void) const override
	{
		return GetReadOnly();
	}

	void	SetLockFlag(const bool lock_f) override
	{
		SetReadOnly(lock_f);
		
		SetCaretStyle(lock_f ? wxSTC_CARETSTYLE_INVISIBLE : wxSTC_CARETSTYLE_LINE);
	}

	void		ClearAnnotations(void) override;
	void		SetAnnotation(int ln, const string &annotation_s) override;
	
	vector<int>	GetBreakpointLines(void) override;
	void		SetBreakpointLines(const vector<int> &bp_list) override;
	
	bool		IsEditorDirty(void) const override;
	void		SetEditorDirty(const bool f) override;
	
	int		SearchText(const string &patt, const uint32_t mask, vector<SearchHit> &hit_list) override;
	void		RemoveSearchHighlights(void) override;
	
	void		SetAllTextLines(const vector<string> &text) override;
	vector<string>	GetAllTextLines(void) const override;
	
	string		GetUserSelection(void) override;
	
	vector<int>	GetUserBookmarks(void);
	
private:
	
	void	UnloadCurrentSFC(void);
	
	void	MarkDirty(void) override
	{
		// wxFAIL_MSG("not implemented");
	}
	
	string	GetToolTipWord(const int pos, int &var_pos, int &styl);
	
	// event handlers
	void	OnWindowCreated(wxWindowCreateEvent &e);
	void	OnMouseMove(wxMouseEvent &e);
	void	OnRightDown(wxMouseEvent &e);
	void	OnMarginClick(wxStyledTextEvent &e);
	void	OnSTCUpdateUI(wxStyledTextEvent &e);
	void	OnReadOnlyModifyAttempt(wxStyledTextEvent &e);
	void	OnStyleNeededEvent(wxStyledTextEvent &e);
	void	OnTextModifiedEvent(wxStyledTextEvent &e);
	void	OnCharAdddedEvent(wxStyledTextEvent &e);
	
	// functions
	void	CancelToolTip(void);
	void	UpdateMarginWidth(void);
	void	ConfigureLexer(void);
	void	SetupKeywords(void);
	void	CalcFontDensity(const int &font_size);
	
	void	ReloadStylCache(void);
	void	OnReloadLuaFunctions(void);
	
	ITopNotePanel		&m_TopNotebook;
	wxImageList		&m_ImageList;
	
	ISourceFileClass	*m_CurrentSFC;
	
	bool			m_DirtyFlag;
	bool			m_WinCreatedFlag;
	
	mutable int		m_LastToolTipPos, m_LastWordPos;			// mutable FAILS on gcc 4.9 ???
	
	struct styl_pair
	{
		char		m_char;
		LEXER_T		m_styl;
	};
	
	vector<styl_pair>	m_CharStylBuff;
	string			m_CharBuff;
	
	EditorSignals		m_Signals;
	
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(EditorCtrl, wxStyledTextCtrl)

	EVT_WINDOW_CREATE(					EditorCtrl::OnWindowCreated)
	EVT_STC_UPDATEUI(		-1,			EditorCtrl::OnSTCUpdateUI)			// to catch cursor movements
	EVT_STC_ROMODIFYATTEMPT(	-1,			EditorCtrl::OnReadOnlyModifyAttempt)
	EVT_STC_MODIFIED(		-1,			EditorCtrl::OnTextModifiedEvent)
	EVT_STC_CHARADDED(		-1,			EditorCtrl::OnCharAdddedEvent)
	EVT_STC_MARGINCLICK(		-1,			EditorCtrl::OnMarginClick)
	
	EVT_MOTION(						EditorCtrl::OnMouseMove)
	EVT_RIGHT_DOWN(						EditorCtrl::OnRightDown)
	
	/*
	EVT_SET_FOCUS(						EditorCtrl::OnSetFocus)
	EVT_KILL_FOCUS(						EditorCtrl::OnKillFocus)
	EVT_CHILD_FOCUS(					EditorCtrl::OnChildFocus)
	*/
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

	EditorCtrl::EditorCtrl(wxWindow *parent_win, int id, ITopNotePanel &topnotebook, wxImageList &img_list)
		: wxStyledTextCtrl(parent_win, id, wxDefaultPosition, wxDefaultSize),
		SlotsMixin(CTX_WX, "stc EditorCtrl"),
		m_TopNotebook(topnotebook),
		m_ImageList(img_list),
		m_CurrentSFC(nil)
{
	m_CharStylBuff.clear();
	m_CharBuff.clear();
	
	// SetCanFocus(false);				// makes scintilla THINK it has focus, while in effect the search field does...
	
	m_WinCreatedFlag = false;
	
	// ! would remove default shortcuts
	//   CmdKeyClearAll();
	
	/*
	// ALT-keyboard shortcuts
	CmdKeyAssign('Z', wxSTC_SCMOD_ALT, wxSTC_CMD_UNDO);
	CmdKeyAssign('X', wxSTC_SCMOD_ALT, wxSTC_CMD_CUT);
	CmdKeyAssign('C', wxSTC_SCMOD_ALT, wxSTC_CMD_COPY);
	CmdKeyAssign('V', wxSTC_SCMOD_ALT, wxSTC_CMD_PASTE);
	CmdKeyAssign('A', wxSTC_SCMOD_ALT, wxSTC_CMD_SELECTALL);
	*/
	
	ConfigureLexer();
	
	SetTabWidth(TAB_NUM_CHARS);
	SetIndent(TAB_NUM_CHARS);
	SetTabIndents(true);

	SetEdgeColumn(80);			// in characters now (used to be in pixels?)
	SetEdgeMode(wxSTC_EDGE_LINE);
	SetEdgeColour(COLOR_VERTICAL_EDIT_EDGE);
	
	vector<pair<int, int>>	ranges {{'a', 'z'}, {'A', 'Z'}, {'0', '9'}, {'_', '_'}};
	
	wxString	word_s;
	
	for (auto it : ranges)
	{
		for (int c = it.first; c <= it.second; c++)	word_s.Append((char)c);
	}
	
	// for moving from word to word
	SetWordChars(word_s);
	SetWhitespaceChars(" \t");
	SetPunctuationChars("().-=[]");
	
	SetupKeywords();
	
	// LEXER folding levels (smudges margin)
	// SetProperty("fold", "1");
	// SetProperty("fold.compact", "1");		// dunno what it does
	
	// hex display for debugging but SCREWS UP line# display?
	// SetFoldFlags(64);
	
	// set caret line background color, alpha & show
	SetCaretLineBackground(COLOR_CARRET_SELECTED_LINE);
	SetCaretLineBackAlpha(0x40);
	SetCaretLineVisible(true);				// kindof sucks, blinks with window focus (?)
	SetCaretStyle(wxSTC_CARETSTYLE_LINE);
	// SetYCaretPolicy(wxSTC_CARET_SLOP | wxSTC_CARET_JUMPS, 2);			// # lines excluded from top/bottom
	SetYCaretPolicy(wxSTC_CARET_SLOP | wxSTC_CARET_EVEN, 3);			// # lines excluded from top/bottom
	
	// search result indicators
	IndicatorSetStyle(SEARCH_INDICATOR_ID, wxSTC_INDIC_STRAIGHTBOX);
	IndicatorSetForeground(SEARCH_INDICATOR_ID, COLOR_FILL_SEARCH_HIGHLIGHT);
	IndicatorSetAlpha(SEARCH_INDICATOR_ID, SEARCH_HIGHLIGHT_BRUSH_ALPHA);
	IndicatorSetOutlineAlpha(SEARCH_INDICATOR_ID, SEARCH_HIGHLIGHT_PEN_ALPHA);
	
	// SetModEventMask(wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT | wxSTC_PERFORMED_UNDO | wxSTC_PERFORMED_REDO | wxSTC_PERFORMED_USER);
	SetModEventMask(wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT | wxSTC_LASTSTEPINUNDOREDO);
}

//---- DTOR -------------------------------------------------------------------

	EditorCtrl::~EditorCtrl()
{
	uLog(DTOR, "STCEditorCtrl::DTOR");
}

//---- Unload Current SFC -----------------------------------------------------

void	EditorCtrl::UnloadCurrentSFC(void)
{
	MixZapGroup(STC_FUNCTION_REFRESH_GROUP);
	
	if (m_CurrentSFC)
	{
		/*
		void	*doc_p = GetDocPointer();
		assert(doc_p);
		
		AddRefDocument(doc_p);
		*/
		
		m_CurrentSFC->SaveFromEditor(*this);
		
		EmptyUndoBuffer();
	}
	
	ClearAll();
		
	m_CurrentSFC = nil;
}

//---- Load From SFC ----------------------------------------------------------

void	EditorCtrl::LoadFromSFC(ISourceFileClass *sfc)
{
	if (sfc == m_CurrentSFC)		return;		// same as earlier

	AutoMuteStc	mute(*this, *this);
	
	UnloadCurrentSFC();
	
	m_CurrentSFC = sfc;
	if (!sfc)		return;
	
	/*
	void	*doc_p = sfc->GetDocument();			// nil on 1st time
	
	if (!doc_p)
	{
		doc_p = CreateDocument();
	}
	else	SetDocPointer(doc_p);
	*/

	m_CurrentSFC->RestoreToEditor(*this);
	
	// signal document is "clean"
	// SetSavePoint();
}

//---- Get All Text Lines -----------------------------------------------------

vector<string>	EditorCtrl::GetAllTextLines(void) const
{
	const int	end_pos = GetLastPosition();
	const char	*p = GetCharacterPointer();
	assert(p);
	
	int	n_LF = 0;
	int	n_CR = 0;
	
	for (int i = 0; i < end_pos; i++)
	{
		if (p[i] == '\n')	n_LF++;		// *nix only
		if (p[i] == '\r')	n_CR++;		// Win too
	}
	
	const int	n_lines = GetNumberOfLines();
	
	uLog(STYLED, "LF %d, CR %d, lines %d", n_LF, n_CR, n_lines);
	
	vector<string>	res;
	
	for (int i= 0; i < n_lines; i++)
		res.emplace_back(GetLineText(i).ToStdString());
	
	return res;
}

//---- Set All Text Lines -----------------------------------------------------

void	EditorCtrl::SetAllTextLines(const vector<string> &text_lines)
{
	Clear();
	
	wxString	s;
	
	for (const string &ln : text_lines)
	{
		s << ln << "\n";
	}
	
	SetText(s);
	
	// call lexer & assign fold levels
	Colourise(0, -1);
	
	CallAfter(&EditorCtrl::ReloadStylCache);	// (parent may not be fully initialized)
	
	UpdateMarginWidth();
	
	EmptyUndoBuffer();
}	

//---- On Window Created event ------------------------------------------------

void	EditorCtrl::OnWindowCreated(wxWindowCreateEvent &e)
{
	m_WinCreatedFlag = true;
}

//---- On Read-Only Modify Attempt event --------------------------------------

void	EditorCtrl::OnReadOnlyModifyAttempt(wxStyledTextEvent &e)
{
	uWarn("EditorCtrl::OnReadOnlyModifyAttempt()");
	
	auto	*sfc = GetCurrentSFC();
	assert(sfc);
	
	const bool	edit_f = sfc->OnReadOnlyEditAttempt();
	
	// unlock immediately so edit continues normally
	SetLockFlag(!edit_f);
}

//---- Is Dirty ? -------------------------------------------------------------

bool	EditorCtrl::IsEditorDirty(void) const
{
	return IsModified();
}

//---- Clear Dirty ------------------------------------------------------------

void	EditorCtrl::SetEditorDirty(const bool f)
{
	const bool	changed_f = (f != m_DirtyFlag);
	
	m_DirtyFlag = f;
	
	SetModified(f);
	
	if (changed_f)
		m_Signals.OnEditorDirtyChanged(CTX_WX, GetCurrentSFC(), f);
}

//---- On Text Modified event -------------------------------------------------

void	EditorCtrl::OnTextModifiedEvent(wxStyledTextEvent &e)
{
	// e.Skip();
	
	if (!m_WinCreatedFlag)		return;			// ignore until window created
	
	const string	s = GetModString(e.GetModificationType());
	const bool	dirty_f = IsModified();
	const bool	dirty_changed_f = (dirty_f != m_DirtyFlag);
	
	// uWarn("EditorCtrl::OnTextModifiedEvent(%S, dirty = %c)", s, dirty_f);
	
	// always reload style cache -- AFTER processed event
	CallAfter(&EditorCtrl::ReloadStylCache);
	
	if (dirty_changed_f)
	{	m_DirtyFlag = dirty_f;
		uLog(STYLED, "EditorCtrl is %s", dirty_f ? "dirty" : "clean");
		
		m_Signals.OnEditorDirtyChanged(CTX_WX, GetCurrentSFC(), dirty_f);
	}
	
	UpdateMarginWidth();
		
	// to update breakpoints
	// ! corner case could see unchanged total lines but still shift breakpoints (when pasting multi-line over selection)
	m_Signals.OnEditorContentChanged(CTX_WX);
}

//----- On Margin Breakpoint clicked ------------------------------------------

void	EditorCtrl::OnMarginClick(wxStyledTextEvent &e)
{
	if (e.GetMargin() != MARGIN_BREAKPOINTS)
	{	e.Skip();
		return;
	}
	
	const int	ln = LineFromPosition(e.GetPosition());
	
	// completely IGNORE state from UI
	m_Signals.OnEditorBreakpointClick(CTX_WX, GetCurrentSFC(), ln);			// 0-based line
}

//---- On STC Update UI -------------------------------------------------------

	// capture cursor movements

void	EditorCtrl::OnSTCUpdateUI(wxStyledTextEvent &e)
{
	// (do NOT get position from event, isn't updated)
	e.Skip();
	
	const stcPos	stc_pos(*this);
	
	m_Signals.OnEditorCursorChanged(CTX_WX, stc_pos.Col(), stc_pos.Line());
}

//---- Show Line (w/ highlight) -----------------------------------------------

void	EditorCtrl::ShowLine(const int ln, const bool focus_f)
{
	if (ln < 0)
	{	// no highlight, show source top
		GotoLine(0);
		return;
	}
	
	GotoLine(ln - 1);

	EnsureCaretVisible();
	
	if (focus_f)
	{	
		SetFocus();
	}
}

//---- Get User Selection -----------------------------------------------------

string	EditorCtrl::GetUserSelection(void)
{
	long	pos1 = -1, pos2 = -1;
	
	GetSelection(&pos1, &pos2);
	if (pos1 >= pos2)	return "";	// selected nothing
	
	int	ln1, ln2;
	
	ln1 = LineFromPosition(pos1);
	ln2 = LineFromPosition(pos2);
	if (ln1 != ln2)		return "";	// selected > 1 line
	
	while (isspace(GetCharAt(pos1)) && (pos1 < pos2))	pos1++;			// crappy ?
	while (isspace(GetCharAt(pos2 -1)) && (pos1 < pos2))	pos2--;
	if (pos1 >= pos2)	return "";	// selected nothing
	
	const string	selection_s = GetTextRange(pos1, pos2).ToStdString();
	return selection_s;
}

//---- Search Text ------------------------------------------------------------

int	EditorCtrl::SearchText(const string &patt, const uint32_t ext_mask, vector<SearchHit> &hit_list)
{
	// clear all first...
	RemoveSearchHighlights();
	
	auto	*sfc = GetCurrentSFC();
	if (!sfc)			return 0;
	
	if (patt.size() == 0)		return 0;
	
	const uint32_t	mask = ext_mask & FIND_FLAG::EXT_FIND_MASK;
	(void)mask;								// to implement !
	
	const bool	no_comment_f = (ext_mask & FIND_FLAG::EXT_NO_COMMENT);
	const bool	highlight_f = (ext_mask & FIND_FLAG::EXT_HIGHLIGHT);
	
	const wxString	wx_patt = wxString(patt);
	// const int	end_pos = GetLastPosition();
	
	const size_t	sz_bf = hit_list.size();
	
	const regex	SEARCH_RE(patt, regex::basic);
	
	auto		it = m_CharBuff.cbegin();
	smatch		matches;
	
	while (regex_search(it, m_CharBuff.cend(), matches/*&*/, SEARCH_RE))
	{
		const size_t	n_matches = matches.size();			// full match = single match (with no capture)
		assert(n_matches == 1);
		
		const size_t	org_pos = it - m_CharBuff.cbegin();
		const size_t	offset = matches.position(0);
		const size_t	pos = org_pos + offset;
		const auto	len = matches.length(0);
		
		// get style at position, exclude comment styles
		if ((!no_comment_f) || (s_CommentStyleSet.count(m_CharStylBuff[pos].m_styl) == 0))
		{
			// (could be DELAYED?)
			const int	ln = LineFromPosition(pos) + 1;
			const int	ln_pos = PositionFromLine(ln -1);
				
			hit_list.push_back(SearchHit(pos, len, sfc, ln, ln_pos));
			
			if (highlight_f)
			{	// filled background rectangle (should handle later?)
				IndicatorFillRange(pos, len);
			}
		}
		
		// skip over result
		it += offset + len + 1;
		assert(it < m_CharBuff.cend());
	}
	
	const size_t	n_res = hit_list.size() - sz_bf;
	if (n_res)
	{
		uLog(STOPWATCH, "EditorCtrl::SearchText() %6zu results", n_res);
	}
	
	return n_res;
}

//---- Remove Search Highlights -----------------------------------------------

void	EditorCtrl::RemoveSearchHighlights(void)
{
	SetIndicatorCurrent(SEARCH_INDICATOR_ID);
	
	IndicatorClearRange(0, GetLastPosition());
}

//---- Clear Annotations ------------------------------------------------------

void	EditorCtrl::ClearAnnotations(void)
{
	AnnotationClearAll();
}

//---- Set Annotation ---------------------------------------------------------

void	EditorCtrl::SetAnnotation(int ln, const string &annotation_s)
{
	AnnotationClearAll();
	AnnotationSetText(ln - 1, wxString(annotation_s));
	AnnotationSetStyle(ln - 1, ANNOTATION_STYLE);
	AnnotationSetVisible(true);
}

//---- Show Span --------------------------------------------------------------

void	EditorCtrl::ShowSpan(const int pos1, const int pos2)
{
	// should use RANGE ?
	const int	ln = LineFromPosition(pos1);
	
	ShowLine(ln + 1, false/*focus?*/);		// pre-offset so is identity in ShowLine()
}

//---- Has Line Marker of given type ? ----------------------------------------

bool	EditorCtrl::HasLineMarker(const MARGIN_MARKER_T hl_typ, const int ln)
{
	assert(s_HighlightToMarkerMap.count(hl_typ) > 0);
	if (ln == -1)		return false;			// should be error?
	assert(ln != 0);
	
	const MARKER_INDEX	marker_id = s_HighlightToMarkerMap.at(hl_typ);
	
	const uint32_t	mask = 1ul << (int) marker_id;
	
	return MarkerGet(ln - 1) & mask;
}	

//---- Set/Clear Line Marker of given type ------------------------------------

void	EditorCtrl::ToggleLineMarker(const MARGIN_MARKER_T hl_typ, const int ln, const bool f)
{
	assert(s_HighlightToMarkerMap.count(hl_typ) > 0);
	if (ln == -1)		return;				// should be error?
	assert(ln != 0);
	const MARKER_INDEX	marker_id = s_HighlightToMarkerMap.at(hl_typ);
	
	if (f)	MarkerAdd(ln - 1, (const int)marker_id);
	else	MarkerDelete(ln - 1, (const int)marker_id);
}

//---- Remove all Line Markers of given type ----------------------------------

void	EditorCtrl::RemoveLineMarkers(const MARGIN_MARKER_T hl_typ)
{
	assert(s_HighlightToMarkerMap.count(hl_typ) > 0);
	const MARKER_INDEX	marker_id = s_HighlightToMarkerMap.at(hl_typ);
		
	MarkerDeleteAll((const int)marker_id);
}

//---- Remove All Highlight ---------------------------------------------------

void	EditorCtrl::RemoveAllHighlights(void)
{
	// uLog(UI, "EditorCtrl::RemoveAllHighlights()");
	
	// SetCaretLineVisible(false);
	
	RemoveLineMarkers(MARGIN_MARKER_T::EXECUTION_LINE);
	RemoveLineMarkers(MARGIN_MARKER_T::SEARCH_RESULT_LINE);
	
	RemoveSearchHighlights();
	
	CancelToolTip();
}

//---- Get Breakpoint Lines ---------------------------------------------------

vector<int>	EditorCtrl::GetBreakpointLines(void)
{
	vector<int>	res;
	
	const int	n_lines = GetNumberOfLines();
	const uint32_t	mask = 1L << (int) MARKER_INDEX::BREAKPOINT;
	
	for (int i = 0; i < n_lines; i++)
	{
		if (!(MarkerGet(i) & mask))	continue;		// non-const !
		
		res.push_back(i + 1);
	}
	
	return res;
}

//---- Set Breakpoint Lines ---------------------------------------------------

void	EditorCtrl::SetBreakpointLines(const vector<int> &breakpoint_lines)
{
	uLog(BREAKPOINT_EDIT, "EditorCtrl::SetBreakpointLines(%zu lines)", breakpoint_lines.size());
	
	AutoMuteStc	mute(*this, *this);
	
	// remove all breakpoints
	MarkerDeleteAll((int)MARKER_INDEX::BREAKPOINT);
	
	for (const auto ln : breakpoint_lines)
	{
		MarkerAdd(ln/*zero-based*/, (int) MARKER_INDEX::BREAKPOINT);
	}
}

//---- On Character Add from user ---------------------------------------------

void	EditorCtrl::OnCharAdddedEvent(wxStyledTextEvent &e)
{
	// occurs AFTER char was already added
	
	e.Skip();
	
	const int	c = e.GetKey();
	if (c == '\n')
	{	// auto-indent - DOESN'T WORK?
		const int	ln = GetCurrentLine();
		assert(ln > 0);
		// get previous line's indentation (in chars)
		const int	n_cols = GetLineIndentation(ln - 1);
		// divide by tab size
		const int	n_tabs = n_cols / GetTabWidth();
		// concat as many tab chars
		wxString	indent_s('\t', n_tabs);
		
		// insert at current position
		AddText(indent_s);
	}
}

//---- On Mouse Move event ----------------------------------------------------

void	EditorCtrl::OnMouseMove(wxMouseEvent &e)
{
	e.Skip();
	
	if (!e.ControlDown())							return CancelToolTip();
	if (!m_TopNotebook.CanQueryDaemon())					return;			// daemon not available now
	
	const wxPoint	pt = e.GetPosition();
	
	const int	pos = PositionFromPointClose(pt.x, pt.y);
	if ((pos == wxSTC_INVALID_POSITION) || (pos == m_LastToolTipPos))	return;
	m_LastToolTipPos = pos;
	
	int	var_pos = -1, styl = -1;
	
	const string	key = GetToolTipWord(pos, var_pos/*&*/, styl/*&*/);
	if (key.empty() || (styl == DDT_USER_LUA_FUNCTION))	return CancelToolTip();
	
	wxString	res;
	
	bool	f = m_TopNotebook.SolveVar_ASYNC(key, res/*&*/);
	if (!f)
	{	// not cached yet (invalidate pos cache so will redo)
		m_LastToolTipPos = -1;
		return;
	}
	
	// was cached, check if is already up
	if (CallTipActive() && (m_LastWordPos == var_pos))			return;		// tooltip for this var already shown
	m_LastWordPos = var_pos;
	
	CallTipSetForeground(COLOR_STROKE_TOOLTIP);
	CallTipSetBackground(COLOR_FILL_TOOLTIP);
		
	CallTipShow(pos, res);
}

//---- Cancel Tooltip ---------------------------------------------------------

void	EditorCtrl::CancelToolTip(void)
{
	CallTipCancel();
	
	m_LastToolTipPos = m_LastWordPos = -1;
}

//---- Get ToolTip Word -------------------------------------------------------

string	EditorCtrl::GetToolTipWord(const int pos, int &var_pos, int &styl)
{
	var_pos = -1;
	const int	w_start = WordStartPosition(pos, true/*only word chars*/);
	const int	w_end = WordEndPosition(pos, true/*only word chars*/);
	
	const string	word = GetTextRange(w_start, w_end);
	
	for (int i = w_start; i < w_end; i++)
	{	styl = GetStyleAt(i);
		if ((styl != DDT_USER_VARIABLE) && (styl != DDT_USER_LUA_FUNCTION))	return "";		// wrong style
	}
	
	// check if picked a FIELD
	if (w_start > 0)
	{	const char	prefix = GetCharAt(w_start - 1);
		if ((prefix == '.') || (prefix == '['))
		{	// caught a FIELD, back-track RECURSIVELY (?)
			return GetToolTipWord(w_start - 2, var_pos/*&*/, styl/*&*/);
		}
	}
	
	var_pos = w_start;
	
	return word;
}
	
//---- On Mouse Right-Click Down ----------------------------------------------

void	EditorCtrl::OnRightDown(wxMouseEvent &e)
{
	const wxPoint	pt = e.GetPosition();
	
	const int	pos = PositionFromPointClose(pt.x, pt.y);
	if (pos == wxSTC_INVALID_POSITION)
	{
		uErr("illegal pos on right-click");
		return;
	}
	
	int	var_pos = -1, styl = -1;
	
	const string	var_name = GetToolTipWord(pos, var_pos/*&*/, styl/*&*/);
	
	uMsg("selected %S", var_name);
	
	// build context menu
	wxMenu	menu;
	
	if (!var_name.empty())	menu.Append(CONTEXT_MENU_ID_STC_ADD_TO_WATCHES, "Add to Watches");
	menu.Append(CONTEXT_MENU_ID_STC_COPY_LUA_ERROR, "Copy Lua Error");
	
	// immediate popup menu -- will send FOCUS event
	const int	id = GetPopupMenuSelectionFromUser(menu);
	e.Skip();	// release mouse
	if (id < 0)				return;			// canceled
	
	switch (id)
	{
		case CONTEXT_MENU_ID_STC_ADD_TO_WATCHES:
		
			m_TopNotebook.AddToWatches(var_name);
			break;
			
		case CONTEXT_MENU_ID_STC_COPY_LUA_ERROR:
		
			uWarn("CONTEXT_MENU_ID_STC_COPY_LUA_ERROR");
			break;
			
		default:
		
			break;
	}
}

//---- Update Margin Width (based on max #lines) ------------------------------

void	EditorCtrl::UpdateMarginWidth(void)
{
	const int	n_tot_lines = GetLineCount();
	const int	n_digits = ((n_tot_lines > 0) ? log10(n_tot_lines) : 0) + 1;
	
	const wxString	test_string = wxString('9', n_digits) + "_";
	
	const int	n_pixels = TextWidth(wxSTC_STYLE_LINENUMBER, test_string);
	
	if (n_pixels != GetMarginWidth(MARGIN_LINE_NUMBERS))
		SetMarginWidth(MARGIN_LINE_NUMBERS, n_pixels);
}

//---- Configure Lua Lexer ----------------------------------------------------

void	EditorCtrl::ConfigureLexer(void)
{
	wxFont	font_t = wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "");
	wxFont	fontItalic = wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL, false, "");
	
	SetBufferedDraw(true);
	StyleClearAll();

	SetFont(font_t);
	StyleSetFont(wxSTC_STYLE_DEFAULT, font_t);
	
	for (int i = 0; i < 32; i++)	StyleSetFont(i, font_t);	// assign font to all styles
	
	StyleSetFont(ANNOTATION_STYLE, font_t);

	#define CLR(r,g,b)	wxColor(r, g, b)

	StyleSetForeground(wxSTC_LUA_DEFAULT,  CLR(128,128,128));	// white space
	
	StyleSetForeground(wxSTC_LUA_COMMENT,  CLR(0,100,20));		// block comment
	StyleSetFont(wxSTC_LUA_COMMENT, fontItalic);
	StyleSetForeground(wxSTC_LUA_COMMENTLINE,  CLR(0,100,20));	// line comment
	StyleSetFont(wxSTC_LUA_COMMENTLINE, fontItalic);
	StyleSetForeground(wxSTC_LUA_COMMENTDOC,  CLR(127,127,127));	// doc. comment (?)
	
	StyleSetForeground(wxSTC_LUA_NUMBER,  CLR(0,127,127));		// number
	StyleSetForeground(wxSTC_LUA_WORD,  CLR(0,0,255));		// Lua word
	StyleSetBold(wxSTC_LUA_WORD,  true);
	
	StyleSetForeground(wxSTC_LUA_STRING,  CLR(127,0,127));
	StyleSetForeground(wxSTC_LUA_CHARACTER,  CLR(127,0,127));	// (not used ?)
	
	StyleSetForeground(wxSTC_LUA_LITERALSTRING,  CLR(0,127,127));
	StyleSetForeground(wxSTC_LUA_PREPROCESSOR,  CLR(127,127,0));
	StyleSetForeground(wxSTC_LUA_OPERATOR, CLR(0,0,0));
	
	StyleSetForeground(wxSTC_LUA_IDENTIFIER, CLR(0,0,0));		// (user script functions)
	
	StyleSetForeground(wxSTC_LUA_STRINGEOL, CLR(0,0,0));		// unterminated strings
	StyleSetBackground(wxSTC_LUA_STRINGEOL, CLR(224,192,224));
	StyleSetBold(wxSTC_LUA_STRINGEOL, true);
	StyleSetEOLFilled(wxSTC_LUA_STRINGEOL, true);
	
	// keyword groups
	StyleSetForeground(wxSTC_LUA_WORD2, CLR(0,20,180));		// built-in Lua functions
	StyleSetForeground(wxSTC_LUA_WORD3, CLR(128,64,0));
	StyleSetForeground(wxSTC_LUA_WORD4, CLR(127,0,0));
	StyleSetForeground(wxSTC_LUA_WORD5, CLR(127,0,95));
	
	StyleSetForeground(wxSTC_LUA_WORD6, CLR(10,10,10));		// user Lua functions
	StyleSetUnderline(wxSTC_LUA_WORD6, true);
	
	StyleSetForeground(wxSTC_LUA_WORD7, CLR(0,127,127));
	StyleSetBackground(wxSTC_LUA_WORD7, CLR(240,255,255));
	StyleSetForeground(wxSTC_LUA_WORD8, CLR(255,127,127));
	StyleSetBackground(wxSTC_LUA_WORD8, CLR(224,255,255));
	
	StyleSetForeground(wxSTC_LUA_LABEL, CLR(0,127,127));		// (style #20)
	StyleSetBackground(wxSTC_LUA_LABEL, CLR(192,255,255));
	
	// no idea, just before folders
	StyleSetForeground(21, CLR(0,0,127));
	StyleSetBackground(21, CLR(176,255,255));
	
	StyleSetForeground(22, CLR(0,127,0));
	StyleSetBackground(22, CLR(255,0,0));
	
	// find highlight style
	StyleSetForeground(23, CLR(0,80,0));
	StyleSetBackground(23, CLR(144,255,144));
	
	// Lua error annotation style (id 24)
	StyleSetForeground(ANNOTATION_STYLE, COLOR_LUA_ERROR_ANNOTATION);
	StyleSetBackground(ANNOTATION_STYLE, CLR(255, 255, 255));
	StyleSetItalic(ANNOTATION_STYLE, true);
	
// UI colors

	// default UI color (don't set)
	// StyleSetForeground(wxSTC_STYLE_DEFAULT, CLR(224,192,224));
	// StyleSetBackground(wxSTC_STYLE_DEFAULT, CLR(224,192,224));
	
	// braces
	StyleSetForeground(wxSTC_STYLE_BRACELIGHT, CLR(0,0,255));
	StyleSetBold(wxSTC_STYLE_BRACELIGHT, true);
	StyleSetForeground(wxSTC_STYLE_BRACEBAD, CLR(255,0,0));
	StyleSetBold(wxSTC_STYLE_BRACEBAD, true);
	
	// indentation guides
	StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, CLR(192, 192, 255));
	StyleSetBackground(wxSTC_STYLE_INDENTGUIDE, CLR(255, 255, 255));

	SetUseTabs(true);
	SetIndentationGuides(true);
	SetVisiblePolicy(wxSTC_VISIBLE_SLOP, 3/*num vertical lines*/);		// how far from border can it be
	// SetXCaretPolicy(wxSTC_CARET_SLOP, 10);
	// SetYCaretPolicy(wxSTC_CARET_SLOP, 3);
	
	// line numbering -- margin ZERO is reserved for line number (style #33)
	StyleSetForeground(wxSTC_STYLE_LINENUMBER, wxColour(20, 20, 20));
	// StyleSetBackground(wxSTC_STYLE_LINENUMBER, CLR(220,220,220));
	SetMarginWidth(MARGIN_LINE_NUMBERS, TextWidth(wxSTC_STYLE_LINENUMBER, "999999"));		// TEMPORARY line # margin pixel width (is recomputed in real-time)
	SetMarginMask(MARGIN_LINE_NUMBERS, 0xFFFFFF00);
	// MarkerSetAlpha(0, 10);
	
	// 4 marker symbols = 4 bits = 0xF
	const uint32_t	combo_marker_mask = 0x7E000000ul | (0xFl << 1/*after line numbers*/);
	// #define wxSTC_MASK_FOLDERS 0xFE000000
	
// breakpoint margin
	SetMarginWidth(MARGIN_BREAKPOINTS, 20/*width*/);		// (based on bitmap)
	SetMarginType(MARGIN_BREAKPOINTS, wxSTC_MARGIN_SYMBOL);
	// MarkerDefine((int)MARKER_INDEX::BREAKPOINT, wxSTC_MARK_CIRCLE, COLOR_STROKE_BREAKPOINT, COLOR_FILL_BREAKPOINT);	// built-in looks bumpy
	MarkerDefineBitmap((int)MARKER_INDEX::BREAKPOINT, m_ImageList.GetBitmap(BITMAP_ID_RED_DOT));
	SetMarginMask(MARGIN_BREAKPOINTS, combo_marker_mask);
	SetMarginSensitive(MARGIN_BREAKPOINTS, true);
	
// current execution line highlight
	SetMarginWidth(MARGIN_CURRENT_EXECUTION_LINE, 0/*width*/);
	SetMarginType(MARGIN_CURRENT_EXECUTION_LINE, wxSTC_MARGIN_SYMBOL);
	MarkerDefine((int)MARKER_INDEX::CURRENT_EXECUTION_LINE, wxSTC_MARK_BACKGROUND, wxColour(0, 255, 0), COLOR_CURRENT_EXECUTION_LINE);
	SetMarginSensitive(MARGIN_CURRENT_EXECUTION_LINE, false);
	SetMarginMask(MARGIN_CURRENT_EXECUTION_LINE, 0);

// current search result marker (1 per project)
	SetMarginWidth(MARGIN_SELECTED_SEARCH_RESULT, 0/*width*/);
	SetMarginType(MARGIN_SELECTED_SEARCH_RESULT, wxSTC_MARGIN_SYMBOL);
	MarkerDefine((int)MARKER_INDEX::SELECTED_SEARCH_RESULT, wxSTC_MARK_SHORTARROW, COLOR_STROKE_SEARCH_HIGHLIGHT, COLOR_FILL_SEARCH_HIGHLIGHT);
	SetMarginMask(MARGIN_SELECTED_SEARCH_RESULT, combo_marker_mask);
	SetMarginSensitive(MARGIN_SELECTED_SEARCH_RESULT, false);
	
// user bookmark marker
	SetMarginWidth(MARGIN_USER_BOOKMARK, 0/*width*/);
	SetMarginType(MARGIN_USER_BOOKMARK, wxSTC_MARGIN_SYMBOL);
	MarkerDefine((int)MARKER_INDEX::USER_BOOKMARK, wxSTC_MARK_ROUNDRECT, COLOR_STROKE_USER_BOOKMARK, COLOR_FILL_USER_BOOKMARK);
	SetMarginMask(MARGIN_USER_BOOKMARK, combo_marker_mask);
	SetMarginSensitive(MARGIN_USER_BOOKMARK, false);	
}

//---- Setup Keywords ---------------------------------------------------------

void	EditorCtrl::SetupKeywords(void)
{   
	SetLexer(wxSTC_LEX_LUA);

	// (keywords ripped from scite 1.68)
	SetKeyWords(0,	"and break do else elseif end false for function if in local nil not or repeat return then true until while ");
			
	SetKeyWords(1,	"_VERSION assert collectgarbage dofile error gcinfo loadfile loadstring print rawget rawset require tonumber tostring type unpack ");
			
	SetKeyWords(2,	"_G getfenv getmetatable ipairs loadlib next pairs pcall rawequal setfenv setmetatable xpcall "
			"string table math coroutine io os debug load module select ");
			
	SetKeyWords(3,	"string.byte string.char string.dump string.find string.len "
			"string.lower string.rep string.sub string.upper string.format string.gfind string.gsub "
			"table.concat table.foreach table.foreachi table.getn table.sort table.insert table.remove table.setn "
			"math.abs math.acos math.asin math.atan math.atan2 math.ceil math.cos math.deg math.exp "
			"math.floor math.frexp math.ldexp math.log math.log10 math.max math.min math.mod "
			"math.pi math.pow math.rad math.random math.randomseed math.sin math.sqrt math.tan "
			"string.gmatch string.match string.reverse table.maxn "
			"math.cosh math.fmod math.modf math.sinh math.tanh math.huge");
			
	SetKeyWords(4,	"coroutine.create coroutine.resume coroutine.status coroutine.wrap coroutine.yield "
			"io.close io.flush io.input io.lines io.open io.output io.read io.tmpfile io.type io.write "
			"io.stdin io.stdout io.stderr "
			"os.clock os.date os.difftime os.execute os.exit os.getenv os.remove os.rename "
			"os.setlocale os.time os.tmpname "
			"coroutine.running package.cpath package.loaded package.loadlib package.path "
			"package.preload package.seeall io.popen "
			"debug.debug debug.getfenv debug.gethook debug.getinfo debug.getlocal "
			"debug.getmetatable debug.getregistry debug.getupvalue debug.setfenv "
			"debug.sethook debug.setlocal debug.setmetatable debug.setupvalue debug.traceback ");
}

//---- Reload Char/Style Cache ------------------------------------------------

void	EditorCtrl::ReloadStylCache(void)
{
	const int	end_pos = GetLastPosition();
	assert(end_pos);
	
	const size_t	sz = end_pos + 1;
	
	uLog(STYLED, "EditorCtrl::ReloadStylCache(%zu chars)", sz);
	
	// allocates only if needed
	m_CharStylBuff.resize(sz, {0, LUA_LEX_ILLEGAL});
	
	// implemented in stc.cpp:376
	// const size_t	received_bytes = wxStyledTextCtrl::OnGetStyledText2(&m_CharStylBuff[0].m_char, sz * sizeof(styl_pair), 0/*startPos*/, end_pos);
	const wxMemoryBuffer	buff = GetStyledText(0/*startPos*/, end_pos);			// TOO SLOW ???
	
	const size_t	received_bytes = buff.GetDataLen();
	assert((received_bytes & 1ul) == 0);
	const size_t	received_elms = received_bytes / sizeof(styl_pair);
	assert(received_elms < sz);
	
	const void	*p = buff.GetData();
	assert(p);
	memcpy(&m_CharStylBuff[0].m_char, p, received_bytes);
	
// de-swizzle
	
	m_CharBuff.resize(received_elms, 0);
	
	for (size_t i = 0; i < received_elms; i++)
		m_CharBuff[i] = m_CharStylBuff[i].m_char;
	
	// terminator is IMPORTANT
	m_CharBuff.push_back(0);
	
	MixDelay(STC_FUNCTION_REFRESH_GROUP, STC_FUNCTION_REFRESH_DELAY_MS, this, &EditorCtrl::OnReloadLuaFunctions);
}

//---- On Reload Lua Functions ------------------------------------------------

void	EditorCtrl::OnReloadLuaFunctions(void)
{
	const string	shortname = GetCurrentSFC()->GetShortName();
	
	// copy to comment-free buffer
	string		char_buff;
	vector<size_t>	rawPosLUT;
	
	const size_t	len = m_CharStylBuff.size();
	
	char_buff.reserve(len);			// too conservative!
	rawPosLUT.reserve(len);
	
	for (size_t i = 0; i < len; i++)
	{
		const LEXER_T	styl = 	m_CharStylBuff[i].m_styl;
		if (s_CommentStyleSet.count(styl))		continue;
		
		char_buff.push_back(m_CharStylBuff[i].m_char);
		rawPosLUT.push_back(i);
	}
	
	#if 0
		for (int ln = 0; ln <= LineFromPosition(GetLastPosition()); ln++)
		{
			const int	raw_lvl = GetFoldLevel(ln);
			const bool	header_f = (raw_lvl & wxSTC_FOLDLEVELHEADERFLAG);
			
			const int	lvl = (raw_lvl & wxSTC_FOLDLEVELNUMBERMASK) - wxSTC_FOLDLEVELBASE;
			if ((lvl == 0) && (!header_f))		continue;
			
			const int	parent_ln = GetFoldParent(ln);	// & wxSTC_FOLDLEVELNUMBERMASK;
			
			uLog(STC_FUNCTION, " [%3d] level %2d, parent ln %3d, header %c", ln, lvl, parent_ln, header_f);
		}
	#endif
	
	StopWatch		sw;
	
	vector<Bookmark>	bmks;
	
	auto		it = char_buff.cbegin();
	smatch		matches;
	
	while (regex_search(it, char_buff.cend(), matches/*&*/, LUA_FUNCTION_RE))
	{
		const int	n_matches = matches.size();
		assert(n_matches == 3);
		// const string	full_match_s = matches[0];
		assert(matches[1].matched ^ matches[2].matched);			// matches either/or (not both or none)
		
		const size_t	match_ind = matches[1].matched ? 1 : 2;
		
		const string	fname_s = matches[match_ind];
		assert(!fname_s.empty());
		
		const auto	fname_offset = matches.position(match_ind);
		
		const size_t	org_pos = it - char_buff.cbegin();
		const size_t	pos = org_pos + fname_offset;
		assert(pos < rawPosLUT.size());
		const size_t	raw_pos = rawPosLUT[pos];
		
		// string	it_s(it, char_buff.cend());
		it = char_buff.cbegin() + pos + fname_s.length() + 1;			// (pre-advance)
		assert(it < char_buff.cend());
		// it_s.assign(it, char_buff.cend());
		
		const LEXER_T	styl = m_CharStylBuff[raw_pos].m_styl;
		assert(s_CommentStyleSet.count(styl) == 0);					// should no longer have comments (were stripped)
		if (s_DormantStyleSet.count(styl))				continue;	// inside string, skip
		if (s_IgnoreFunctionsSet.count(fname_s))			continue;	// reserved name, skip
		
		const int	ln = LineFromPosition(raw_pos) + 1;
		uLog(STC_FUNCTION, "  %s:%d", fname_s, ln);
		bmks.emplace_back(fname_s, ln);
	}
	
	uLog(STOPWATCH, "  regexed %zu chars, collected %zu functions, took %s", char_buff.size(), bmks.size(), sw.elap_str());
	
	stringstream	ss;
	
	for (const auto &elm : bmks)
	{
		ss << elm.m_Name << " ";
	}
	
	// should be GLOBAL to project!!! (could take long time?)
	SetKeyWords(5, wxString(ss.str()));
	
	m_Signals.OnEditorFunctionsChanged(CTX_WX, shortname, bmks);
}

//---- Get User Bookmarks -----------------------------------------------------

vector<int>	EditorCtrl::GetUserBookmarks(void)
{
	const int	n_lines = GetNumberOfLines();
	const uint32_t	mask = 1L << (int) MARKER_INDEX::USER_BOOKMARK;
	
	vector<int>	lines;
	
	for (int i = 0; i < n_lines; i++)
	{
		if (!(MarkerGet(i) & mask))	continue;		// non-const !
		
		lines.push_back(i + 1);
	}
	
	return lines;
}

//---- INSTANTIATE ------------------------------------------------------------

// static
IEditorCtrl*	IEditorCtrl::CreateStyled(wxWindow *parent_win, int id, ITopNotePanel &topnotebook, wxImageList &img_list)
{
	return new EditorCtrl(parent_win, id, topnotebook, img_list);
}
	
// nada mas
