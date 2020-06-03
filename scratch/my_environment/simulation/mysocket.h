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

void socketReceive(ns3::Ptr<ns3::Socket> sockPtr);

class MySocket
{
public:
	MySocket() :dataReceived(nullptr), dataSent(nullptr), pimpl(nullptr) { throw std::runtime_error("Shouldn't happen. Some construction error."); };
	MySocket(ns3::Ptr<ns3::Socket> implptr) : dataReceived(nullptr), dataSent(nullptr), pimpl(implptr) {
		pimpl->SetRecvCallback(ns3::MakeCallback([this](auto sock) -> void {
			this->receive(sock->Recv(std::numeric_limits<unsigned>::max(), 0));
		})); //   ns3::Callback< void, ns3::Ptr< ns3::Socket > > (&socketReceive));
		// callback originally &socketReceive, changed for lambda to allow capturing this. Makes for cleaner implementation
	};

	MySocket(const MySocket& o) {
		this->pimpl = o.pimpl;
		this->dataReceived = std::make_unique<Data>();
		this->dataReceived->count = o.dataReceived->count;
		this->dataReceived->size = o.dataReceived->size;
		this->dataReceived->creationTime = o.dataReceived->creationTime;
		this->dataSent = std::make_unique<Data>();
		this->dataSent->count = o.dataSent->count;
		this->dataSent->size = o.dataSent->size;
		this->dataSent->creationTime = o.dataSent->creationTime;
	};

	MySocket(MySocket&& o) : dataReceived(std::move(o.dataReceived)), dataSent(std::move(o.dataSent)), pimpl(std::move(o.pimpl)) {	};

	MySocket& operator=(MySocket&& o) {
		pimpl = std::move(o.pimpl);
		dataReceived = std::move(o.dataReceived);
		dataSent = std::move(o.dataSent);
		return *this;
	};

	MySocket& operator=(const MySocket& o) {
		this->pimpl = o.pimpl;
		this->dataReceived = std::make_unique<Data>();
		this->dataReceived->count = o.dataReceived->count;
		this->dataReceived->size = o.dataReceived->size;
		this->dataReceived->creationTime = o.dataReceived->creationTime;
		this->dataSent = std::make_unique<Data>();
		this->dataSent->count = o.dataSent->count;
		this->dataSent->size = o.dataSent->size;
		this->dataSent->creationTime = o.dataSent->creationTime;
		return *this;
	};

	virtual ~MySocket() = default;

	void send(ns3::Ptr<ns3::Packet> packet, unsigned flags, ns3::InetSocketAddress addr)
	{
		if (dataSent == nullptr)
			dataSent = std::make_unique<Data>();
		dataSent->size += packet->GetSize();
		dataSent->count += 1;
		pimpl->SendTo(packet, flags, addr);
	};
	void receive(ns3::Ptr<ns3::Packet> packet)
	{
		// Making it here ensures correct timer for the hold period check
		if (dataReceived == nullptr)
			dataReceived = std::make_unique<Data>();
		dataReceived->size += packet->GetSize();
		dataReceived->count += 1;
	};

	ns3::Ptr<ns3::Socket> get() {
		return pimpl;
	};

	#define my_assert(b) if (!(b)) throw std::runtime_error("Assertion failed");

	uint64_t getSentCount() const { my_assert(this->dataSent != nullptr);  return dataSent->count; };
	uint64_t getRecvCount() const { my_assert(this->dataReceived != nullptr);  return dataReceived->count; };

	uint64_t getSentSize() const { my_assert(this->dataSent != nullptr);  return dataSent->size; };
	uint64_t getRecvSize() const { my_assert(this->dataReceived != nullptr);  return dataReceived->size; };

	uint64_t startTime() const
	{
		// This would be so much easier if I just split this into sendsocket and recvsocket.
		// #TODO
		my_assert(this->dataSent != nullptr || this->dataReceived != nullptr);

		if (dataSent != nullptr)
		{
			my_assert(this->dataReceived == nullptr);
			return dataSent->creationTime;
		}
		return dataReceived->creationTime;
	}
	const Data* getDest() const {
		return dataReceived.get();
	}
	const Data* getSender() const {
		return dataSent.get();
	}
	#undef my_assert
private:
	std::unique_ptr<Data> dataReceived;
	std::unique_ptr<Data> dataSent;
	ns3::Ptr<ns3::Socket> pimpl;

};


#endif