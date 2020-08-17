#include "myreceiver.h"
#include "../simulation/simulationenvironment.h"
#include "apps.h"

#include <ostream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

#include "ns3/stats-module.h"
#include "ns3/log.h"
#include "ns3/log-macros-enabled.h"

NS_LOG_COMPONENT_DEFINE("MyReceiver");

MyReceiver::MyReceiver(ns3::Ptr<SimulationEnvironment> ptr)
: env(std::move(ptr))
{}
MyReceiver::~MyReceiver() {};
void MyReceiver::Receive (Ptr<Socket> socket)
{
	// NS_LOG_FUNCTION (this << socket << packet << from);

  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from))) {
      if (InetSocketAddress::IsMatchingType (from)) {
					NS_LOG_ERROR("Received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 ());
          // NS_LOG_INFO ("Received " << packet->GetSize () << " bytes from " <<
          //              InetSocketAddress::ConvertFrom (from).GetIpv4 ());
        }
			
      TimestampTag timestamp;
      // Should never not be found since the sender is adding it, but
      // you never know.
      if (packet->FindFirstMatchingByteTag (timestamp)) {
          Time tx = timestamp.GetTimestamp ();

          if (m_delay != 0) {
              m_delay->Update (Simulator::Now () - tx);
            }
        }

      if (m_calc != 0) {
          m_calc->Update ();
        }

			env->AddReceivedPacket();
      // end receiving packets
    }

  // end Receiver::Receive
}