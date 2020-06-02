#pragma once
#ifndef INC_SEND_APP_H_
#define INC_SEND_APP_H_
//#include "sendnode.h"
//#include "receivenode.h"

#include "../simulation/mysocket.h"
#include "../simulation/helpers.h"

#include "ns3/application.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/ipv4-address.h"
#include "ns3/inet-socket-address.h"

class SendApplication : public ns3::Application
{
public:
	SendApplication() { throw std::runtime_error("Default SendApplication called"); /* Shouldn't happen - only used in resize call when active > action */ };
	SendApplication(uint32_t dur, uint32_t sz, double tp, double thr, MySocket&& sndr, MySocket&& dst) : duration(dur), size(sz), bps(tp), threshold(thr), sender(std::move(sndr)), destination(std::move(dst)) { };
	SendApplication(SendApplication&& o) : size(o.size), bps(o.bps), sender(std::move(o.sender)), destination(std::move(o.destination)), running(o.running), nextEvent(std::move(o.nextEvent)) { };
	virtual ~SendApplication()
	{
		this->StopApplication();
	};

	SendApplication& operator=(SendApplication&& o) {
		duration = o.duration;
		size = o.size;
		bps = o.bps;
		threshold = o.threshold;
		sender = std::move(o.sender);
		destination = std::move(o.destination);
		running = o.running;
		nextEvent = std::move(o.nextEvent);
		return *this;
	};

	SendApplication& operator=(const SendApplication& o) {
		duration = o.duration;
		size = o.size;
		bps = o.bps;
		threshold = o.threshold;
		sender = o.sender;
		destination = o.destination;
		running = o.running;
		nextEvent = o.nextEvent;
		return *this;
	};


	void send()
	{
		ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>(size);
		ns3::Address addr;
		destination.GetSockName(addr);
		sender.send(packet, 0, ns3::InetSocketAddress::ConvertFrom(addr));
		// Time spent for a single packet = bits / bits per second = second
		// Multiply by 10^6 to get microseconds.

		// Size is in bytes, so size * 8
		auto time = static_cast<unsigned>(((size * 8) / bps) * 1000000 + 0.5);
		nextEvent = ns3::Simulator::Schedule(ns3::MicroSeconds(time), &SendApplication::send, this);
	}


	virtual void StartApplication()
	{
		running = true;
		this->send();
	};
	virtual void StopApplication()
	{
		running = false;
		ns3::Simulator::Cancel(nextEvent);
	};

	
	bool isRunning() const { return running; };
	bool isComplete() const
	{ 
		// Duration should be expressed in Simulator timestamp unit, US in our case.
		if (ns3::Simulator::Now().GetMicroSeconds() - this->getFirstRecvPacketTime() - (this->getFirstSentPacketTime() - this->getFirstRecvPacketTime()) > this->duration)
		{ // Take time first receive -> now, plus an extra send->receive interval for jitter. If that's higher than duration, we assume all packets that should arrive have arrived.
			return true;
		}
		return false;
	};

	bool isSuccessful() const
	{
		if (!isComplete())
			throw std::runtime_error("Successful check on incomplete flow");
		
		//Let's say half can be lost: 
		// If recvCount is bigger than sent * 0.5 ( = 1-0.5), we'll return true.
		// else false, so no reward. Makes sense.
		
		// Can add grades to this at a later point
		// #TODO
		if (this->getRecvCount() > this->getSentCount() * (1-this->threshold))
			return true;
		return false;

	}

	double getThreshold() const { return threshold; };
	uint64_t getSentCount() const { return sender.getSentCount(); };
	uint64_t getRecvCount() const { return destination.getRecvCount(); };

	uint64_t getRecvSize() const { return destination.getRecvSize(); };
	uint64_t getSentSize() const { return sender.getSentSize(); };

	uint64_t getFirstSentPacketTime() const { return sender.startTime(); };
	uint64_t getFirstRecvPacketTime() const { return destination.startTime(); };

private:
	uint32_t duration;
	uint32_t size;
	double bps;
	double threshold;
  MySocket sender;
	MySocket destination;
	bool running;
	ns3::EventId nextEvent;

};
#endif