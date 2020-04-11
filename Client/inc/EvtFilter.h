// lua ddt wxEvent filter

#ifndef LUA_DDT_EVENT_FILTER_H_INCLUDED
#define	LUA_DDT_EVENT_FILTER_H_INCLUDED

#include <cstdint>
#include <string>
#include <unordered_set>
#include <unordered_map>

#include "wx/event.h"
#include "wx/eventfilter.h"

namespace DDT_CLIENT
{

enum class DRAG_MODE : int
{
	NONE = 0,
	START,
	DRAGGING
};

//---- My Event Filter --------------------------------------------------------

class MyEventFilter: public wxEventFilter
{
public:
	// ctor
	MyEventFilter();
	// dtor
	virtual ~MyEventFilter();

	void	AddSelf(wxEvtHandler *hdl);
	void	RemoveSelf(void);
	void	Toggle(const bool &f);
	
	// implementation
	virtual int	FilterEvent(wxEvent &e);
	
	std::unordered_set<wxEventType>	m_WantSet, m_IgnoreSet;
	std::unordered_set<wxEventType>	m_MouseSet, m_FocusSet, m_KeySet;
	
	std::unordered_map<wxEventType, std::string>	m_TypeToName;
	
private:

	wxEvtHandler	*m_OrgHandler;
	bool		m_EnabledFlag;
	bool		m_RecurseLock;
	
	wxPoint		m_InitPt;
	DRAG_MODE	m_DragMode;
};

} // namespace DDT_CLIENT


#endif // LUA_DDT_EVENT_FILTER_H_INCLUDED

// nada mas
