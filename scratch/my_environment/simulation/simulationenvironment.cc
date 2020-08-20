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
#include "ns3/random-walk-2d-mobility-model.h"

#include <ctime>
#include <sstream>
#include <numeric>

#include "../apps/mysender.h"
#include "../apps/myreceiver.h"
#include "../apps/mynoisemachine.h"

#define INPUT_FILE "scratch/my_environment/input/flow.json"

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

SimulationEnvironment::SimulationEnvironment(unsigned inter) : interval(inter), nextFlowId(0), score(0), sent(0), recv(0), sendApplication(nullptr) {}

void SimulationEnvironment::AddCompletedFlow(unsigned id, unsigned s)
{
	std::cout << "Adding completed flow." << std::endl;
	completedFlows.push_back(id);
	// +1 to give the benefit of the doubt; packet could be in transit and about to arrive. 
	// Giving the benefit of the doubt, if ratio almost that high is more likely to be correct than incorrect.
	if (static_cast<double>(recvPacketMap.at(id) + 1) / static_cast<double>(sentPacketMap.at(id)) > 0.95)
		score += s;
	else score -= 1;
}
void SimulationEnvironment::AddFlowId(unsigned id)
{
	sentPacketMap.emplace(id, 0);
	recvPacketMap.emplace(id, 0);
}
void SimulationEnvironment::AddSentPacket(unsigned flowId)
{
	sentPacketMap.at(flowId) += 1;
	sent += 1;
}
void SimulationEnvironment::AddReceivedPacket(unsigned flowId)
{
	if (recvPacketMap.find(flowId) != recvPacketMap.end())
	{
		recvPacketMap.at(flowId) += 1;
		recv += 1;
	}
}
void SimulationEnvironment::setupDefaultEnvironment()
{
	nodes.Create(WIFI_NODE_COUNT + 1);
	
	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
	YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();

	wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel",
																	"Frequency", DoubleValue (5.180e9));
	wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");

	wifiPhy.SetChannel (wifiChannel.Create ());
	wifiPhy.Set ("TxPowerStart", DoubleValue (1)); // dBm (1.26 mW)
	wifiPhy.Set ("TxPowerEnd", DoubleValue (1));
	wifiPhy.Set ("Frequency", UintegerValue (5180));
	WifiHelper wifi;
	wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
	wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
	WifiMacHelper wifiMac;
	wifiMac.SetType ("ns3::AdhocWifiMac");
	NetDeviceContainer nodeDevices = wifi.Install (wifiPhy, wifiMac, nodes);

	InternetStackHelper internet;
	internet.SetIpv4StackInstall(true);
	internet.SetIpv6StackInstall(false);
	internet.Install (nodes);
	Ipv4AddressHelper ipAddrs;
	ipAddrs.SetBase ("192.168.0.0", "255.255.255.0");
	ipAddrs.Assign (nodeDevices);

	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

	// Non-zero positions because randomwalk model has infinite loop if things spawn on the edge (or maybe only on the corners?)
	// Changed position for both coordinates to avoid corners, 

	// AP position (central in grid)
	positionAlloc->Add (Vector (2.5, 2.5, 0.0));
	positionAlloc->Add (Vector (1.0, 1.0, 0.0));
	// All other nodes.
	positionAlloc->Add (Vector (0.1, 0.1, 0.0)); positionAlloc->Add (Vector (2.5, 0.1, 0.0)); positionAlloc->Add (Vector (4.9, 0.1, 0.0));
	positionAlloc->Add (Vector (0.1, 2.5, 0.0));  positionAlloc->Add (Vector (4.9, 2.5, 0.0));
	positionAlloc->Add (Vector (0.1, 4.9, 0.0)); positionAlloc->Add (Vector (2.5, 4.9, 0.0)); positionAlloc->Add (Vector (4.9, 4.9, 0.0));
	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	NodeContainer nodeSet;
	nodeSet.Add(nodes.Get(0));
	nodeSet.Create(1);
	noiseNode = nodeSet.Get(1);
	wifiPhy.Set("TxPowerStart", DoubleValue(-25));
	wifiPhy.Set("TxPowerEnd", DoubleValue(-25));
	auto noiseDevice = wifi.Install(wifiPhy, wifiMac, noiseNode);
	internet.SetIpv4StackInstall(true);
	internet.SetIpv6StackInstall(false);
	ipAddrs.SetBase("192.168.1.0", "255.255.255.0");
	internet.Install(noiseNode);
	ipAddrs.Assign(noiseDevice);
	mobility.Install(nodeSet);
	nodeSet = NodeContainer();
	for (auto it = ++nodes.Begin(); it != nodes.End(); ++it)
	{
		nodeSet.Add(*it);
	}
	mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", 
														"Mode", StringValue("Time"), 
														"Speed", StringValue("ns3::UniformRandomVariable[Min=0|Max=5]"), 
														"Time", TimeValue(Seconds(2)),
														"Direction", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=6.283184]"),
														"Bounds", StringValue("0|5|0|5"));
	mobility.Install (nodeSet);
	
	this->CreateApplications(noiseDevice.Get(0));
	std::cout << "Scheduled stateRead " << interval << " seconds from now." << std::endl;
	//Simulator::Schedule(Time::FromInteger(interval, Time::S), &SimulationEnvironment::StateRead, this);
	Simulator::ScheduleNow(&SimulationEnvironment::StateRead, this);

	std::cout << "Environment set up" << std::endl;
}
void SimulationEnvironment::CreateApplications(ns3::Ptr<ns3::NetDevice> noiseDevice)
{
	auto AP = nodes.Get(0);
	std::vector<Ptr<MyReceiver>> receivers;
	std::vector<Ipv4Address> recvAddresses;
	for (auto i = 1u; i <= WIFI_NODE_COUNT; ++i)
	{
		receivers.emplace_back(CreateObject<MyReceiver>(ns3::Ptr<SimulationEnvironment>(this), nodes.Get(i)));
		nodes.Get(i)->AddApplication(receivers.back());
		recvAddresses.push_back(nodes.Get(i)->GetObject<ns3::Ipv4>()->GetAddress(1,0).GetLocal());
		receivers.back()->SetStartTime(ns3::Time::FromInteger(250, ns3::Time::Unit::MS));
	}
	this->sendApplication = CreateObject<MySender>(ns3::Ptr<SimulationEnvironment>(this), recvAddresses, AP);
	AP->AddApplication(this->sendApplication);
	this->sendApplication->SetStartTime(ns3::Time::FromInteger(500, ns3::Time::Unit::MS));

	auto noiseApp = CreateObject<MyNoiseMachine>(noiseDevice);
	noiseNode->AddApplication(noiseApp);
	noiseApp->SetStartTime(ns3::Time::FromInteger(150, ns3::Time::Unit::MS));
}

void SimulationEnvironment::StateRead()
{
	std::cout << "Stateread!" << std::endl;
	Simulator::Schedule(Time::FromInteger(interval, Time::S), &SimulationEnvironment::StateRead, this);
	Notify();
}	

void SimulationEnvironment::HandleFlowCancellation(std::vector<unsigned>& count)
{
	if (!cancelledFlows.empty()) 
		throw std::runtime_error("Expected empty cancelled flows vector.");
	cancelledFlows = std::move(count);
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
	std::cout << "Executing action = " << flowCount << std::endl;
	NS_ASSERT(flowCount >= 0);
	this->sendApplication->SetActiveFlows(flowCount);
	this->handleCancelledFlows();
	std::cout << "Returning after action." << std::endl;
	return true;
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
	std::cout << "getting observation" << std::endl;
	Ptr<OpenGymBoxContainer<float>> observation = CreateObject<OpenGymBoxContainer<float>>(std::vector<unsigned>(1));
	if (sent != 0) observation->AddValue((1.0 * recv)/sent);
	else observation->AddValue(0);
	std::cout << "End observationGet" << std::endl;
	return observation;
}

bool SimulationEnvironment::GetGameOver()
{
	std::cout << "Runtime = " << Simulator::Now().GetSeconds() << " seconds." << std::endl;
	//std::cout << "GGO!" <<std::endl;
	// Some time frame, probably. Maybe just always false is okay for now.
	// --> We simply support infinite streams for now, python agent controls episode length.
	// + environment defines maximum runtime
	return false;
}
void removeCompleted(std::map<unsigned, unsigned>& recvMap, std::map<unsigned, unsigned>& sentMap, std::vector<unsigned>& completed)
{
	for (auto keyValue : completed) 
	{
		recvMap.erase(keyValue);
		sentMap.erase(keyValue);
	}
	completed.clear();
}
void SimulationEnvironment::handleCancelledFlows()
{
	score -= cancelledFlows.size() * 10;
	for (auto x : cancelledFlows)
	{
		sentPacketMap.erase(x);
		recvPacketMap.erase(x);
	}	

	cancelledFlows.clear();
}
float SimulationEnvironment::GetReward()
{
	auto points = score, sentCount = sent, recvCount = recv;
	score = 0; sent = 0; recv = 0;
	float ret;
	// Score member variable tracks completed & cancelled flow rewards, so we only adjust here 
	// Based on no active flows and on receive fraction.
	if (this->sendApplication->getActiveCount() != 0)
	{
		// Total complete + cancelled rewards,
		// minus 5x (1 - ratio recv/sent)
		ret = points + (-5 * (1 - (1.0 * recvCount)/sentCount));
	}
	else 
	{
		// For activeCount 0, we can't have bonus from completed flows
		// However, extra punishment from cancelled flows should be incurred.
		ret = points - 5;
	}
	removeCompleted(recvPacketMap, sentPacketMap, completedFlows);
	if (ret > 200000)
		throw std::runtime_error("That's not supposed to happen.");
	return ret;
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