#include "mynoisemachine.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/net-device.h"
#include "ns3/ipv4.h"
#include "ns3/mac48-address.h"
#include "ns3/wifi-net-device.h"
#include "ns3/ipv4-l3-protocol.h"
#include <cstdlib>
#include <stdexcept>

using namespace ns3;
MyNoiseMachine::MyNoiseMachine(ns3::Ptr<ns3::NetDevice> noiseDevice) : dev(noiseDevice) {};
void MyNoiseMachine::StartApplication()
{
	event =	ns3::Simulator::ScheduleNow(&MyNoiseMachine::Send, this);
}
void MyNoiseMachine::Send()
{
	//std::cout << "Sending noise!" << std::endl;
	const auto pkt_size = 1375;
	auto pkt = ns3::Create<ns3::Packet>(pkt_size);
	dev->Send(pkt, Mac48Address::GetBroadcast(), 0);
	Simulator::Schedule(ns3::Time::FromInteger(1 + std::rand() % 10, ns3::Time::Unit::MS), &MyNoiseMachine::Send, this);
}
void MyNoiseMachine::StopApplication()
{
	ns3::Simulator::Cancel(event);
}
