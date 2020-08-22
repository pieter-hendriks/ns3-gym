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
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cstdlib>

#include "../apps/mysender.h"
#include "../apps/myreceiver.h"
#include "../apps/mynoisemachine.h"

#define INPUT_FILE "scratch/my_environment/input/flow.json"

#define WIFI_NODE_COUNT 8u

// Actual packet sizes are limited to [100, 1500]
// (Fragmentation breaks packet counter, so have to avoid it)
#define PACKET_SIZE_MEAN 1350
#define PACKET_SIZE_SD 75


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
{}
void SimulationEnvironment::AddCompletedFlow(unsigned id, const FlowSpec& spec)
{
	completedFlows.push_back(id);
	if (static_cast<double>(recvPacketMap.at(id)) / static_cast<double>(sentPacketMap.at(id)) > 1) 
		throw std::runtime_error("U fucking wot");
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

void SimulationEnvironment::setupDefaultEnvironment()
{
	nodes.Create(WIFI_NODE_COUNT + 1);
	
	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
	YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();

	//wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
	//wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	//wifiChannel.SetPropagationDelay("ns3::RandomPropagationDelayModel");

	wifiPhy.SetChannel (wifiChannel.Create ());
	wifiPhy.Set ("TxPowerStart", DoubleValue (1)); // dBm (1.26 mW)
	wifiPhy.Set ("TxPowerEnd", DoubleValue (1));
	wifiPhy.Set ("Frequency", UintegerValue (5180));
	WifiHelper wifi;
	wifi.SetRemoteStationManager("ns3::AarfWifiManager");
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
	wifiPhy.Set("TxPowerStart", DoubleValue(-90));
	wifiPhy.Set("TxPowerEnd", DoubleValue(-90));
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
														"Speed", StringValue("ns3::UniformRandomVariable[Min=0|Max=1.5]"), 
														"Time", TimeValue(Seconds(2)),
														"Direction", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=6.283184]"),
														"Bounds", StringValue("0|5|0|5"));
	mobility.Install (nodeSet);
	
	this->CreateApplications(noiseDevice.Get(0));
	//std::cout << "Scheduled stateRead " << interval << " seconds from now." << std::endl;
	//Simulator::Schedule(Time::FromInteger(interval, Time::S), &SimulationEnvironment::StateRead, this);
	Simulator::ScheduleNow(&SimulationEnvironment::StateRead, this);

	//wifiPhy.EnablePcapAll("mypcap", true);

	//std::cout << "Environment set up" << std::endl;
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
		receivers.back()->SetStartTime(ns3::Time::FromInteger(100, ns3::Time::Unit::MS));
	}
	std::vector<int> appCounts = {static_cast<int>(std::rand() % 120 + 5), static_cast<int>(std::rand() % 120 + 5)};
	this->sendApplication = CreateObject<MySender>(ns3::Ptr<SimulationEnvironment>(this), recvAddresses, AP,
	 PACKET_SIZE_MEAN, PACKET_SIZE_SD, std::move(appCounts));
	AP->AddApplication(this->sendApplication);
	this->sendApplication->SetStartTime(ns3::Time::FromInteger(750, ns3::Time::Unit::MS));
	
	auto noiseApp = CreateObject<MyNoiseMachine>(noiseDevice);
	noiseNode->AddApplication(noiseApp);
	noiseApp->SetStartTime(ns3::Time::FromInteger(150, ns3::Time::Unit::MS));
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

void SimulationEnvironment::HandleFlowCancellation(std::vector<unsigned>& count, const FlowSpec& spec)
{
	score[spec.id] += spec.value * spec.badRewardValuePercentage * count.size();
	cancelledFlows.insert(cancelledFlows.end(), std::make_move_iterator(count.begin()), std::make_move_iterator(count.end()));
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

	std::cout << "Executing action (" << valueOne << ", " << valueTwo << "): Old count = " << "(" << this->sendApplication->getActiveCount(0) << ", " << this->sendApplication->getActiveCount(1) << ")";
	std::cout << "[" << this->sendApplication->getActiveGoal(0) << ", " << this->sendApplication->getActiveGoal(1) << "]";
	//std::cout << "Executing action = " << flowCount << std::endl;
	this->handleCancelledFlows();
	this->sendApplication->incrementActiveFlows(0, valueOne);
	this->sendApplication->incrementActiveFlows(1, valueTwo);
	std::cout << ", new count = " <<  "(" << this->sendApplication->getActiveCount(0) << ", " << this->sendApplication->getActiveCount(1) << ")" << std::endl;
	//std::cout << "Returning after action." << std::endl;
	return true;
}

Ptr<OpenGymDataContainer> SimulationEnvironment::GetObservation()
{
	std::cout << "Getting observation; currently active flows = " << "(" << this->sendApplication->getActiveCount(0) << ", " << this->sendApplication->getActiveCount(1) << ")" << std::endl;
	auto observation = CreateObject<OpenGymTupleContainer>();
	std::vector<unsigned> shape; shape.push_back(1); shape.push_back(1);
	auto fracContainerOne = CreateObject<OpenGymBoxContainer<float>>(shape), fracContainerTwo = CreateObject<OpenGymBoxContainer<float>>(shape);
	if (sent[0] > 0)
		fracContainerOne->AddValue(static_cast<float>(recv[0])/sent[0]);
	else 
	{
		fracContainerOne->AddValue(1); // 1 for consistency: All sent packets have arrived.
	}
	if (sent[1] > 0)
	fracContainerTwo->AddValue(static_cast<float>(recv[1])/sent[1]);
	else 
	{
		fracContainerTwo->AddValue(1); // 1 for consistency: All sent packets have arrived.
	}

	auto sentSizeOne = CreateObject<OpenGymDiscreteContainer>(), sentSizeTwo = CreateObject<OpenGymDiscreteContainer>();
	sentSizeOne->SetValue(sentSize[0]); sentSizeTwo->SetValue(sentSize[1]);
	sentSize[0] = 0; sentSize[1] = 0;
	
	auto activeCountOne = CreateObject<OpenGymDiscreteContainer>(), activeCountTwo = CreateObject<OpenGymDiscreteContainer>();
	activeCountOne->SetValue(this->sendApplication->getActiveGoal(0));
	activeCountTwo->SetValue(this->sendApplication->getActiveGoal(1));

	auto indicatorOne = CreateObject<OpenGymDiscreteContainer>(), indicatorTwo = CreateObject<OpenGymDiscreteContainer>();
	// Add indicator value when doing nothing, should allow for easier re-starts and faster learning
	if (this->sendApplication->getActiveCount(0) != 0)
		indicatorOne->SetValue(0); 
	else indicatorOne->SetValue(1); 

	if (this->sendApplication->getActiveCount(1) != 0)
		indicatorTwo->SetValue(0);
	else indicatorTwo->SetValue(1);
	observation->Add(fracContainerOne); observation->Add(sentSizeOne); observation->Add(activeCountOne); observation->Add(indicatorOne);
	observation->Add(fracContainerTwo); observation->Add(sentSizeTwo); observation->Add(activeCountTwo); observation->Add(indicatorTwo);
	// Add single constant value, useful if e.g. all zeroes.
	observation->Add(CreateObject<OpenGymDiscreteContainer>(1));
	// std::cout << "End observationGet" << std::endl;
	std::cout << "Outputting obs: [" << fracContainerOne->GetValue(0) << ", " << sentSizeOne->GetValue() << ", " << activeCountOne->GetValue() << ", " << indicatorOne->GetValue() << ", ";
	std::cout << fracContainerTwo->GetValue(0) << ", " << sentSizeTwo->GetValue() << ", " << activeCountTwo->GetValue() << ", " << indicatorTwo->GetValue() << ", 1]" << std::endl;
	return observation;
}

Ptr<OpenGymSpace> SimulationEnvironment::GetObservationSpace()
{
	// Not sure if all the duplication is necessary - might be able to just do space, category, fill in cat, then add cat twice. 
	// But this is always correct. Issues with the other method might be hard to find.
	auto space = CreateObject<OpenGymTupleSpace>();
	auto constantValue = CreateObject<OpenGymDiscreteSpace>(1);
	std::vector<unsigned> shape; shape.push_back(1); shape.push_back(1);
	auto arrivalFractionOne = CreateObject<OpenGymBoxSpace>(0, 1, shape, TypeNameGet<float>());
	auto arrivalFractionTwo = CreateObject<OpenGymBoxSpace>(0, 1, shape, TypeNameGet<float>());
	
	auto sentSizeOne = CreateObject<OpenGymDiscreteSpace>(std::numeric_limits<int>::max());
	auto sentSizeTwo = CreateObject<OpenGymDiscreteSpace>(std::numeric_limits<int>::max());
	
	auto activeCountOne = CreateObject<OpenGymDiscreteSpace>(std::numeric_limits<int>::max());
	auto activeCountTwo = CreateObject<OpenGymDiscreteSpace>(std::numeric_limits<int>::max());
	
	auto zeroIndicatorOne = CreateObject<OpenGymDiscreteSpace>(1);
	auto zeroIndicatorTwo = CreateObject<OpenGymDiscreteSpace>(1);
	
	space->Add(arrivalFractionOne); space->Add(sentSizeOne); space->Add(activeCountOne); space->Add(zeroIndicatorOne);
	space->Add(arrivalFractionTwo); space->Add(sentSizeTwo); space->Add(activeCountTwo); space->Add(zeroIndicatorTwo);
	space->Add(constantValue);

	return space;
}
bool SimulationEnvironment::GetGameOver()
{
	//std::cout << "GGO!" <<std::endl;
	// Some time frame, probably. Maybe just always false is okay for now.
	// --> We simply support infinite streams for now, python agent controls episode length.
	// + environment defines maximum runtime
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
	return ret; 

	// auto points = score, sentCount = sent, recvCount = recv;
	// score = 0; sent = 0; recv = 0, sentSize = 0;
	// float ret = 0;
	// //std::cout << "Getting reward:\n\tFlowPoints:" << points << "\n\tArrival Fraction: " << (sentCount > 0 ? (-5 * (1 - (1.0 * recvCount)/sentCount)) : 1010101010101010ULL) << std::endl;
	// // Score member variable tracks completed & cancelled flow rewards, so we only adjust here 
	// // Based on no active flows and on receive fraction.
	// if (this->sendApplication->getActiveGoal() != 0)
	// {
	// 	// Total complete + cancelled rewards,
	// 	// minus 5x (1 - ratio recv/sent)
	// 	//std::cout << "\t" << points << " + " << (-5*(1-(1.0 * recvCount) / sentCount)) << std::endl;
	// 	ret = points + (-5 * (1 - (1.0 * recvCount)/sentCount) * this->sendApplication->getActiveCount());
	// }
	// else 
	// {
	// 	// For activeCount 0, we can't have bonus from completed flows
	// 	// However, extra punishment from cancelled flows should be incurred.
	// 	// in theory, probably doesn't actually matter since this is just a sentinel 'do-not-do-this' value
	// 	ret = points - 100000;
	// }
	// //std::cout << "\tTotal: " << ret << std::endl;
	// removeCompleted(recvPacketMap, sentPacketMap, completedFlows);
	// if (ret > 200000)
	// 	throw std::runtime_error("That's not supposed to happen.");
	// return ret;
}


std::string SimulationEnvironment::GetExtraInfo()
{
	// Nothing to add here, for now.
	return "";
}