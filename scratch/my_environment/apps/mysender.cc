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
#include <cstdlib>
#include <algorithm> 
NS_LOG_COMPONENT_DEFINE("MySender");

#define MAX_PACKET_SIZE 1500U
#define MIN_PACKET_SIZE 100U
// TypeId MySender::GetTypeId (void)
// {
// 	static TypeId tid = TypeId("MySender").SetParent<Sender>().AddConstructor<MySender>();
// 	return tid;
// };
MySender::MySender(ns3::Ptr<SimulationEnvironment> ptr, const std::vector<Ipv4Address>& addresses, ns3::Ptr<Node> node, double pkSzMean, double pkSzSD, std::vector<int> flowGoal) 
: active(false), receivers(std::move(addresses)), currentReceiverIndex(0), flowSpecs(readFlowsInput("scratch/my_environment/input/flow.json")), flowList(flowSpecs.size()), 
 env(std::move(ptr)), currentFlowGoal(std::move(flowGoal)), generator(std::time(nullptr)), packetSizeDist(std::make_unique<std::normal_distribution<>>(pkSzMean, pkSzSD))
	
{ 
	NS_ASSERT(currentFlowGoal.size() == flowSpecs.size());
	this->m_node = node;
	this->m_pktSize = 576; 
	for (auto i = 0u; i < flowSpecs.size(); ++i)
	{
		while (static_cast<unsigned>(currentFlowGoal[i]) > flowList[i].size())
			createFlow(i);
	}
}
MySender::~MySender() 
{
	NS_LOG_FUNCTION_NOARGS ();
	if (active)	
		this->StopApplication();
}
void MySender::createFlow(unsigned index)
{
	static std::normal_distribution<double> dist(0, 100);
	auto& addedFlow = flowList[index].emplace_back(Flow(&flowSpecs[index], currentReceiverIndex++));
	currentReceiverIndex %= receivers.size();
	ns3::Simulator::Schedule(ns3::Time::FromDouble(std::abs(dist(generator)), ns3::Time::Unit::MS), &MySender::Send, this, addedFlow);
	//ns3::Simulator::ScheduleNow(&MySender::Send, this, addedFlow);
	env->AddFlowId(addedFlow.getId());
}
void MySender::incrementActiveFlows(unsigned index, int32_t flowIncrement)
{
	// We punish only for decrease in active flows. Not allowing flows to be recreated is not an issue.
	currentFlowGoal[index] = std::max(0, flowIncrement + currentFlowGoal[index]);
	while (flowList[index].size() < static_cast<unsigned>(currentFlowGoal[index]))
			this->createFlow(index);	
	// if before the while would be redundant
	// else ->
	if (flowList[index].size() > static_cast<unsigned>(currentFlowGoal[index]))
	{
		// Flows are started in sequence, we can always cancel the most recently created ones for optimal completion.
		auto startIt = flowList[index].begin() + currentFlowGoal[index]; 

		std::vector<unsigned> removedFlows;
		while (startIt != flowList[index].end())
		{
			// Record ids to pass on to env
			removedFlows.push_back(startIt->getId());
			++startIt;
		}
		// Then pass cancelled flows to environment and locally remove them from application.
		env->HandleFlowCancellation(removedFlows);
		startIt = flowList[index].begin() + currentFlowGoal[index];
		flowList[index].erase(startIt, flowList[index].end());
	}
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
	if (std::find(flowList[flow.getSpec().id].begin(), flowList[flow.getSpec().id].end(), flow) == flowList[flow.getSpec().id].end())
		return;
	
	m_destAddr = receivers[flow.getDestination()];
	m_pktSize = std::min(MAX_PACKET_SIZE, std::max(static_cast<unsigned>(std::abs(packetSizeDist->operator()(generator))), MIN_PACKET_SIZE));
  Ptr<Packet> packet = Create<Packet>(m_pktSize);
	// std::cout << "Returning packet size = " << m_pktSize << " bytes." << std::endl;

	FlowTag tag;
	tag.setId(flow.getId());
	tag.setFlowSpec(flow.getSpec());
	packet->AddByteTag(tag);

	this->SendPacket(packet);
	env->AddSentPacket(flow.getId(), m_pktSize, flow.getSpec());
	// Schedule delay = packetsize/throughput
	if (!flow.isCompleted())
	{
		// flow is in bits per second, packet size in bytes.
		ns3::Simulator::Schedule(ns3::Seconds(m_pktSize * 8 / flow.getThroughput()), &MySender::Send, this, flow);
	}
	else
	{
		this->HandleFlowCompletion(flow);
	}
	
}
void MySender::HandleFlowCompletion(const Flow& flow)
{
	auto index = 0u;
	while (flow.getSpec() != flowSpecs[index])
	{
		++index;
		if (index > flowSpecs.size())
			throw std::runtime_error("This should be impossible.");
	}
	if (!flow.isCompleted())
		throw std::runtime_error("This can't happen if logic is correct.");
	auto it = flowList[index].begin();
	// can reach flowList end if action removes one of the 
	while (*it != flow)
	{
		++it;
		if (it == flowList[index].end()) throw std::runtime_error("Should have prevented this now?");
	} 
	env->AddCompletedFlow(flow.getId(), flow.getSpec());
	flowList[index].erase(it);
}

unsigned MySender::getActiveCount(unsigned index) const
{
	return flowList[index].size();
}
unsigned MySender::getActiveGoal(unsigned index) const
{
	return currentFlowGoal[index];
}
