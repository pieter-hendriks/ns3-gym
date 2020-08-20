#include "mynoisemachine.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/net-device.h"
#include "ns3/ipv4.h"
#include "ns3/mac48-address.h"
#include "ns3/wifi-net-device.h"
#include <cstdlib>
#include <stdexcept>

using namespace ns3;

void MyNoiseMachine::StartApplication()
{
	event =	ns3::Simulator::ScheduleNow(&MyNoiseMachine::Send, this);
}
void MyNoiseMachine::Send()
{
	const auto pkt_size = 1500u;
	auto pkt = ns3::Create<ns3::Packet>(pkt_size);
	ns3::Ptr<ns3::NetDevice> dev = nullptr;
	for (unsigned i = 0; i < this->GetNode()->GetNDevices(); ++i)
	{
		dev = this->GetNode()->GetDevice(i);
		if (dev->GetTypeId() == WifiNetDevice::GetTypeId())
		{
			break;
		}
	}
	if (dev->GetTypeId() != WifiNetDevice::GetTypeId())
	{
		throw std::runtime_error("This should be impossible.");
	}
	dev->Send(pkt, Mac48Address::GetBroadcast(), 0);
	Simulator::Schedule(ns3::Time::FromInteger(1 + std::rand() % 150, ns3::Time::Unit::MS), &MyNoiseMachine::Send, this);
}
void MyNoiseMachine::StopApplication()
{
	ns3::Simulator::Cancel(event);
}