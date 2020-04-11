
// Lua DDT DLL/shared library stub

#ifdef LUA_DDT_DDT

//---- Application Implementation (used only in non-wx app) -------------------

class AppImp: public wxApp
{
public:
	// CTOR
	AppImp()
		: wxApp()
	{
	}
	// DTOR
	~AppImp()
	{
		// when should this be called?
		
	}

// IMPLEMENTATIONS
	
	bool	OnInit()
	{
		// don't call base class so won't fiddle with command line
		// ...but shouldn't be called anyway since is called from within CTOR
		// meaning the virtual table isn't built anyway
		
		// SetExitOnFrameDelete(true);
		
		return true;
	}
/*	
	// don't need a SECOND event loop?
	int	MainLoop()
	{
		
		return 0;
	}
	

	
	int	OnExit(void)
	{
		// DON'T call base class - will be called every time exits debugger GUI and returns to client app
		// int	res = wxApp::OnExit();
		
		// error code is currently ignored
		return 0;
	}
*/
	
private:

	DECLARE_ABSTRACT_CLASS(AppImp)

	
} *s_AppInst = nil;

IMPLEMENT_ABSTRACT_CLASS(AppImp, wxApp);

IMPLEMENT_APP_NO_MAIN(AppImp)			// implements a wxApp anyway?

	// check if wx was initialized
	if (!wxTheApp == nil)
	{	int	argc = 0;
		wxChar	**argv = nil;
		
		if (!wxEntryStart(argc, argv))
		{	std::cerr << "error calling wxEntryStart(), aborted\n";
			return nil;
		}
		
		if (!wxTheApp || !wxTheApp->CallOnInit())
		{
			std::cerr << "error calling wxTheApp::CallOnInit(), aborted\n";
			return nil;
		}
		
		// s_AppInst = (AppImp*) wxTheApp;
	}
	else	s_AppInst = nil;
	


#endif // LUA_DDT_DDT

// nada mas
