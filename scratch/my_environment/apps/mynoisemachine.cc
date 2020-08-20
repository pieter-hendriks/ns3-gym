#include "mynoisemachine.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/net-device.h"
#include "ns3/ipv4.h"

#include <cstdlib>

using namespace ns3;

void MyNoiseMachine::StartApplication()
{
	event =	ns3::Simulator::ScheduleNow(&MyNoiseMachine::Send, this);
}
void MyNoiseMachine::Send()
{
	const auto pkt_size = 1500u;
	auto pkt = ns3::Create<ns3::Packet>(pkt_size);
	this->GetNode()->GetDevice(0)->Send(pkt, Ipv4Address("192.168.1.100"), 0);
	Simulator::Schedule(ns3::Time::FromInteger(1 + std::rand() % 150, ns3::Time::Unit::MS), &MyNoiseMachine::Send, this);
}
void MyNoiseMachine::StopApplication()
{
	ns3::Simulator::Cancel(event);
}
