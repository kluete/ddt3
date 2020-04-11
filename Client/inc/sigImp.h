// signal implementations

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>

#include "lx/event/xsig_slot.h"

#include "SourceFileClass.h"
#include "ddt/MessageQueue.h"

namespace DDT_CLIENT
{
// import into namespace
using LX::Signal;

using std::string;
using std::vector;
using std::unordered_set;
enum PANE_ID : int;

//---- Logic Signals ----------------------------------------------------------

class LogicSignals
{
public:
	Signal<>				OnRefreshProject;
	Signal<ISourceFileClass*, vector<int>>	OnRefreshSfcBreakpoints;		// which data/UI direction?
	Signal<string, int>			OnShowLocation;
	Signal<string, string, string, bool>	OnShowMemory;
};

//---- VarView UI signals -----------------------------------------------------

class VarViewSignals
{
public:	
	Signal<uint32_t>	OnVarCheckboxes;
	Signal<bool>		OnGridOverlays;
	Signal<uint32_t>	OnVarColumns;
	Signal<LX::Message>	OnCollectedMessage;
};

//---- Notepage Signals -------------------------------------------------------

class NotepageSignals
{
public:
	Signal<>		OnPanelClearVariables;
	Signal<>		OnPanelRequestReload;
};

//---- code Editor Signals ----------------------------------------------------

class EditorSignals
{
public:
	Signal<ISourceFileClass*, bool>		OnEditorDirtyChanged;
	Signal<>				OnEditorContentChanged;
	Signal<string, vector<Bookmark>>	OnEditorFunctionsChanged;
	Signal<ISourceFileClass*, int>		OnEditorBreakpointClick;
	Signal<int, int>			OnEditorCursorChanged;
};

} // namespace DDT_CLIENT

// nada mas
