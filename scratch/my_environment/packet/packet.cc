#include "packet.h"
#include "ns3/simulator.h"
using ns3::Simulator;
bool MyPacket::isExpired()
{
	auto currentTime = Simulator::Now().GetMilliSeconds();
	return (currentTime - this->creationTime > this->TTL);
}