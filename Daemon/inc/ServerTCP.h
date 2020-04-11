// lua ddt daemon SERVER

#pragma once

#include <cstdlib>
#include <iostream>

#include <vector>
#include <thread>
#include <future>
#include <stdexcept>

#include "lx/xutils.h"

#ifndef nil
	#define nil	nullptr
#endif

using LogLevel = uint32_t;

// forward references
namespace LX
{
	class MemDataOutputStream;
	class timestamp_t;
	
	enum class TCP_OPTION : uint8_t
	{
		RECEIVE_BUFFER_SIZE = 0,
		SEND_BUFFER_SIZE,
		NO_DELAY,
		REUSE_ADDRESS
	};
} // namespace LX

namespace DDT
{

class Breather
{
public:
	virtual bool	Breathe(void) = 0;
};

using std::string;
using std::vector;

using LX::timestamp_t;

//---- Server TCP -------------------------------------------------------------

class IServerTCP
{
	friend class ServerTCP;
public:
	
	// dtor
	virtual ~IServerTCP() = default;
	
	virtual string	GetHostname(void) = 0;
	virtual bool	IsConnected(void) const = 0;
	virtual bool	WaitForConnection(void) = 0;
	virtual bool	Disconnect(void) = 0;
	virtual bool	SetOption(const LX::TCP_OPTION typ, const int32_t val32) = 0;
	
	virtual bool	ReadMessage(vector<uint8_t> &buff) = 0;
	virtual bool	WriteMessage(const LX::MemDataOutputStream &mos) = 0;
	
	virtual	void	QueueUDPLog(const timestamp_t stamp, const LogLevel level, const string &msg) = 0;
	
	static
	IServerTCP*	Create(const int port, Breather &breather);
};

} // namespace DDT

// nada mas
