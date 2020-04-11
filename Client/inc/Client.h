// lua ddt client network bridge

#pragma once

#include <cstdint>
#include <vector>
#include <memory>

#include "ClientTCP.h"

#ifndef nil
	#define nil	nullptr
#endif

namespace LX
{
	class MemDataOutputStream;
	
} // namespace LX

namespace DDT
{
	class RequestStartupLua;
	
} // namespace DDT

namespace DDT_CLIENT
{
using namespace DDT;
using std::vector;
using std::unique_ptr;

// forward declarations
class IClientTCP;
class Controller;
class TopFrame;
class RemoteServerConfig;

//---- Client network "bridge" ------------------------------------------------

class Client: public Breather
{
public:
	// ctor
	Client(Controller &controller, TopFrame &tf, const int sync_sleep_ms, const int async_sleep_ms);
	// dtor
	virtual ~Client();
	
	bool	ServerHandshake(const RemoteServerConfig &remote);
	bool	IsConnected(void) const;
	bool	CloseConnection(void);
	double	NetStressTest(const size_t n_rounds, const size_t sz, const double rnd_variance, const size_t frame_sz);		// returns throughput
	
	void	CheckUDPQueue(void);

	bool		StartReadMessage_Async(void);
	bool		HasPendingAsync(void) const;
	READ_ASYNC_RES	ReceivedAsyncMessage(vector<uint8_t> &buff);
	
	bool	SendMessage(const LX::MemoryOutputStream &mos);
	DAEMON_MSG	GetDaemonMessageType(const std::vector<uint8_t> &buff) const;
	bool	RequestReply(const LX::MemoryOutputStream &mos);
	
	bool	SendSyncAndStartLua(const RequestStartupLua &req_startup);
	bool	SendBreakpointList(const bool &enable_f = true);
	
	void	ClearCounters(void);
	size_t	GetReadCounter(void) const;
	size_t	GetWriteCounter(void) const;
	
	// IMP
	virtual bool	Breathe(void);			// (BS ?)
	
private:

	bool	ReadMessage(std::vector<uint8_t> &buff);
	bool	CheckConnection(void) const;
	bool	LogOutgoingMessageType(const LX::MemoryOutputStream &mos, const string &prefix = "") const;		// CACA
	void	GetSerializedSources(LX::MemoryOutputStream &mos);
	
	Controller		&m_Controller;
	TopFrame		&m_TopFrame;
	unique_ptr<IClientTCP>	m_ClientTCP_Ptr;
	IClientTCP		&m_ClientTCP;
	int			m_SyncSleepMS;
};

} // namespace DDT_CLIENT

// nada mas