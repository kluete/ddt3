// Lua command text control

#include "wx/textctrl.h"
#include "wx/tglbtn.h"

#include "TopFrame.h"
#include "Controller.h"
#include "BottomNotebook.h"
#include "UIConst.h"

#include "ddt/sharedDefs.h"

#include "lx/xstring.h"

#include "lx/misc/autolock.h"

#include "logImp.h"

#include "LuaTextCtrl.h"

using namespace std;
using namespace LX;
using namespace DDT_CLIENT;

const wxColour	LUA_LOG_COLOR_GREEN(0, 0xa0, 0);

BEGIN_EVENT_TABLE(LuaTextCtrl, wxPanel)

	EVT_TEXT(		EDITCTRL_ID_LUA_COMMAND,		LuaTextCtrl::OnTextChanged)
	EVT_BUTTON(		BUTTON_ID_SEND_LUA_COMMAND,		LuaTextCtrl::OnSendButtonEvent)
	EVT_TOGGLEBUTTON(	BUTTON_ID_HOLD_LUA_COMMAND,		LuaTextCtrl::OnHoldButtonEvent)
	
END_EVENT_TABLE()

//---- CTOR -------------------------------------------------------------------

	LuaTextCtrl::LuaTextCtrl(IBottomNotePanel &parent, int id)
		: wxPanel(parent.GetWxWindow(), id),
		PaneMixIn(parent, id, this)
{
	m_LogLock = false;
	
	// (uses COW)
	wxFont	ft(wxFontInfo(9).Family(wxFONTFAMILY_MODERN).Encoding(wxFONTENCODING_DEFAULT));
	SetFont(ft);
	
	m_NormalAttr = wxTextAttr(*wxBLACK, *wxWHITE, ft);
	m_GreenAttr = wxTextAttr(LUA_LOG_COLOR_GREEN, *wxWHITE, ft);
	m_ErrorAttr = wxTextAttr(*wxRED, *wxWHITE, ft);
	
	m_EditCtrl = new wxTextCtrl(this, EDITCTRL_ID_LUA_COMMAND, "", wxDefaultPosition, wxDefaultSize, wxTE_RICH | wxTE_NOHIDESEL | wxTE_PROCESS_ENTER);
	m_SendButton = new wxButton(this, BUTTON_ID_SEND_LUA_COMMAND, "Send", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	m_HoldButton = new wxToggleButton(this, BUTTON_ID_HOLD_LUA_COMMAND, "Hold", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	
	wxFont	button_ft(wxFontInfo(10).Family(wxFONTFAMILY_SWISS).Encoding(wxFONTENCODING_DEFAULT));
	m_SendButton->SetFont(button_ft);
	m_SendButton->SetCanFocus(false);
	m_HoldButton->SetFont(button_ft);
	m_HoldButton->SetCanFocus(false);
	
	wxBoxSizer	*h_sizer = new wxBoxSizer(wxHORIZONTAL);
	
	h_sizer->Add(m_EditCtrl, wxSizerFlags(1).Border(wxALL, 2).Expand());
	h_sizer->Add(m_SendButton, wxSizerFlags(0).Border(wxALL, 2));
	h_sizer->Add(m_HoldButton, wxSizerFlags(0).Border(wxALL, 2));
	
	wxBoxSizer	*v_sizer = new wxBoxSizer(wxVERTICAL);
	
	m_OutTextCtrl = new wxTextCtrl(this, TEXTCTRL_ID_LUA_OUTPUT, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH | wxTE_NOHIDESEL);
	
	v_sizer->Add(m_OutTextCtrl, wxSizerFlags(1).Border(wxALL, 2).Expand());
	v_sizer->Add(h_sizer, wxSizerFlags(0).Border(wxALL, 2).Expand());
	
	SetSizer(v_sizer);
	
	m_OutTextCtrl->SetDefaultStyle(m_NormalAttr);
}

//---- DTOR -------------------------------------------------------------------

	LuaTextCtrl::~LuaTextCtrl()
{	
	uLog(DTOR, "LuaTextCtrl::DTOR");
	
	m_OutTextCtrl = nil;
}

//---- Log Message ------------------------------------------------------------

void	LuaTextCtrl::LogMessage(const timestamp_t stamp, const string &msg)
{
	return;									// is dead slow !!!
	
	if (!m_OutTextCtrl)			return;		// ignore
	if (!wxThread::IsMain())		return;		// not main thread
	if (m_LogLock)				return;		// prevent re-entry -- BULLSHIT
	AutoLock	lock(m_LogLock/*&*/);
	
	const string	s = xsprintf("%s > %s\n", stamp.str(), msg);
	
	m_OutTextCtrl->SetDefaultStyle(m_NormalAttr);
	
	m_OutTextCtrl->AppendText(wxString(s));
}

//---- Log Lua Error ----------------------------------------------------------

void	LuaTextCtrl::LogLuaError(const string &err_msg)
{
	assert(m_OutTextCtrl);
	
	const string	ln = xsprintf("%s\n", err_msg);
	
	m_OutTextCtrl->SetDefaultStyle(m_ErrorAttr);
	
	m_OutTextCtrl->AppendText(wxString(ln));
}

//---- Clear Log --------------------------------------------------------------

void	LuaTextCtrl::ClearLog(void)
{
	assert(m_OutTextCtrl);
	
	m_OutTextCtrl->Clear();
}

//---- On Text Changed event --------------------------------------------------

void	LuaTextCtrl::OnTextChanged(wxCommandEvent &e)
{
	assert(m_EditCtrl);
	
	const wxString	cmd_s = m_EditCtrl->GetValue();
	
	uMsg("LuaTextCtrl::OnTextChanged(sz %zu)", cmd_s.size());

	e.Skip();
}

//---- On Send Button pressed -------------------------------------------------

void	LuaTextCtrl::OnSendButtonEvent(wxCommandEvent &e)
{
	uMsg("LuaTextCtrl::OnSendButtonEvent()");
	
	e.Skip();
}

//---- On Hold Button toggled -------------------------------------------------

void	LuaTextCtrl::OnHoldButtonEvent(wxCommandEvent &e)
{
	uMsg("LuaTextCtrl::OnHoldButtonEvent()");
	
	e.Skip();
}

// nada mas	
