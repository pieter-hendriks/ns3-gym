#include "myreceiver.h"
#include "../simulation/simulationenvironment.h"
#include "apps.h"
#include "flowtag.h"

#include <ostream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

#include "ns3/stats-module.h"
#include "ns3/log.h"
#include "ns3/log-macros-enabled.h"

NS_LOG_COMPONENT_DEFINE("MyReceiver");

MyReceiver::MyReceiver(ns3::Ptr<SimulationEnvironment> ptr, ns3::Ptr<Node> node)
: active(false), env(std::move(ptr))
{
	this->m_node = node;
	this->m_port = 5000;
	//ns3::Ptr<UdpSocketFactory> factory = CreateObject<ns3::UdpSocketFactoryImpl>();
	//this->AggregateObject(factory);
}
MyReceiver::~MyReceiver() { if (active) this->StopApplication(); };
void MyReceiver::Receive (Ptr<Socket> socket)
{
	// NS_LOG_FUNCTION (this << socket << packet << from);
	if (!active)
	{
		active = true;
		this->StartApplication();
	}
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from))) 
	{
		if (InetSocketAddress::IsMatchingType (from)) 
		{
			NS_LOG_ERROR("Received " << packet->GetSize () << " bytes from " <<
										InetSocketAddress::ConvertFrom (from).GetIpv4 ());
			// NS_LOG_INFO ("Received " << packet->GetSize () << " bytes from " <<
			//              InetSocketAddress::ConvertFrom (from).GetIpv4 ());
		}
		FlowTag flowtag;
		TimestampTag timestamp;
		// Should never not be found since the sender is adding it, but
		// you never know.
		if (packet->FindFirstMatchingByteTag (timestamp)) 
		{
			Time tx = timestamp.GetTimestamp ();

			if (m_delay != 0) 
			{
				m_delay->Update (Simulator::Now () - tx);
			}
		}
		if (packet->FindFirstMatchingByteTag (flowtag))
		{
			env->AddReceivedPacket(flowtag.getId(), flowtag.getFlowSpec());
		}
		else throw std::runtime_error("Should be impossible, since sender is adding tag.");

		if (m_calc != 0) 
		{
			m_calc->Update ();
		}

		// end receiving packets
	}

  // end Receiver::Receive
}
