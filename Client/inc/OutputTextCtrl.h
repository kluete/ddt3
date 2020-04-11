// lua ddt outputTextCtrl

#pragma once

#include <string>
#include <vector>
#include <memory>

#include "lx/ulog.h"
#include "lx/color.h"
#include "lx/misc/geometry.h"
#include "lx/juce/jfont.h"

#include "BottomNotebook.h"

// forward declarations
class wxBitmap;

namespace DDT_CLIENT
{
class TopFrame;

using std::string;
using std::shared_ptr;

using LX::timestamp_t;
using LX::LogLevel;
using LX::Color;
using LX::RGB_COLOR;

struct log_def
{
	log_def(const string &label, const RGB_COLOR &clr)
		: m_Label(label), m_Color(clr)
	{}
	
	log_def(const log_def&) = default;
	log_def& operator=(const log_def&) = default;
	
	const string	m_Label;
	const RGB_COLOR	m_Color;
	// could also store hash but is just as well recomputed
};

//---- Log Output interface ---------------------------------------------------

class ILogOutput : public IPaneComponent
{
public:
	virtual ~ILogOutput() = default;
	
	virtual void	DaemonLog(const timestamp_t org_stamp, const LogLevel level, const string &msg) = 0;
	
	static
	ILogOutput*	Create(shared_ptr<IFixedFont> ff);
};

//---- Solo Log Frame ---------------------------------------------------------

class ISoloLogFrame
{
public:
	virtual ~ISoloLogFrame() = default;
	
	virtual wxWindow*	GetWxWindow(void) = 0;
	virtual void		SetSize(const LX::Rect &r) = 0;
	virtual void		Close(const bool force_f) = 0;
	virtual	LX::Rect	GetScreenRect(void) = 0;
	virtual void		ToggleShowHide(const bool show_f) = 0;
	
	static log_def		GetLevelDef(const LogLevel &lvl);
	
	static
	ISoloLogFrame*	Create(TopFrame *tf, const int id, const wxBitmap &bm);
};

} // namespace DDT_CLIENT

// nada mas