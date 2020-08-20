#include "mysender.h"
#include "../simulation/simulationenvironment.h"
#include "apps.h"
#include "flowtag.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/stats-module.h"
#include "ns3/log.h"
#include "ns3/log-macros-enabled.h"

#include <stdexcept>
#include <ostream>
#include <cmath>
#include <ctime>
#include <iomanip>
NS_LOG_COMPONENT_DEFINE("MySender");

#define MAX_PACKET_SIZE 1500U
#define MIN_PACKET_SIZE 100U
// TypeId MySender::GetTypeId (void)
// {
// 	static TypeId tid = TypeId("MySender").SetParent<Sender>().AddConstructor<MySender>();
// 	return tid;
// };
MySender::MySender(ns3::Ptr<SimulationEnvironment> ptr, const std::vector<Ipv4Address>& addresses, ns3::Ptr<Node> node, double pkSzMean, double pkSzSD) 
: active(false), receivers(std::move(addresses)), currentReceiverIndex(0), flowList(), flowsToRecreate(0), flowspec(readFlowSpec("scratch/my_environment/input/flow.json")),
 env(std::move(ptr)), currentFlowGoal(0), generator(std::time(nullptr)), packetSizeDist(std::make_unique<std::normal_distribution<>>(pkSzMean, pkSzSD))
	
{ 
	this->m_node = node;
	this->m_pktSize = 576; 
}
MySender::~MySender() 
{
	NS_LOG_FUNCTION_NOARGS ();
	if (active)	
		this->StopApplication();
}
void MySender::createFlow()
{
	static std::normal_distribution<double> dist(0, 100);
	auto& addedFlow = flowList.emplace_back(Flow(flowspec, currentReceiverIndex++));
	currentReceiverIndex %= receivers.size();
	ns3::Simulator::Schedule(ns3::Time::FromDouble(std::abs(dist(generator)), ns3::Time::Unit::MS), &MySender::Send, this, addedFlow);
	//ns3::Simulator::ScheduleNow(&MySender::Send, this, addedFlow);
	env->AddFlowId(addedFlow.getId());
}
void MySender::SetActiveFlows(unsigned newFlowCount)
{
	// We punish only for decrease in active flows. Not allowing flows to be recreated is not an issue.
	currentFlowGoal = newFlowCount;
	auto flowCount = flowList.size();
	if (flowCount < newFlowCount)
	{
		for (unsigned i = 0; i < newFlowCount - flowCount; ++i)
		{
			// Add flows to the back of the list.
			this->createFlow();
		}
	}
	else if (flowCount > newFlowCount)
	{
		// Flows are started in sequence, we can always cancel the most recently created ones for optimal completion.
		auto startIt = flowList.begin() + newFlowCount; 

		std::vector<unsigned> removedFlows;
		while (startIt != flowList.end())
		{
			// Cancel all the removed flows and remove them from the list. 
			// Record ids to pass on to env
			removedFlows.push_back(startIt->getId());
			++startIt;
		}
		env->HandleFlowCancellation(removedFlows);
		startIt = flowList.begin() + newFlowCount;
		flowList.erase(startIt, flowList.end());
	}
	// if equal we do nothing
}

void MySender::Send(const Flow& flow)
{
	// if application wasn't started yet, start it now.
	if (!active)
	{
		active = true;
		this->StartApplication();
	}
	// if send event being triggered is for a flow that has been cancelled, ignore it. 
	if (std::find(flowList.begin(), flowList.end(), flow) == flowList.end())
		return;
	
	m_destAddr = receivers[flow.getDestination()];
	m_pktSize = std::min(MAX_PACKET_SIZE, std::max(static_cast<unsigned>(std::abs(packetSizeDist->operator()(generator))), MIN_PACKET_SIZE));
  Ptr<Packet> packet = Create<Packet>(m_pktSize);
	// std::cout << "Returning packet size = " << m_pktSize << " bytes." << std::endl;

	FlowTag tag;
	tag.setId(flow.getId());
	packet->AddByteTag(tag);

	this->SendPacket(packet);
	env->AddSentPacket(flow.getId(), m_pktSize);
	// Schedule delay = packetsize/throughput
	if (!flow.isCompleted())
	{
		// flow is in bits per second, packet size in bytes.
		ns3::Simulator::Schedule(ns3::Seconds(m_pktSize * 8 / flow.getThroughput()), &MySender::Send, this, flow);
	}
	else
	{
		this->HandleFlowCompletion(flow);
		// this->scheduleCreateFlow();
	}
	
}
// void MySender::scheduleCreateFlow()
// {
// 	flowsToRecreate += 1;
// }
void MySender::HandleFlowCompletion(const Flow& flow)
{
	if (!flow.isCompleted())
		throw std::runtime_error("This can't happen if logic is correct.");
	auto it = flowList.begin();
	// can reach flowList end if action removes one of the 
	while (/*it != flowList.end() && */*it != flow)
	{
		++it;
		if (it == flowList.end()) throw std::runtime_error("Should have prevented this now?");
	} 
	env->AddCompletedFlow(flow.getId(), flow.getValue());
	flowList.erase(it);
}

unsigned MySender::getActiveCount() const
{
	return flowList.size();
}
