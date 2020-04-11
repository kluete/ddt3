// ddt drag & drop target TEMPLATE

#ifndef DDT_DROP_TARGET_H_DEFINED
#define DDT_DROP_TARGET_H_DEFINED

#include "wx/dnd.h"

namespace DDT_CLIENT
{

template<typename _Listener>
class DropTarget: public wxFileDropTarget
{
public:
	DropTarget(_Listener *recipient)
	{
		assert(recipient);
		m_Recipient = recipient;
	}
	
	// (continuous)
	virtual wxDragResult	OnDragOver(wxCoord x, wxCoord y, wxDragResult res)
	{	assert(m_Recipient);
		const wxPoint	pt(x, y);
		
		if (pt == m_LastPt)		return res;		// no pos change
		m_LastPt = pt;
		
		bool	ok = m_Recipient->OnDragOver(pt);
		return ok ? res : wxDragCancel;
	}
	
	// (final)
	virtual bool	OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &filenames)
	{	assert(m_Recipient);
		
		return m_Recipient->OnDropFiles(wxPoint(x, y), filenames);
	}
	
	static
	wxString	ResultString(const wxDragResult &res)
	{	switch (res)
		{	case wxDragError:	return "wxDragError"; break;
			case wxDragNone:	return "wxDragNone"; break;
			case wxDragCopy:	return "wxDragCopy"; break;
			case wxDragMove:	return "wxDragMove"; break;
			case wxDragLink:	return "wxDragLink"; break;
			case wxDragCancel:	return "wxDragCancel"; break;
			default:		return "<wxDragUNKNOWN>"; break;
		}
	}
	
private:
	
	_Listener	*m_Recipient;
	wxPoint		m_LastPt;
};

} // namespace DDT_CLIENT


#endif // DDT_DROP_TARGET_H_DEFINED

// nada mas
