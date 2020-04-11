// Lua text control

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "wx/wx.h"
#include "wx/panel.h"
#include "wx/tglbtn.h"

#include "ddt/sharedDefs.h"

#include "BottomNotebook.h"

namespace DDT_CLIENT
{
// forward references
class TopFrame;
class Controller;
class BottomNotePanel;

//---- Memory View Control ----------------------------------------------------

class LuaTextCtrl: public wxPanel, public PaneMixIn, private LX::SlotsMixin<LX::CTX_WX>
{
public:
	LuaTextCtrl(IBottomNotePanel &parent, int id);
	virtual ~LuaTextCtrl();
	
	void	LogMessage(const LX::timestamp_t stamp, const std::string &msg);
	void	LogLuaError(const std::string &err_msg);
	void	ClearLog(void);
	
private:
	
	void	OnTextChanged(wxCommandEvent &e);
	void	OnSendButtonEvent(wxCommandEvent &e);
	void	OnHoldButtonEvent(wxCommandEvent &e);
	
	wxTextCtrl		*m_OutTextCtrl;
	wxTextCtrl		*m_EditCtrl;
	wxButton		*m_SendButton;
	wxToggleButton		*m_HoldButton;
	
	wxTextAttr		m_NormalAttr, m_GreenAttr, m_ErrorAttr;
	
	bool			m_LogLock;
	
	DECLARE_EVENT_TABLE()
};

} // namespace DDT_CLIENT

// nada mas
