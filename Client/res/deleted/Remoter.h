// remoter navigator frame ("remoter" stems from "Berzerker")

#ifndef LUA_DDT_REMOTER_H_DEFINED
#define LUA_DDT_REMOTER_H_DEFINED

#include "wx/wx.h"
#include "DirTreeCtrl.h"

#ifndef nil
	#define	nil	nullptr
#endif

// forward declarations
class wxChoice;
class TopFrame;
class Controller;
class LocalDirCtrl;
class ClientTCP_Abstract;
class NavigationBar;

//---- Remoter Frame --------------------------------------------------------------

class RemoterFrame: public wxFrame, public DirTreeListener
{
public:
	// ctor
	RemoterFrame(TopFrame *tf, int id, Controller *controller, ClientTCP_Abstract *client_tcp);
	// dtor
	virtual ~RemoterFrame();
	
	// DirTreeListener IMPLEMENTATIONS
	virtual bool	NotifyDragOriginator(DirTreeCtrl *originator, const StringList &path_list);
	virtual	bool	NotifyDragLoop(DirTreeCtrl *recipient, const std::string &highlight_path, const wxPoint &pt);
	virtual bool	NotifyDragRecipient(DirTreeCtrl *recipient, const std::string &dest_path, const StringList &opt_path_list);
	
private:
	
	void	Redraw(void);
	
	bool	ShouldPreventAppExit() const override;
	
	// events
	void	OnCloseEvent(wxCloseEvent &e);
	void	OnRefreshBroadcast(wxThreadEvent &e);
	void	OnStartBrowsing(wxThreadEvent &e);
	void	OnMoveEvent(wxMoveEvent &e);
	void	OnResizeEvent(wxSizeEvent &e);
	void	OnLocalFilterChoice(wxCommandEvent &e);
	void	OnRemoteFilterChoice(wxCommandEvent &e);
	
	TopFrame		*m_TopFrame;
	Controller		*m_Controller;
	ClientTCP_Abstract	*m_ClientTCP;
	
	wxPanel		*m_LeftPanel, *m_RightPanel;
	wxBoxSizer	*m_TopHSizer;
	NavigationBar	*m_LeftNavigationBar, *m_RightNavigationBar;
	LocalDirCtrl	*m_LocalDirCtrl;
	DirTreeCtrl	*m_RemoteDirCtrl;
	wxChoice	*m_LeftFilterChoice, *m_RightFilterChoice;
	
	DECLARE_EVENT_TABLE()
};

#endif // LUA_DDT_REMOTER_H_DEFINED

// nada mas