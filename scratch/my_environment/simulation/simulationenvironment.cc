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
#define LTE_BOOLEAN true

#define RECEIVER_NODE_COUNT 8u

// Actual packet sizes are limited to [100, 1400]
// (Fragmentation breaks packet counter, so have to avoid it)
#define PACKET_SIZE_MEAN 1250
#define PACKET_SIZE_SD 75

// output limits
#define ACTIVE_COUNT_MAX 750u
#define OUTPUT_SIZE_MAX 75u

// Roughly calculated, only valid for current scenario.
// Distance AP -> corner == sqrt(2.5^2 + 2.5^2)
#define MAX_DISTANCE 3.6 


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
														"Speed", StringValue("ns3::UniformRandomVariable[Min=0|Max=1.5]"), 
														"Time", TimeValue(Seconds(2)),
														"Direction", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=6.283184]"),
														"Bounds", StringValue("0|5|0|5"));
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
	//nodes.Add(enbNodes.Get(0));
	//nodes.Add(pgw);
	
	std::cout << "First section ok" << std::endl;


	// nodes.Create(RECEIVER_NODE_COUNT);
	// NodeContainer nodeSet;
	// nodeSet.Create(2);
	// auto apNode = nodeSet.Get(0);
	// noiseNode = nodeSet.Get(1);
	// // Install mobility
	// MobilityHelper mobility;
	// Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
	// // Non-zero positions because randomwalk model has infinite loop if things spawn on the edge (or maybe only on the corners?)
	// // Changed position for both coordinates to avoid corners, 

	// // AP position (central in grid)
	// positionAlloc->Add (Vector (2.5, 2.5, 0.0));
	// // NoiseDevice Position
	// positionAlloc->Add (Vector (1.0, 1.0, 0.0));
	// // All other nodes.
	// positionAlloc->Add (Vector (0.1, 0.1, 0.0)); positionAlloc->Add (Vector (2.5, 0.1, 0.0)); positionAlloc->Add (Vector (4.9, 0.1, 0.0));
	// positionAlloc->Add (Vector (0.1, 2.5, 0.0));  positionAlloc->Add (Vector (4.9, 2.5, 0.0));
	// positionAlloc->Add (Vector (0.1, 4.9, 0.0)); positionAlloc->Add (Vector (2.5, 4.9, 0.0)); positionAlloc->Add (Vector (4.9, 4.9, 0.0));
	// mobility.SetPositionAllocator (positionAlloc);
	// mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel"); 
	// mobility.Install(nodeSet);
	// mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", 
	// 												"Mode", StringValue("Time"), 
	// 												"Speed", StringValue("ns3::UniformRandomVariable[Min=0|Max=1.5]"), 
	// 												"Time", TimeValue(Seconds(2)),
	// 												"Direction", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=6.283184]"),
	// 												"Bounds", StringValue("0|5|0|5"));
	// mobility.Install (nodes);

	// NetDeviceContainer receiverDevices;
	// NetDeviceContainer senderDevice;
	// Ptr<LteHelper> helper = CreateObject<LteHelper>();
	// Config::SetDefault("ns3::LteUePhy::TxPower", DoubleValue(20));
	// Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(30));
	// // helper->SetEnbDeviceAttribute ("DlBandwidth", UintegerValue (25));
	// // helper->SetEnbDeviceAttribute ("UlBandwidth", UintegerValue (25));
	// helper->SetEnbDeviceAttribute ("DlEarfcn", UintegerValue(1575));
	// helper->SetEnbDeviceAttribute ("UlEarfcn", UintegerValue(19575));
	// Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
	// helper->SetEpcHelper(epcHelper);
	// NodeContainer enb_container;
	// enb_container.Add(nodeSet.Get(0));
	// receiverDevices = helper->InstallUeDevice(nodes);
	// senderDevice = helper->InstallEnbDevice(enb_container);
	// InternetStackHelper internet;
	// // internet.SetIpv4StackInstall(true);
	// // internet.SetIpv6StackInstall(false);
	// internet.Install (nodes);
	// Ipv4InterfaceContainer ueIpInterfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(receiverDevices));
	// // uint64_t imsi = 313460000000001;
	// Ipv4StaticRoutingHelper ipv4RoutingHelper;
	// for (auto u = 0u; u < nodes.GetN(); ++u)
	// {
	// 	ipv4RoutingHelper.GetStaticRouting(nodes.Get(u)->GetObject<Ipv4>())->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
	// 	// nodes.Get(u)->GetObject<LteUeNetDevice>()->SetAttribute("Imsi", UintegerValue(imsi));
	// }

	// helper->Attach(receiverDevices, senderDevice.Get(0));
	// // helper->Attach(receiverDevices, senderDevice.Get(0));
	// Config::SetDefault("ns3::LteUePhy::TxPower", DoubleValue(0.5));
	// noiseDevice = helper->InstallUeDevice(noiseNode);
	// // Noise node
	// nodes.Add(noiseNode);
	// // And AP node
	// nodes.Add(apNode);
}

void SimulationEnvironment::setupDefaultEnvironment()
{
	if (LTE_BOOLEAN)
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
		auto noiseApp = CreateObject<MyNoiseMachine>(noiseDevice.Get(0), LTE_BOOLEAN);
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
	Notify();
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
	space->Add(CreateObject<OpenGymDiscreteSpace>(65));
	return space;
}
bool SimulationEnvironment::ExecuteActions(Ptr<OpenGymDataContainer> action)
{
	auto space = DynamicCast<OpenGymTupleContainer>(action);
	auto actionOne = DynamicCast<OpenGymDiscreteContainer>(space->Get(0));
	auto actionTwo = DynamicCast<OpenGymDiscreteContainer>(space->Get(1));
	int valueOne = actionOne->GetValue(), valueTwo = actionTwo->GetValue();

	this->handleCancelledFlows();

	//std::cout << "Executing action = " << flowCount << std::endl;
	this->sendApplication->incrementActiveFlows(0, valueOne);
	this->sendApplication->incrementActiveFlows(1, valueTwo);
	//std::cout << "Returning after action." << std::endl;
	return true;
}

Ptr<OpenGymDataContainer> SimulationEnvironment::GetObservation()
{
	auto observation = CreateObject<OpenGymTupleContainer>();
	std::vector<unsigned> shape; shape.push_back(1); shape.push_back(1);
	
	auto packetDropCategoryOne = CreateObject<OpenGymDiscreteContainer>(), packetDropCategoryTwo = CreateObject<OpenGymDiscreteContainer>();
	ns3::Ptr<OpenGymDiscreteContainer> containers[2] = {packetDropCategoryOne, packetDropCategoryTwo};
	
	for (auto i = 0u; i < 2; ++i)
	{
		if (sent[i] > 0)
		{
			auto arrivalPercentage = static_cast<double>(recv[i] + this->sendApplication->getActiveCount(i)) / sent[i];
			std::cout << "Arrival percentage: " << arrivalPercentage;
			if (arrivalPercentage > (1.0 - this->sendApplication->getFlowSpecs()[i].fullRewardDropPercentage))
			{
				containers[i]->SetValue(0);
			}
			else if (arrivalPercentage > (1.0 - this->sendApplication->getFlowSpecs()[i].smallRewardDropPercentage))
			{
				containers[i]->SetValue(1);
			}
			else
			{
				containers[i]->SetValue(2);
			}
			std::cout << ", leads to value " << containers[i]->GetValue() << std::endl;
		}
		else
		{
			std::cout << "No sent, so adding 0" << std::endl;
			containers[i]->SetValue(0);
		}
	}
	// auto fracContainerOne = CreateObject<OpenGymBoxContainer<float>>(shape), fracContainerTwo = CreateObject<OpenGymBoxContainer<float>>(shape);
	// if (sent[0] > 0)
	// {
	// 	fracContainerOne->AddValue(std::max(0.0f, 1.0f - (static_cast<float>(recv[0]) + this->sendApplication->getActiveCount(0)) / sent[0]));
	// 	//fracContainerOne->AddValue(std::min(1.0f, static_cast<float>(recv[0])/sent[0]));
	// }
	// else 
	// {
	// 	fracContainerOne->AddValue(1.0f);
	// }
	// if (sent[1] > 0)
	// {
	// 	fracContainerTwo->AddValue(std::max(0.0f, 1.0f - (static_cast<float>(recv[1])  + this->sendApplication->getActiveCount(0)) / sent[1]));
	// }
	// else 
	// {
	// 	fracContainerTwo->AddValue(1.0f);
	// }

	// auto sentSizeOne = CreateObject<OpenGymDiscreteContainer>(), sentSizeTwo = CreateObject<OpenGymDiscreteContainer>();
	// sentSizeOne->SetValue(std::min(OUTPUT_SIZE_MAX, static_cast<unsigned>((sentSize[0] / 1024.0 / 1024) + 0.5))); // Convert to MiB to lower the value
	// sentSizeTwo->SetValue(std::min(OUTPUT_SIZE_MAX, static_cast<unsigned>((sentSize[1] / 1024.0 / 1024) + 0.5)));
	sentSize[0] = 0; sentSize[1] = 0;

	auto activeCountOne = CreateObject<OpenGymDiscreteContainer>(), activeCountTwo = CreateObject<OpenGymDiscreteContainer>();
	activeCountOne->SetValue(std::min(ACTIVE_COUNT_MAX, this->sendApplication->getActiveGoal(0)));
	activeCountTwo->SetValue(std::min(ACTIVE_COUNT_MAX, this->sendApplication->getActiveGoal(1)));
	auto CQI = CreateObject<OpenGymBoxContainer<float>>(shape); 

	auto indicatorOne = CreateObject<OpenGymDiscreteContainer>(), indicatorTwo = CreateObject<OpenGymDiscreteContainer>();
	// Add indicator value when doing nothing, should allow for easier re-starts and faster learning
	if (this->sendApplication->getActiveCount(0) != 0)
		indicatorOne->SetValue(0); 
	else indicatorOne->SetValue(1); 

	if (this->sendApplication->getActiveCount(1) != 0)
		indicatorTwo->SetValue(0);
	else indicatorTwo->SetValue(1);

	observation->Add(packetDropCategoryOne); 
	//observation->Add(sentSizeOne); 
	observation->Add(activeCountOne); 
	observation->Add(indicatorOne);
	observation->Add(packetDropCategoryTwo); 
	//observation->Add(sentSizeTwo); 
	observation->Add(activeCountTwo); 
	observation->Add(indicatorTwo);

	double cqi = 0;
	for (auto it = nodes.Begin() + 1; it != nodes.End(); ++it)
	{
		cqi += (**it).GetObject<MobilityModel>()->GetDistanceFrom(sendNode.Get(0)->GetObject<MobilityModel>());
	}
	cqi = 1 - (cqi / (MAX_DISTANCE * nodes.GetN()));
	CQI->AddValue(static_cast<float>(std::max(0.0, std::min(1.0, cqi))));

	observation->Add(CQI);

	// std::cout << "End observationGet" << std::endl;
	std::cout << "Outputting obs: [" << packetDropCategoryOne->GetValue() << /*", " << sentSizeOne->GetValue() <<*/ ", " << activeCountOne->GetValue() << ", " << indicatorOne->GetValue() << ", ";
	std::cout << packetDropCategoryTwo->GetValue() << /*", " << sentSizeTwo->GetValue() << */", " << activeCountTwo->GetValue() << ", " << indicatorTwo->GetValue() << ", " << CQI->GetValue(0) << "]" << std::endl;
	
	return observation;
}

Ptr<OpenGymSpace> SimulationEnvironment::GetObservationSpace()
{
	// Not sure if all the duplication is necessary - might be able to just do space, category, fill in cat, then add cat twice. 
	// But this is always correct. Issues with the other method might be hard to find.
	auto space = CreateObject<OpenGymTupleSpace>();
	std::vector<unsigned> shape; shape.push_back(1); shape.push_back(1);
	auto arrivalFractionOne = CreateObject<OpenGymDiscreteSpace>(3);//CreateObject<OpenGymBoxSpace>(0.0f, 1.0f, shape, TypeNameGet<float>());
	auto arrivalFractionTwo = CreateObject<OpenGymDiscreteSpace>(3);//CreateObject<OpenGymBoxSpace>(0.0f, 1.0f, shape, TypeNameGet<float>());
	
	//auto sentSizeOne = CreateObject<OpenGymDiscreteSpace>(OUTPUT_SIZE_MAX); // Maximum is some max value, because we won't send limits::max()
	//auto sentSizeTwo = CreateObject<OpenGymDiscreteSpace>(OUTPUT_SIZE_MAX); // We also return the size in some unit (MiB) other than bytes, so value is pretty low.
	
	auto activeCountOne = CreateObject<OpenGymDiscreteSpace>(ACTIVE_COUNT_MAX); // Max value, beyond that we just indicate n. 
	auto activeCountTwo = CreateObject<OpenGymDiscreteSpace>(ACTIVE_COUNT_MAX); // This is far more than would be productive, so should be plenty.
	
	auto zeroIndicatorOne = CreateObject<OpenGymDiscreteSpace>(1);
	auto zeroIndicatorTwo = CreateObject<OpenGymDiscreteSpace>(1);
	
	auto cqi = CreateObject<OpenGymBoxSpace>(0, 1, shape, TypeNameGet<float>());

	space->Add(arrivalFractionOne); 
	//space->Add(sentSizeOne); 
	space->Add(activeCountOne); 
	space->Add(zeroIndicatorOne);
	space->Add(arrivalFractionTwo); 
	//space->Add(sentSizeTwo); 
	space->Add(activeCountTwo); 
	space->Add(zeroIndicatorTwo);

	space->Add(cqi);

	return space;
}
bool SimulationEnvironment::GetGameOver()
{
	return false;
}

float SimulationEnvironment::GetReward()
{
	// Super simple reward function, let's try how well this does.
	float ret = 0;
	// Hard-coded for two flow categories
	for (auto i = 0u; i < 2; ++i)
	{
		auto points = score[i]; //, sentCount = sent[i], recvCount = recv[i];
		score[i] = 0; sent[i] = 0; recv[i] = 0;
		ret += points;
	}
	// // Negative reward for not allowing any flows, should motivate agent to keep some open.
	// // Both for BE and QoS traffic, punishment for no QoS is much larger, though.
	// if (this->sendApplication->getActiveGoal(0) == 0)
	// {
	// 	ret -= 18; 
	// }
	// if (this->sendApplication->getActiveGoal(1) == 0)
	// {
	// 	ret -= 2;
	// }
	std::cout << "Obtained reward = " << ret << std::endl;
	return ret; 
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
	wifiPhy.Set("TxPowerStart", DoubleValue(-70));
	wifiPhy.Set("TxPowerEnd", DoubleValue(-70));
	noiseDevice = wifi.Install(wifiPhy, wifiMac, noiseNode);
	nodes.Add(nodeSet.Get(0));
	InternetStackHelper internet;
	internet.SetIpv4StackInstall(true);
	internet.SetIpv6StackInstall(false);
	internet.Install (nodes);
	Ipv4AddressHelper ipAddrs;
	ipAddrs.SetBase ("192.168.0.0", "255.255.255.0");
	ipAddrs.Assign (receiverDevices);
	ipAddrs.Assign (senderDevice);
	ipAddrs.SetBase("192.168.1.0", "255.255.255.0");
	internet.Install(noiseNode);
	ipAddrs.Assign(noiseDevice);
	nodes.Add(noiseNode);
	nodes.Add(apNode);
}