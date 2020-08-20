#include "mysender.h"
#include "../simulation/simulationenvironment.h"
#include "apps.h"
#include "flowtag.h"

#include "ns3/udp-socket-factory-impl.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/stats-module.h"
#include "ns3/log.h"
#include "ns3/log-macros-enabled.h"

#include <stdexcept>
#include <ostream>
NS_LOG_COMPONENT_DEFINE("MySender");
// TypeId MySender::GetTypeId (void)
// {
// 	static TypeId tid = TypeId("MySender").SetParent<Sender>().AddConstructor<MySender>();
// 	return tid;
// };
MySender::MySender(ns3::Ptr<SimulationEnvironment> ptr, const std::vector<Ipv4Address>& addresses, ns3::Ptr<Node> node) 
: active(false), receivers(std::move(addresses)), currentReceiverIndex(0), flowList(), flowsToRecreate(0), flowspec(readFlowSpec("scratch/my_environment/input/flow.json")), env(std::move(ptr)), currentFlowGoal(0)
{ 
	this->m_node = node;
	this->m_pktSize = 11520 /* 576 * 20 */; // Was originally 576, increased for performance reasons. Probably not a great solution, but problem appears to be in simulator.
};
MySender::~MySender() 
{
	NS_LOG_FUNCTION_NOARGS ();
	if (active)	
		this->StopApplication();
}
void MySender::createFlow()
{
	auto& addedFlow = flowList.emplace_back(Flow(flowspec, currentReceiverIndex++));
	currentReceiverIndex %= receivers.size();
	ns3::Simulator::Schedule(ns3::Time::FromInteger(2, ns3::Time::Unit::MS), &MySender::Send, this, addedFlow);
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
			removedFlows.push_back(startIt->getId());
			++startIt;
		}
		env->HandleFlowCancellation(removedFlows);

		startIt = flowList.begin() + newFlowCount;
		flowList.erase(startIt, flowList.end());
		std::cout << "New size: " << flowList.size() << "\nExpected Size: " << newFlowCount << std::endl;
		if (flowList.size() != newFlowCount) throw std::runtime_error("fuk");
	}
	// if equal we do nothing
}

void MySender::Send(const Flow& flow)
{
	std::cout << "Sending" << std::endl;
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
  Ptr<Packet> packet = Create<Packet>(m_pktSize);

	FlowTag tag;
	tag.setId(flow.getId());
	packet->AddByteTag(tag);

	this->SendPacket(packet);
	env->AddSentPacket(flow.getId());
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