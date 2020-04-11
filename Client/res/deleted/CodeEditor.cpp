// ddt juce code editor

#pragma GCC diagnostic ignored "-Wunused-private-field"

#include <cassert>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <thread>
#include <cmath>

#include "JuceHeader.h"

#include "lx/misc/geometry.h"
#include "lx/juce/jeometry.h"
#include "lx/juce/jwxBinder.h"
#include "lx/juce/JuceBridge.h"
#include "lx/juce/jfont.h"
#include "lx/juce/jOrgSizer.h"

#include "sigImp.h"
#include "logImp.h"

#include "TopFrame.h"
#include "TopNotebook.h"
#include "SourceFileClass.h"

#include "StyledSourceCtrl.h"

using namespace std;
using namespace juce;
using namespace LX;
using namespace DDT_CLIENT;

/*	
	LUA_COMMENT		= 0x006414FFul,
	LUA_OPERATOR		= 0x000000FFul,
	LUA_STRING		= 0x7F007FFFul,
	LUA_FUNCTION_BUILTIN	= 0x0014B4FFul,
	LUA_NUMBER		= 0x007F7FFFul,
*/

/*
	enum TokenType
	
	tokenType_error = 0,
	tokenType_comment,
	tokenType_keyword,
	tokenType_operator,
	tokenType_identifier,
	tokenType_integer,
	tokenType_float,
	tokenType_string,
	tokenType_bracket,
	tokenType_punctuation
*/

#define TOKEN_CLR(nm, clr_enum)	{nm, Color(clr_enum)}
static const
unordered_map<string, Color> s_TokenColorMap
{
	TOKEN_CLR("Error",		RGB_COLOR::RED),
	TOKEN_CLR("Comment",		RGB_COLOR::LUA_COMMENT),
	TOKEN_CLR("Keyword",		RGB_COLOR::LUA_FUNCTION_BUILTIN),
	TOKEN_CLR("Operator",		RGB_COLOR::LUA_OPERATOR),
	TOKEN_CLR("Identifier",		RGB_COLOR::BLACK),		// require, user identifier too?
	TOKEN_CLR("Integer",		RGB_COLOR::LUA_NUMBER),
	TOKEN_CLR("Float",		RGB_COLOR::LUA_NUMBER),
	TOKEN_CLR("String",		RGB_COLOR::LUA_STRING),
	TOKEN_CLR("Bracket",		RGB_COLOR::BLACK),
	TOKEN_CLR("Punctuation",	RGB_COLOR::BLACK),
};
#undef TOKEN_CLR

//---- My Tokenizer -----------------------------------------------------------

class MyTokenizer : public LuaTokeniser		// CodeTokeniser
{
public:
	MyTokenizer()
		: m_ColorScheme(LuaTokeniser::getDefaultColourScheme())
	{
		for (const auto &it : s_TokenColorMap)
		{	
			const string	name = it.first;
			const Color	clr = it.second;
			
			m_ColorScheme.set(name, clr);
		}
	}
	
	CodeEditorComponent::ColourScheme	getDefaultColourScheme(void) override
	{
		return m_ColorScheme;
	}
	
private:
	
	CodeEditorComponent::ColourScheme	m_ColorScheme;
};

//---- juce Code Editor -------------------------------------------------------

class CodeEditor : public Component, public IEditorCtrl, private SlotsMixin<CTX_JUCE>
{
public:
	CodeEditor(ITopNotePanel &top_notebook)
		: m_TopNotebook(top_notebook),
		m_CurrentSFC(nil),
		m_Editor(m_Document, &m_Tokenizer)
	{
		m_Editor.setTabSize(8, false/*spaces?*/);
		m_Editor.setFont(Font("DejaVu Sans Mono", "Book", 13));
		
		addAndMakeVisible(&m_Editor);
		
		auto	top_sizer = IContainerSizer::CreateTopSizer(*this, AXIS::VERTICAL);
		
		top_sizer->Add(&m_Editor).Weight(1).Border(2);
		
		top_sizer->LayoutNow();
	}
	
	wxWindow*		GetWxWindow(void) override		{return nil;}
	EditorSignals&		GetSignals(void) override		{return m_Signals;}
	ISourceFileClass*	GetCurrentSFC(void) const override	{return m_CurrentSFC;}
	
	void	LoadFromSFC(ISourceFileClass *sfc) override;
	
	void	SaveToDisk(void) override
	{
		
	}
	
	iPoint	GetCursorPos(void) const override
	{
		const CodeDocument::Position	pos = m_Editor.getCaretPos();
		
		return iPoint(pos.getIndexInLine(), pos.getLineNumber());
	}
	
	void	SetCursorPos(const iPoint &pt) override
	{
		CodeDocument::Position	pos(m_Document, pt.y(), pt.x());
		
		m_Editor.moveCaretTo(pos, false/*selecting?*/);
	}
	
	int	GetTotalLines(void) const override
	{
		return m_Document.getNumLines();
	}
	
	void	ShowLine(const int ln, const bool focus_f) override
	{
		CodeDocument::Position	pos(m_Document, ln, 0/*col*/);
		m_Editor.moveCaretTo(pos, false/*selecting?*/);
		
		m_Editor.scrollToKeepCaretOnScreen();
		
		// const int	first_screen_ln = ln;
		// m_Editor.scrollToLine(first_screen_ln);
	}
	
	void	ShowSpan(const int pos1, const int pos2) override
	{
		
	}
	
	void	SetFocus(void) override
	{
		
	}
	
	bool	HasLineMarker(const MARGIN_MARKER_T hl_typ, const int ln) override
	{
		return false;
	}
	
	void	ToggleLineMarker(const MARGIN_MARKER_T hl_typ, const int ln, const bool f) override
	{
		
	}
	
	void	RemoveLineMarkers(const MARGIN_MARKER_T hl_typ) override
	{
		
	}
	
	void	RemoveAllHighlights(void) override
	{
	}
	
	void	RemoveSearchHighlights(void) override
	{
	}
	
	bool	HasLockFlag(void) const override
	{
		return m_Editor.isReadOnly();
	}
	
	void	SetLockFlag(const bool f) override
	{
		m_Editor.setReadOnly(f);
	}
	
	void	ClearAnnotations(void) override
	{
	}
	
	void	SetAnnotation(int ln, const string &annotation_s) override
	{
		
	}
	
	void	SetBreakpointLines(const vector<int> &bp_list) override
	{
		
	}
	
	bool	IsEditorDirty(void) const override
	{
		return false;
	}
	
	void	SetEditorDirty(const bool f) override
	{
		
	}
	
	int	SearchText(const string &patt, const uint32_t mask, vector<SearchHit> &hit_list) override
	{
		return 0;
	}
	
	void		SetAllTextLines(const vector<string> &text) override;
	vector<string>	GetAllTextLines(void) const override;
	
	string		GetUserSelection(void) override
	{
		return "";
	}
	
	void	BindToPanel(wxWindow *panel, const size_t retry_ms) override
	{
		if (m_Binder)	return;		// already bound
		
		Rebind(panel, retry_ms);
	}
	
private:
	
	void	Rebind(wxWindow *win, const size_t retry_ms)
	{
		assert(win);
		
		if (!IjwxWindowBinder::IsBindable(*win))
		{	
			// not yet bindable, retry later
			MixDelay("", retry_ms, this, &CodeEditor::Rebind, win, retry_ms);
			return;
		}
		
		m_Binder.reset(IjwxWindowBinder::Create(*win, *this, "Editor"));
		
		uLog(REALIZED, "CodeEditor::Rebind() ok");
		
		// MixConnect(this, &CodeEditor::OnEditorFunctionsChanged, m_TopNotebook.GetSignals().OnEditorFunctionsChanged);
	}
	
	void	UnloadCurrentSFC(void);
	
	void	paint(Graphics &g) override
	{
		const iRect	r = getLocalBounds();
		
		g.fillAll(Color(RGB_COLOR::GTK_GREY));
		
		// border
		g.setColour(Color(RGB_COLOR::GTK_GREY).Darker(80));
		g.drawRect(r.Inset(1), 1);
	}
	
	ITopNotePanel				&m_TopNotebook;
	ISourceFileClass			*m_CurrentSFC;
	CodeDocument				m_Document;
	MyTokenizer				m_Tokenizer;
	CodeEditorComponent			m_Editor;
	
	unique_ptr<IjwxWindowBinder>	m_Binder;
	EditorSignals			m_Signals;
};

//---- Unload Current SFC -----------------------------------------------------

void	CodeEditor::UnloadCurrentSFC(void)
{
	// MixZapGroup(STC_FUNCTION_REFRESH_GROUP);
	
	if (m_CurrentSFC)
	{
		/*
		void	*doc_p = GetDocPointer();
		assert(doc_p);
		
		AddRefDocument(doc_p);
		*/
		
		m_CurrentSFC->SaveFromEditor(*this);
		
		// EmptyUndoBuffer();
	}
	
	// ClearAll();
		
	m_CurrentSFC = nil;
}

//---- Load From SFC ----------------------------------------------------------

void	CodeEditor::LoadFromSFC(ISourceFileClass *sfc)
{
	if (sfc == m_CurrentSFC)		return;		// same as earlier

	// AutoMuteStc	mute(*this, *this);
	
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

vector<string>	CodeEditor::GetAllTextLines(void) const
{
	vector<string>	res;
	
	const int	n_lines = m_Document.getNumLines();
	
	for (int i= 0; i < n_lines; i++)
		res.emplace_back(m_Document.getLine(i).toStdString());
	
	return res;
}

//---- Set All Text Lines -----------------------------------------------------

void	CodeEditor::SetAllTextLines(const vector<string> &text_lines)
{
	// Clear();
	
	ostringstream	ss;
	
	for (const string &ln : text_lines)
	{
		ss << ln << "\n";
	}
	
	m_Editor.loadContent(ss.str());
	
	// call lexer & assign fold levels
	// Colourise(0, -1);
	
	// CallAfter(&EditorCtrl::ReloadStylCache);	// (parent may not be fully initialized)
	
	// UpdateMarginWidth();
	
	// EmptyUndoBuffer();
}

//---- INSTANTIATE ------------------------------------------------------------

// static
IEditorCtrl*	IEditorCtrl::CreateJuceEditor(ITopNotePanel &top_notebook)
{
	return new CodeEditor(top_notebook);
}

// nada mas
