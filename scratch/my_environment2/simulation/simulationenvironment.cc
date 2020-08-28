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
#include "ns3/lte-ue-net-device.h"

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

#include "ns3/lte-helper.h"
#include "ns3/eps-bearer.h"

#include <ctime>
#include <sstream>
#include <numeric>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <cstdlib>

#include "../apps/mysender.h"
#include "../apps/myreceiver.h"
#include "../apps/mynoisemachine.h"
#include "ns3/point-to-point-epc-helper.h"
#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"

#define INPUT_FILE "scratch/my_environment/input/flow.json"

#define RECEIVER_NODE_COUNT 8u

// Actual packet sizes are limited to [100, 1400]
// (Fragmentation breaks packet counter, so have to avoid it)
#define PACKET_SIZE_MEAN 1250
#define PACKET_SIZE_SD 75

// output limits
#define ACTIVE_COUNT_MAX 750u
#define OUTPUT_SIZE_MAX 75u

// Roughly calculated, only valid for current scenario.
// Distance AP -> corner == sqrt(7.5^2 + 7.5^2) ~= 10.1
#define MAX_DISTANCE 10.1


using namespace ns3;

TypeId SimulationEnvironment::GetTypeId()
{
	static TypeId tid = TypeId ("SimulationEnvironment")
													.SetParent<OpenGymEnv> ()
													.SetGroupName ("OpenGym")
													.AddConstructor<SimulationEnvironment> ();
	return tid;
}
// Hard-coded for 2 flows. Probably not great, but eh. Alternatively, in createDefaultEnvironment, get the actual count from MySender and reinitialize the vectors
SimulationEnvironment::SimulationEnvironment(double inter) : interval(inter), nextFlowId(0), score(2), sent(2), recv(2), sentSize(2), sentPacketMap(), recvPacketMap(),
	completedFlows(), cancelledFlows(), sendApplication(nullptr), nodes(), noiseNode(nullptr)
{
	this->setupDefaultEnvironment();
}
void SimulationEnvironment::AddCompletedFlow(unsigned id, const FlowSpec& spec)
{
	completedFlows.push_back(id);
	// +1 to give the benefit of the doubt; packet could be in transit and about to arrive. 
	// Giving the benefit of the doubt, if ratio almost that high is more likely to be correct than incorrect.
	double arrivalRate = (static_cast<double>(recvPacketMap.at(id) + 1) / static_cast<double>(sentPacketMap.at(id)));
	//std::cout << "Flow has achieved an arrival rate of " << std::setw(5) << arrivalRate << std::endl;
	if (arrivalRate > (1.0 - spec.fullRewardDropPercentage))
	{
		score[spec.id] += spec.value;
	}
	else if (arrivalRate > (1.0 - spec.smallRewardDropPercentage))
	{
		score[spec.id] += spec.value * spec.smallRewardValuePercentage;
	}
	else 
	{
		score[spec.id] += spec.value * spec.badRewardValuePercentage;
	}
}

void SimulationEnvironment::AddFlowId(unsigned id)
{
	sentPacketMap.emplace(id, 0);
	recvPacketMap.emplace(id, 0);
}

void SimulationEnvironment::AddSentPacket(unsigned flowId, unsigned packetSize, const FlowSpec& spec)
{
	sentPacketMap.at(flowId) += 1;
	sent[spec.id] += 1;
	sentSize[spec.id] += packetSize;
}

void SimulationEnvironment::AddReceivedPacket(unsigned flowId, const FlowSpec& spec)
{
	if (recvPacketMap.find(flowId) != recvPacketMap.end())
	{
		recvPacketMap.at(flowId) += 1;
		recv[spec.id] += 1;
	}
}


void SimulationEnvironment::SetupLTEEnvironment()
{
	lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper>  epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
	ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

	// Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device
  //Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

	NodeContainer ueNodes;
	sendNode.Create(1);
	ueNodes.Create(RECEIVER_NODE_COUNT);

  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	// // AP position (central in grid)
	positionAlloc->Add (Vector (2.5, 2.5, 0.0));
	// // All other nodes.
	positionAlloc->Add (Vector (0.1, 0.1, 0.0)); positionAlloc->Add (Vector (2.5, 0.1, 0.0)); positionAlloc->Add (Vector (4.9, 0.1, 0.0));
	positionAlloc->Add (Vector (0.1, 2.5, 0.0));  positionAlloc->Add (Vector (4.9, 2.5, 0.0));
	positionAlloc->Add (Vector (0.1, 4.9, 0.0)); positionAlloc->Add (Vector (2.5, 4.9, 0.0)); positionAlloc->Add (Vector (4.9, 4.9, 0.0));
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(sendNode);
	mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", 
														"Mode", StringValue("Time"), 
														"Speed", StringValue("ns3::UniformRandomVariable[Min=0|Max=3]"), 
														"Time", TimeValue(Seconds(2)),
														"Direction", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=6.283184]"),
														"Bounds", StringValue("0|15|0|15"));
  mobility.Install(ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (sendNode);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
	{
		Ptr<Node> ueNode = ueNodes.Get (u);
		// Set the default gateway for the UE
		Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
		ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
	}

  // Attach one UE per eNodeB
  for (uint16_t i = 0; i < RECEIVER_NODE_COUNT; i++)
	{
		lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(0));
		// side effect: the default EPS bearer will be activated
	}

	for (auto x = ueNodes.Begin(); x != ueNodes.End(); ++x)
	{
		nodes.Add(*x);
	}
}

// #include "lte_env.h"
#include "wifi_env.h"

void SimulationEnvironment::setupDefaultEnvironment()
{
	#ifndef ENV_SELECTOR
	std::cout << "Do you want to use the LTE or the WiFi environment? Please enter '0' for wifi, '1' for LTE." << std::endl;
	unsigned selector = 0;
	std::cin >> selector;
	#else
		#if ENV_SELECTOR==0
			unsigned selector = 0;
		#elif ENV_SELECTOR==1
			unsigned selector = 1;
		#else
			throw std::runtime_error("Invalid ENV_SELECTOR definition value.");
		#endif
	#endif
	if (selector != 0 && selector != 1)
		throw std::runtime_error("Please enter one of the values requested.");
	if (selector == 1)
	{
		this->SetupLTEEnvironment();
	}
	else
	{
		this->SetupWifiEnvironment();
	}

}
void SimulationEnvironment::Activate()
{
	Simulator::ScheduleNow(&SimulationEnvironment::CreateApplications, this);
	Simulator::ScheduleNow(&SimulationEnvironment::StateRead, this);
}
void SimulationEnvironment::CreateApplications()
{
	auto AP = nodes.Get(nodes.GetN() - 1);
	std::vector<Ptr<MyReceiver>> receivers;
	std::vector<Ipv4Address> recvAddresses;
	for (auto i = 0u; i < RECEIVER_NODE_COUNT; ++i)
	{
		receivers.emplace_back(CreateObject<MyReceiver>(ns3::Ptr<SimulationEnvironment>(this), nodes.Get(i)));
		nodes.Get(i)->AddApplication(receivers.back());
		recvAddresses.push_back(nodes.Get(i)->GetObject<ns3::Ipv4>()->GetAddress(1,0).GetLocal());
		receivers.back()->SetStartTime(ns3::Time::FromInteger(100, ns3::Time::Unit::MS));
	}
	std::vector<int> appCounts = {static_cast<int>(std::rand() % 70 + 3), static_cast<int>(std::rand() % 30 + 3)};
	this->sendApplication = CreateObject<MySender>(ns3::Ptr<SimulationEnvironment>(this), recvAddresses, AP,
	 PACKET_SIZE_MEAN, PACKET_SIZE_SD, std::move(appCounts));
	AP->AddApplication(this->sendApplication);
	this->sendApplication->SetStartTime(ns3::Time::FromInteger(750, ns3::Time::Unit::MS));
	if (noiseNode != nullptr)
	{
		auto noiseApp = CreateObject<MyNoiseMachine>(noiseDevice.Get(0));
		noiseNode->AddApplication(noiseApp);
		noiseApp->SetStartTime(ns3::Time::FromInteger(150, ns3::Time::Unit::MS));
	}
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
	//score -= cancelledFlows.size() * 10;
	for (auto x : cancelledFlows)
	{
		sentPacketMap.erase(x);
		recvPacketMap.erase(x);
	}	

	cancelledFlows.clear();
}

void SimulationEnvironment::StateRead()
{
	//std::cout << "Step " << progress++ << std::endl;
	//std::cout << "Stateread!" << std::endl;
	Simulator::Schedule(Time::FromDouble(interval, Time::S), &SimulationEnvironment::StateRead, this);
	std::cout << "Notify 0." << std::endl;
	Notify();
	std::cout << "Notify 1." << std::endl;
	Notify();
	std::cout << "Notified!" << std::endl;
}	

void SimulationEnvironment::HandleFlowCancellation(std::vector<unsigned>& indices, const FlowSpec& spec)
{
	score[spec.id] += spec.value * spec.cancelRewardValuePercentage * indices.size();
	cancelledFlows.insert(cancelledFlows.end(), std::make_move_iterator(indices.begin()), std::make_move_iterator(indices.end()));
}
Ptr<OpenGymSpace> SimulationEnvironment::GetActionSpace()
{
	Ptr<OpenGymTupleSpace> space = CreateObject<OpenGymTupleSpace>();
	space->Add(CreateObject<OpenGymDiscreteSpace>(65));
	return space;
}
bool SimulationEnvironment::ExecuteActions(Ptr<OpenGymDataContainer> action)
{
	static auto index = 0u;
	auto space = DynamicCast<OpenGymTupleContainer>(action);
	auto actionOne = DynamicCast<OpenGymDiscreteContainer>(space->Get(index));
	int valueOne = actionOne->GetValue();

	this->handleCancelledFlows();

	this->sendApplication->incrementActiveFlows(index, valueOne);
	return true;
}

Ptr<OpenGymDataContainer> SimulationEnvironment::GetObservation()
{
	auto observation = CreateObject<OpenGymTupleContainer>();
	static auto index = 0u;
	auto flowIndex = CreateObject<OpenGymDiscreteContainer>(1);
	flowIndex->SetValue(index);
	auto perfPerf = CreateObject<OpenGymDiscreteContainer>(1), goodPerf = CreateObject<OpenGymDiscreteContainer>(1), okayPerf = CreateObject<OpenGymDiscreteContainer>(1), badPerf = CreateObject<OpenGymDiscreteContainer>(1);
	ns3::Ptr<OpenGymDiscreteContainer> containers[4] = {perfPerf, goodPerf, okayPerf, badPerf};
	auto indicator = CreateObject<OpenGymDiscreteContainer>(1);
	indicator->SetValue(0);

	for (auto i = 0u; i < 4; ++i) 
		containers[i]->SetValue(0);
	
	if (sent[index] > 0)
	{
		auto arrivalPercentage = static_cast<double>(recv[index]) / sent[index];
		std::cout << "Category " << index << " arrival percent = " << arrivalPercentage << std::endl;
		if (arrivalPercentage >= 0.99999)
			containers[0]->SetValue(1);
		else if (arrivalPercentage > (1.0 - this->sendApplication->getFlowSpecs()[index].fullRewardDropPercentage))
		{
			containers[1]->SetValue(1);
		}
		else if (arrivalPercentage > (1.0 - this->sendApplication->getFlowSpecs()[index].smallRewardDropPercentage))
		{
			containers[2]->SetValue(1);
		}
		else
		{
			containers[3]->SetValue(1);
		}
	}
	else
	{
		std::cout << "Category " << index << " arrival percentage not relevant." << std::endl;
		indicator->SetValue(1);
	}

	observation->Add(flowIndex);
	observation->Add(perfPerf);
	observation->Add(goodPerf);
	observation->Add(okayPerf);
	observation->Add(badPerf);
	observation->Add(indicator);
	
	std::cout << "Observation: " << flowIndex << perfPerf << goodPerf << okayPerf << badPerf << indicator << std::endl;
	
	if (index == 1)
	{
		sentSize[0] = 0; sentSize[1] = 0;
		sent[0] = 0; sent[1] = 0;
		recv[0] = 0; recv[1] = 0;
		index = 0u;
	}
	else index = 1u;
	
	return observation;
}
Ptr<OpenGymSpace> SimulationEnvironment::GetObservationSpace()
{
	// Not sure if all the duplication is necessary - might be able to just do space, category, fill in cat, then add cat twice. 
	// But this is always correct. Issues with the other method might be hard to find.
	auto space = CreateObject<OpenGymTupleSpace>();
	auto indexIndicator = CreateObject<OpenGymDiscreteSpace>(1);
	auto perfPerfCatOne = CreateObject<OpenGymDiscreteSpace>(1), goodPerfCatOne = CreateObject<OpenGymDiscreteSpace>(1), okayPerfCatOne = CreateObject<OpenGymDiscreteSpace>(1), badPerfCatOne = CreateObject<OpenGymDiscreteSpace>(1);
	auto zeroIndicatorOne = CreateObject<OpenGymDiscreteSpace>(1);
	
	space->Add(indexIndicator);
	space->Add(perfPerfCatOne);
	space->Add(goodPerfCatOne); 
	space->Add(okayPerfCatOne);
	space->Add(badPerfCatOne);
	space->Add(zeroIndicatorOne);

	return space;
}
bool SimulationEnvironment::GetGameOver()
{
	return false;
}

float SimulationEnvironment::GetReward()
{
	static auto reward = 0.0f;
	static auto index = 0u;
	// Super simple reward function, let's try how well this does.
	if (index == 0)
	{
		reward = 0.0f;
		// Hard-coded for two flow categories
		for (auto i = 0u; i < 2; ++i)
		{
			reward += score[i];
			score[i] = 0; 
		}
		std::cout << "Obtained reward = " << reward << std::endl;
		index = 1;
	}
	else
	{
		std::cout << "re-used reward = " << reward << std::endl;
		index = 0;
	}
	return reward;
}


std::string SimulationEnvironment::GetExtraInfo()
{
	// Nothing to add here, for now.
	return "";
}

void SimulationEnvironment::SetupWifiEnvironment()
{
	nodes.Create(RECEIVER_NODE_COUNT);
	NodeContainer nodeSet;
	nodeSet.Create(2);
	auto apNode = nodeSet.Get(0);
	sendNode.Add(apNode);
	noiseNode = nodeSet.Get(1);
	// Install mobility
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
	// Non-zero positions because randomwalk model has infinite loop if things spawn on the edge (or maybe only on the corners?)
	// Changed position for both coordinates to avoid corners, 

	// AP position (central in grid)
	positionAlloc->Add (Vector (2.5, 2.5, 0.0));
	// NoiseDevice Position
	positionAlloc->Add (Vector (1.0, 1.0, 0.0));
	// All other nodes.
	positionAlloc->Add (Vector (0.1, 0.1, 0.0)); positionAlloc->Add (Vector (2.5, 0.1, 0.0)); positionAlloc->Add (Vector (4.9, 0.1, 0.0));
	positionAlloc->Add (Vector (0.1, 2.5, 0.0));  positionAlloc->Add (Vector (4.9, 2.5, 0.0));
	positionAlloc->Add (Vector (0.1, 4.9, 0.0)); positionAlloc->Add (Vector (2.5, 4.9, 0.0)); positionAlloc->Add (Vector (4.9, 4.9, 0.0));
	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); 
	mobility.Install(nodeSet);
	mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", 
													"Mode", StringValue("Time"), 
													"Speed", StringValue("ns3::UniformRandomVariable[Min=0|Max=1.5]"), 
													"Time", TimeValue(Seconds(2)),
													"Direction", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=6.283184]"),
													"Bounds", StringValue("0|5|0|5"));
	mobility.Install (nodes);

	NetDeviceContainer receiverDevices;
	NetDeviceContainer senderDevice;
	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
	YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
	wifiPhy.SetChannel (wifiChannel.Create ());
	wifiPhy.Set ("TxPowerStart", DoubleValue (1)); // dBm (1.26 mW)
	wifiPhy.Set ("TxPowerEnd", DoubleValue (1));
	wifiPhy.Set ("Frequency", UintegerValue (5180));
	WifiHelper wifi;
	wifi.SetRemoteStationManager("ns3::AarfWifiManager");
	WifiMacHelper wifiMac;
	wifiMac.SetType ("ns3::AdhocWifiMac");
	receiverDevices = wifi.Install (wifiPhy, wifiMac, nodes);
	senderDevice = wifi.Install(wifiPhy, wifiMac, sendNode);
	wifiPhy.Set("TxPowerStart", DoubleValue(-70));
	wifiPhy.Set("TxPowerEnd", DoubleValue(-70));
	noiseDevice = wifi.Install(wifiPhy, wifiMac, noiseNode);
	
	InternetStackHelper internet;
	internet.SetIpv4StackInstall(true);
	internet.SetIpv6StackInstall(false);
	internet.Install (nodes);
	internet.Install (sendNode);
	internet.Install (noiseNode);

	Ipv4AddressHelper ipAddrs;
	ipAddrs.SetBase ("192.168.0.0", "255.255.255.0");
	ipAddrs.Assign (receiverDevices);
	ipAddrs.Assign (senderDevice);
	ipAddrs.SetBase("192.168.1.0", "255.255.255.0");
	ipAddrs.Assign(noiseDevice);

	nodes.Add(noiseNode);
	nodes.Add(apNode);
}
