
#include "ServerTCP.h"

#include "lx/stream/MemDataInputStream.h"
#include "lx/stream/MemoryOutputStream.h"

#include "ddt/FileSystem.h"

#include "DaemonJIT.h"
#include "ServerTCP.h"

#include "Daemon.h"

// using namespace std;
// using namespace LX;

namespace DDT
{

using std::string;
using std::vector;
using std::unordered_set;

//---- Daemon IMP -------------------------------------------------------------

class Daemon: public IDaemon, public Breather, public LX::LogSlot
{
public:
	// ctors
	Daemon(const int server_port, const int sleep_ms);
	// dtor
	~Daemon();
	
	bool	WaitForConnection(void) override;
	bool	SessionSetupLoop(LuaStartup &startup) override;
	void	Reset(void) override;
	void	InitDDT(lua_State *L, IJitGlue *jglue, const bool break_asap_f) override;
	void	ExitDDT(lua_State *L) override;
	
	VM_ERR	loadfile(lua_State *L, const string &lua_filename) override;
	void	pcall(lua_State *L, int nargs, int nresults, const string &debug_s) override;
	
	bool	IsConnected(void) const override;
	bool	SessionEnded(const int cod) override;
	
	void	EnableNetLogLevels(const unordered_set<LogLevel> &levels) override;
	
// pseudo-private	
	
	bool	ReadMessage(vector<uint8_t> &buff);
	bool	SendMessage(const MemDataOutputStream &&mos);
	
	// conditional compilation if has no MOS as head
	template<typename _T, typename ... Args>
	typename std::enable_if<!std::is_same<_T, MemDataOutputStream>(),bool>::type
		SendMessage(_T &&val, Args&& ... args)
	{
		return SendMessage(MemDataOutputStream{}, std::forward<_T>(val), std::forward<Args>(args)...);
	}
	template<typename _T, typename ... Args>
	bool	SendMessage(MemDataOutputStream &&mos, _T &&val, Args&& ... args)
	{
		mos << val;
		
		return SendMessage(std::move(mos), std::forward<Args>(args) ...);
	}
	
	bool	SendSessionEndMessage(const uint32_t &res);
	
	bool	Breathe(void) override;
	
	static IServerTCP	*s_ServerInst;
	
private:
	
	void	LogAtLevel(const timestamp_t stamp, const LogLevel level, const string &msg) override;
	
	void	SyncNStartFailed(const string &err_msg);
	bool	LogOutgoingMessage(const MemDataOutputStream &mos) const;
	
	unique_ptr<IServerTCP>	m_ServerTCPPtr;
	IServerTCP		&m_ServerTCP;
	
	LuaGlue			m_LuaGlue;
	const int		m_AsyncSleepMS;
	
	vector<NetSourceFile>	m_NetFileList;
	unordered_set<LogLevel>	m_NetLogLevelSet;
};

} // namespace DDT

// nada mas