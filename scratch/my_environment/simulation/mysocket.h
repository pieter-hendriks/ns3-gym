#pragma once
#ifndef INC_MY_SOCKET_H_
#define INC_MY_SOCKET_H_
#include "ns3/udp-socket-impl.h"
#include "ns3/udp-socket.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/callback.h"
#include "ns3/simulator.h"
#include "ns3/inet-socket-address.h"
#include <map>
#include <memory>
#include <cassert>
#include <cstring> //memcpy
#include <limits>
#include <functional>
struct MyTimer
{
	MyTimer() : creationTime(ns3::Simulator::Now().GetMicroSeconds()) {};
	virtual ~MyTimer() = default;
	uint64_t creationTime;
};

struct Data : public MyTimer
{
	uint64_t size;
	uint64_t count;
};

class MySocket
{
public:
	virtual char getType() const = 0;
	MySocket() : data(nullptr), pimpl(nullptr) { throw std::runtime_error("Shouldn't happen. Some construction error."); };
	MySocket(ns3::Ptr<ns3::Socket> implptr) : data(nullptr), pimpl(implptr) {
		
	};

	MySocket(const MySocket& o) {
		this->pimpl = o.pimpl;
		if (o.data != nullptr)
		{
			this->data = std::make_unique<Data>();
			this->data->count = o.data->count;
			this->data->size = o.data->size;
			this->data->creationTime = o.data->creationTime;
		}
		else
		{
			this->data = nullptr;
		}
	};

	MySocket(MySocket&& o) : data(std::move(o.data)), pimpl(std::move(o.pimpl)) {	};

	MySocket& operator=(MySocket&& o) {
		pimpl = std::move(o.pimpl);
		data = std::move(o.data);
		return *this;
	};

	MySocket& operator=(const MySocket& o) {
		this->pimpl = o.pimpl;
		if (o.data != nullptr)
		{
			this->data = std::make_unique<Data>();
			this->data->count = o.data->count;
			this->data->size = o.data->size;
			this->data->creationTime = o.data->creationTime;
		}
		else
		{
			this->data = nullptr;
		}
		
		return *this;
	};

	virtual ~MySocket() = default;

	ns3::Ptr<ns3::Socket> get() {
		return pimpl;
	};

	#define my_assert(b) if (!(b)) throw std::runtime_error("Assertion failed");

	// If both are not nullptr, something's wrong.
	// If either is, we're active.
	bool isActive() const { return this->data != nullptr; };
	virtual uint64_t getCount() const {my_assert(this->data != nullptr); return this->data->count; };
	virtual uint64_t getSize() const { my_assert(this->data != nullptr);  return data->size; };

	uint64_t startTime() const
	{
		my_assert(this->data != nullptr);
		return this->data->creationTime;
	}
	const Data* get() const {
		return this->data.get();
	}
	#undef my_assert
protected:
	std::unique_ptr<Data> data;
	std::unique_ptr<Data> dataSent;
	ns3::Ptr<ns3::Socket> pimpl;

};

class MySendSocket : public MySocket
{
public:
	MySendSocket() = default;
	MySendSocket(ns3::Ptr<ns3::Socket> implptr) : MySocket(implptr) {	};
	virtual ~MySendSocket() = default;
	virtual char getType() const { return 's'; };
	void send(ns3::Ptr<ns3::Packet> packet, unsigned flags, ns3::InetSocketAddress addr)
	{
		// Making it here ensures correct timer for the hold period check
		if (data == nullptr)
			data = std::make_unique<Data>();
		data->size += packet->GetSize();
		data->count += 1;
		pimpl->SendTo(packet, flags, addr);
	};

};

class MyRecvSocket : public MySocket
{
public:
  MyRecvSocket() = default;
	virtual char getType() const { return 'r'; };
	virtual ~MyRecvSocket() = default;
	MyRecvSocket(ns3::Ptr<ns3::Socket> implptr) : MySocket(implptr) {
		pimpl->SetRecvCallback(ns3::MakeCallback(&MyRecvSocket::readSocket, this)); 
	};
	void receive(ns3::Ptr<ns3::Packet> packet)
	{
		// Making it here ensures correct timer for the hold period check
		if (data == nullptr)
			data = std::make_unique<Data>();
		data->size += packet->GetSize();
		data->count += 1;
	};
	void readSocket(ns3::Ptr<ns3::Socket> sock)
	{
		this->receive(sock->Recv(std::numeric_limits<unsigned>::max(), 0));
	}
	virtual uint64_t getCount() const { if (this->data == nullptr) return 0; return this->data->count; };
	virtual uint64_t getSize() const { if (this->data == nullptr) return 0; return this->data->size; };
};


#endif