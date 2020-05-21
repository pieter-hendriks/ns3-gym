#pragma once
#ifndef INC_MY_PACKET_H_
#define INC_MY_PACKET_H_
#include "ns3/packet.h"
#include "ns3/simulator.h"
class Flow;
class MyPacket : public ns3::Packet
{
public:
	MyPacket() = delete; 
	MyPacket(const MyPacket& p) : ns3::Packet(p), creationTime(p.creationTime), myFlow(p.myFlow) {};
	MyPacket(uint32_t size, const Flow& f) : ns3::Packet(size), creationTime(ns3::Simulator::Now().GetMilliSeconds()), myFlow(f) {};
	virtual ~MyPacket() = default;

	bool isExpired();

private:
	uint64_t TTL;
	uint64_t creationTime;

	const Flow& myFlow; 
};
#endif