// ddt TCP client

#pragma once

#include <cstdint>
#include <vector>
#include <string>

#ifndef nil
	#define nil	nullptr
#endif

namespace LX
{
	class MemoryOutputStream;

	enum class TCP_OPTION : uint8_t
	{
		RECEIVE_BUFFER_SIZE = 0,
		SEND_BUFFER_SIZE,
		NO_DELAY,
		// CAN_ABORT_CONNECTION,
		// SOCKET_LINGER
	};
	
} // namespace LX

namespace DDT_CLIENT
{

enum class READ_ASYNC_RES : uint8_t
{
	NOT_YET = 0,
	DONE,
	FAILED
};

class Breather
{
public:
	virtual ~Breather() = default;
	
	virtual bool	Breathe(void) = 0;
};

using std::string;
using std::vector;

//---- Client TCP (ABSTRACT) --------------------------------------------------

class IClientTCP
{
	friend class ClientTCP;
public:
	
	// dtor
	virtual ~IClientTCP() = default;
	
	virtual bool	IsConnected(void) const = 0;
	virtual bool	Connect(const string &host, const int port) = 0;
	virtual void	Disconnect(void) = 0;
	virtual bool	SetOption(const LX::TCP_OPTION typ, const int32_t val32) = 0;
	
	virtual void	ServiceUDPLogs(vector<vector<uint8_t>> &buff) = 0;
	
	virtual bool	StartReadMessage_Async(void) = 0;
	virtual	bool	HasPendingAsync(void) const = 0;
	virtual	READ_ASYNC_RES	ReceivedAsyncMessage(vector<uint8_t> &buff) = 0;
	
	virtual bool	ReadMessage(vector<uint8_t> &buff) = 0;
	virtual bool	WriteMessage(const LX::MemoryOutputStream &mos) = 0;
	
	virtual	void	ClearCounters(void) = 0;
	virtual	size_t	GetReadCounter(void) const = 0;
	virtual	size_t	GetWriteCounter(void) const = 0;
	
	static
	IClientTCP*	Create(Breather &breather, const int async_sleep_ms);
};

} // namespace DDT_CLIENT

// nada mas
