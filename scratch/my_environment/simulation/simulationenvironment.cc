#include "simulationenvironment.h"
#include "mysocket.h"
#include "helpers.h"

#include "../node/sendnode.h"
#include "../node/receivenode.h"
#include "flowspec.h"

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
#include "ns3/udp-socket-impl.h"

#include "ns3/opengym-module.h"


#define INPUT_FILE "scratch/my_environment/input/flow.json"
#define PACKET_SIZE 4608U


using namespace ns3;

TypeId SimulationEnvironment::GetTypeId()
{
	static TypeId tid = TypeId ("SimulationEnvironment")
													.SetParent<OpenGymEnv> ()
													.SetGroupName ("OpenGym")
													.AddConstructor<SimulationEnvironment> ();
	return tid;
}

SimulationEnvironment::SimulationEnvironment(unsigned inter) : interval(inter), nextFlowId(0), sender(nullptr), receiver(nullptr), applications{}
{
	// Create the actual environment, a la sim.cc
	// We'd prefer to have this here rather than in a different file so we have the entire setup wrapped in this environment.
	// Later on, that'll let us define new functions to setup different environment configurations.

	this->setupDefaultEnvironment();

	// Read the flow spec from input file - defines bandwidth requirements and such.
	// These are the values used to gauge how well the agent is doing (reward function etc).
	this->readFlowSpec();
	// Currently behaving as if an infinite amount of flows are available, there should be an optional limiting parameter.
	// Reward function needs to be updated to support that, though.

	Simulator::ScheduleNow(&SimulationEnvironment::StateRead, this);
}

void SimulationEnvironment::StateRead()
{
	Simulator::Schedule(Time::FromInteger(interval, Time::US), &SimulationEnvironment::StateRead, this);
	/*for (auto it = this->applications.begin(); it != this->applications.end(); ++it)
	{
		// Only the applications at the front of the list can be complete, as they're oldest -> newest.
		if (!it->complete())
			break;
	}*/
	Notify();
	std::remove_if(this->applications.begin(), this->applications.end(), [](const auto& app) -> bool { return app.isComplete(); });
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
	//std::cout << "Executing actions!" <<std::endl;
	// Open/close flows as required to match number.
	// When closing, we close those most recently opened as they're furthest away from the reward.
	auto currentlyOpen = this->applications.size();
	auto actionCount = static_cast<unsigned>(DynamicCast<OpenGymDiscreteContainer>(action)->GetValue());
	std::cout << "Action = " << actionCount << std::endl;
	if (this->applications.capacity() < actionCount)
		this->applications.reserve(actionCount);
	if (actionCount < currentlyOpen)
	{
		this->applications.resize(actionCount);
	}
	if (actionCount > currentlyOpen)
	{
		for (unsigned i = currentlyOpen; i < actionCount; ++i)
		{
			// We should create a socket pair for each application, install it on the nodes.
			MySocket srcSock(this->sender->GetObject<UdpSocketFactory>()->CreateSocket());
			MySocket dstSock(this->receiver->GetObject<UdpSocketFactory>()->CreateSocket());

			// Get flow id, use as port, increment for future uses.
			auto port = this->nextFlowId;
			++this->nextFlowId;

			auto srcIp = this->sender->getIP();
			auto dstIp = this->receiver->getIP();

			srcSock.get()->Bind(InetSocketAddress(srcIp, port));
			dstSock.get()->Bind(InetSocketAddress(dstIp, port));

			srcSock.get()->Connect(InetSocketAddress(dstIp, port));
			// Construct the new sending application in-place.
			applications.emplace_back(this->flowSpec.period, PACKET_SIZE, this->flowSpec.minThroughput_bps, 0/*this->flowSpec.max_loss*/, std::move(srcSock), std::move(dstSock));
			applications[applications.size()-1].StartApplication();
		}
	}

	return true;
}

Ptr<OpenGymSpace> SimulationEnvironment::GetObservationSpace()
{
	//std::cout << "GetObservationSpace" << std::endl;
	// Active flow count, amount performing well, amount performing acceptably, amount performing badly.
	static Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace>(0, std::numeric_limits<unsigned>::max(), std::vector<unsigned>{4U,}, TypeNameGet<unsigned>());
	return space;
}
Ptr<OpenGymDataContainer> SimulationEnvironment::GetObservation()
{
	//std::cout << "Getting Observation!" <<std::endl;
	// Active flow count, amount performing well/acceptable/badly
	auto activeCount = this->applications.size();
	auto statuses = checkApplicationPerformance(this->applications);

	Ptr<OpenGymBoxContainer<unsigned>> observation = CreateObject<OpenGymBoxContainer<unsigned>>(std::vector<unsigned>(4));
	observation->AddValue(activeCount);
	observation->AddValue(std::get<0>(statuses));
	observation->AddValue(std::get<1>(statuses));
	observation->AddValue(std::get<2>(statuses));


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
	//std::cout << "reward!" <<std::endl;
	auto reward = 0;
	for (const auto& app : applications)
	{
		if (app.isComplete())
		{
			if (app.isSuccessful())
				reward += this->flowSpec.value;
			if (isPerformingBad(app))
				reward -= this->flowSpec.value * 0.2;
		}
		else
		{
			if (isPerformingWell(app))
			{
				reward += 1;
			}
			else if (isPerformingBad(app))
			{
				reward -= 3;
			}
		}
	}
	return reward;
}

std::string SimulationEnvironment::GetExtraInfo()
{
	// Nothing to add here, for now.
	return "";
}

void SimulationEnvironment::setupDefaultEnvironment()
{
	NodeContainer nodes;
	// Lifetime is length of the environment so should be okay!
	this->sender = CreateObject<SendNode>();
	this->receiver = CreateObject<RecvNode>();
	nodes.Add(this->sender);
	nodes.Add(this->receiver);

	PointToPointHelper pointToPoint;
	pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

	NetDeviceContainer devices;
	devices = pointToPoint.Install (nodes);

	InternetStackHelper stack;
	stack.Install (nodes);

	Ipv4AddressHelper address;
	address.SetBase ("10.1.1.0", "255.255.255.0");

	Ipv4InterfaceContainer interfaces = address.Assign (devices);

	std::cout << "Setup complete" << std::endl;
}


void SimulationEnvironment::readFlowSpec()
{
	flowSpec = ::readFlowSpec(INPUT_FILE);
	flowSpec.period *= 1000000; // convert to micro seconds rather than seconds. Ensures consistent internal time unit.
}