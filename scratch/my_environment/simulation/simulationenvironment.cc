#include "simulationenvironment.h"
#include "mysocket.h"
#include "helpers.h"

// #include "../node/sendnode.h"
// #include "../node/receivenode.h"
#include "flow.h"

#include "ns3/string.h"
#include "ns3/node-container.h"
#include "ns3/node.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/net-device-container.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/type-name.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/udp-header.h"
#include "ns3/udp-server.h"
#include "ns3/udp-trace-client.h"

#include "ns3/wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/mobility-helper.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/stats-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/opengym-module.h"
#include <ctime>
#include <sstream>

#include "../apps/mysender.h"
#include "../apps/myreceiver.h"

#define INPUT_FILE "scratch/my_environment/input/flow.json"
#define PACKET_SIZE 4608U
#define WIFI_NODE_COUNT 8u


using namespace ns3;

TypeId SimulationEnvironment::GetTypeId()
{
	static TypeId tid = TypeId ("SimulationEnvironment")
													.SetParent<OpenGymEnv> ()
													.SetGroupName ("OpenGym")
													.AddConstructor<SimulationEnvironment> ();
	return tid;
}

SimulationEnvironment::SimulationEnvironment(unsigned inter) : interval(inter), nextFlowId(0), score(0), sent(0), recv(0), sendApplication(nullptr)
{
	// Create the actual environment, a la sim.cc
	// We'd prefer to have this here rather than in a different file so we have the entire setup wrapped in this environment.
	// If necessary, we can define new functions to setup different environment configurations.
	this->setupDefaultEnvironment();
	
	Simulator::ScheduleNow(&SimulationEnvironment::StateRead, this);
}

void SimulationEnvironment::AddScore(unsigned s)
{
	score += s;
}
void SimulationEnvironment::AddSentPacket()
{
	++sent;
}
void SimulationEnvironment::AddReceivedPacket()
{
	++recv;
}

void SimulationEnvironment::setupDefaultEnvironment()
{
	NodeContainer nodes; nodes.Create(WIFI_NODE_COUNT + 1);
	auto AP = nodes.Get(0);

	WifiHelper wifi;
	WifiMacHelper wifiMac;
	wifiMac.SetType ("ns3::AdhocWifiMac");
	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
	YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
	wifiPhy.SetChannel (wifiChannel.Create ());
	NetDeviceContainer nodeDevices = wifi.Install (wifiPhy, wifiMac, nodes);

	InternetStackHelper internet;
	internet.Install (nodes);
	Ipv4AddressHelper ipAddrs;
	ipAddrs.SetBase ("192.168.0.0", "255.255.255.0");
	ipAddrs.Assign (nodeDevices);

	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
	// AP position (central in grid)
	positionAlloc->Add (Vector (1.0, 1.0, 0.0));
	// All other nodes.
	positionAlloc->Add (Vector (0.0, 0.0, 0.0)); positionAlloc->Add (Vector (1.0, 0.0, 0.0)); positionAlloc->Add (Vector (2.0, 0.0, 0.0));
	positionAlloc->Add (Vector (0.0, 1.0, 0.0));  positionAlloc->Add (Vector (2.0, 1.0, 0.0));
	positionAlloc->Add (Vector (0.0, 2.0, 0.0)); positionAlloc->Add (Vector (1.0, 2.0, 0.0)); positionAlloc->Add (Vector (2.0, 2.0, 0.0));
	mobility.SetPositionAllocator (positionAlloc);
	mobility.Install (nodes);

	std::vector<Ptr<MyReceiver>> receivers;
	std::vector<Ipv4Address> recvAddresses;
	for (auto i = 1u; i <= WIFI_NODE_COUNT; ++i)
	{
		receivers.emplace_back(CreateObject<MyReceiver>(ns3::Ptr<SimulationEnvironment>(this)));
		nodes.Get(i)->AddApplication(receivers.back());
		recvAddresses.push_back(nodes.Get(i)->GetObject<ns3::Ipv4>()->GetAddress(1,0).GetLocal());
		receivers.back()->SetStartTime(Seconds(0));
	}
	this->sendApplication = CreateObject<MySender>(ns3::Ptr<SimulationEnvironment>(this), recvAddresses);
	AP->AddApplication(this->sendApplication);
	this->sendApplication->SetStartTime(Seconds(0.25));

}

void SimulationEnvironment::StateRead()
{
	Simulator::Schedule(Time::FromInteger(interval, Time::S), &SimulationEnvironment::StateRead, this);
	/*for (auto it = this->applications.begin(); it != this->applications.end(); ++it)
	{
		// Only the applications at the front of the list can be complete, as they're oldest -> newest.
		if (!it->complete())
			break;
	}*/
	Notify();
}



Ptr<OpenGymSpace> SimulationEnvironment::GetActionSpace()
{
	//std::cout << "GetActionSpace" << std::endl;
	// Single-digit action (# flows to have open in the next step)
	// We can't do limits<unsigned>::max() because discretespace takes integer parameter, would wrap around to negative.
	static Ptr<OpenGymDiscreteSpace> space = CreateObject<OpenGymDiscreteSpace>(std::numeric_limits<int>::max());
	return space;
}
bool SimulationEnvironment::ExecuteActions(Ptr<OpenGymDataContainer> action)
{
	auto flowCount = static_cast<int>(DynamicCast<OpenGymDiscreteContainer>(action)->GetValue());
	NS_ASSERT(flowCount >= 0);
	this->sendApplication->SetActiveFlows(flowCount);

	return true;
	// // Open/close flows as required to match number.
	// // When closing, we close those most recently opened as they're furthest away from the reward.
	// auto currentlyOpen = this->applications.size();
	
	// std::cout << "Executing actions (actionCount = " << actionCount << ")!" <<std::endl;
	// if (this->applications.capacity() < actionCount)
	// 	this->applications.reserve(actionCount);
	// if (actionCount < currentlyOpen)
	// {
	// 	std::cout << "Removing action" << std::endl;
	// 	this->applications.resize(actionCount);
	// 	std::cout << "Removed action!" << std::endl;
	// }
	// if (actionCount > currentlyOpen)
	// {
	// 	std::cout << "Activating " << actionCount - currentlyOpen << " extra flows." << std::endl;
	// 	for (unsigned i = currentlyOpen; i < actionCount; ++i)
	// 	{
	// 		// We should create a socket pair for each application, install it on the nodes.
	// 		MySendSocket srcSock(this->sender->GetObject<UdpSocketFactory>()->CreateSocket());
	// 		MyRecvSocket dstSock(this->receiver->GetObject<UdpSocketFactory>()->CreateSocket());

	// 		// Get flow id, use as port, increment for future uses.
	// 		auto port = this->nextFlowId;
	// 		++this->nextFlowId;

	// 		auto srcIp = this->sender->getIP();
	// 		auto dstIp = this->receiver->getIP();

	// 		srcSock.get()->Bind(InetSocketAddress(srcIp, port));
	// 		dstSock.get()->Bind(InetSocketAddress(dstIp, port));

	// 		srcSock.get()->Connect(InetSocketAddress(dstIp, port));
	// 		// Construct the new sending application in-place.
	// 		applications.emplace_back(this->flowSpec.period, PACKET_SIZE, this->flowSpec.minThroughput_bps, 0/*this->flowSpec.max_loss*/, std::move(srcSock), std::move(dstSock));
	// 		applications[applications.size()-1].StartApplication();
	// 		if ((i - currentlyOpen) % 1000 == 0)
	// 		{
	// 			std::cout << i - currentlyOpen << " are done!" << std::endl;
	// 		}
	// 	}
	// 	std::cout << actionCount - currentlyOpen << " extra flows have been activated." << std::endl;
	// }
	// std::cout << "Actions executed!" << std::endl;
	// return true;
}

Ptr<OpenGymSpace> SimulationEnvironment::GetObservationSpace()
{
	//std::cout << "GetObservationSpace" << std::endl;
	// Active flow count, amount performing well, amount performing acceptably, amount performing badly.
	static Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace>(0, 1, std::vector<unsigned>{1U,}, TypeNameGet<float>());
	return space;
}
Ptr<OpenGymDataContainer> SimulationEnvironment::GetObservation()
{
	//std::cout << "Getting Observation!" <<std::endl;
	// Active flow count, amount performing well/acceptable/badly

	Ptr<OpenGymBoxContainer<float>> observation = CreateObject<OpenGymBoxContainer<float>>(std::vector<unsigned>(1));
	if (sent != 0) observation->AddValue((1.0 * recv)/sent);
	else observation->AddValue(0);
	return observation;
}

bool SimulationEnvironment::GetGameOver()
{
	//std::cout << "GGO!" <<std::endl;
	// Some time frame, probably. Maybe just always false is okay for now.
	// --> We simply support infinite streams for now, python agent controls episode length.
	return false;
}

float SimulationEnvironment::GetReward()
{
	if (this->sendApplication->getActiveCount() != 0)
		return score - (5 * recv/sent);
	return -5;
}

std::string SimulationEnvironment::GetExtraInfo()
{
	// Nothing to add here, for now.
	return "";
}


// Old Default Env 
// NodeContainer nodes;
	// // Lifetime is length of the environment so should be okay!
	// this->sender = CreateObject<SendNode>();
	// this->receiver = CreateObject<RecvNode>();
	// nodes.Add(this->sender);
	// nodes.Add(this->receiver);

	// PointToPointHelper pointToPoint;
	// pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	// pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

	// NetDeviceContainer devices;
	// devices = pointToPoint.Install (nodes);

	// InternetStackHelper stack;
	// stack.Install (nodes);

	// Ipv4AddressHelper address;
	// address.SetBase ("10.1.1.0", "255.255.255.0");

	// Ipv4InterfaceContainer interfaces = address.Assign (devices);

	// std::cout << "Setup complete" << std::endl;