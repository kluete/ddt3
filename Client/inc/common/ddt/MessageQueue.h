// Lua DDT message queue

#pragma once

#include <cstdint>

#include <memory>
#include <list>
#include <mutex>
#include <vector>

namespace LX
{
class MemDataOutputStream;		// bullshit ?

using std::vector;
using std::list;
using std::mutex;

class Message
{
public:
	// ctors
	Message();
	Message(const MemDataOutputStream &mos);
	Message(const vector<uint8_t> &buff);
	
	const uint8_t*	GetPtr(void) const
	{
		return &m_Buff[0];
	}
	
	size_t	GetSize(void) const
	{
		return m_Buff.size();
	}
	
	const vector<uint8_t>&	GetBuff(void) const
	{
		return m_Buff;
	}
	
private:
	
	vector<uint8_t>		m_Buff;
};

//---- Message Queue ----------------------------------------------------------

class MessageQueue
{
public:
	MessageQueue();
	~MessageQueue();
	
	// void	Put(Message && m);		// MOVE operation
	void	Put(const Message &m);
	int	NumPending(void);
	bool	HasPending(void);
	
	bool	Get(Message& m);		// returns [ok]
	
private:

	list<Message>	m_Queue;
	mutex		m_Mutex;
};

} // namespace LX

// nada mas
