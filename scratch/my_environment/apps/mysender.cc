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
: active(false), receivers(std::move(addresses)), currentReceiverIndex(0), flowList(), flowspec(readFlowSpec("scratch/my_environment/input/flow.json")), env(std::move(ptr)), currentFlowGoal(0)
{ 
	this->m_node = node;
	this->m_pktSize = 576;
	//this->AggregateObject(CreateObject<ns3::UdpSocketFactoryImpl>());
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

	ns3::Simulator::ScheduleNow(&MySender::Send, this, addedFlow);
	env->AddFlowId(addedFlow.getId());
}
void MySender::SetActiveFlows(unsigned newFlowCount)
{
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
		auto toCancel = flowCount - newFlowCount;
		auto startIt = flowList.begin() + toCancel; 
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
  Ptr<Packet> packet = Create<Packet>(m_pktSize);

	FlowTag tag;
	tag.setId(flow.getId());
	packet->AddByteTag(tag);

	this->SendPacket(packet);
	env->AddSentPacket(flow.getId());
	// Schedule delay = packetsize/throughput
	if (!flow.isCompleted())
	{
		ns3::Simulator::Schedule(ns3::Seconds(m_pktSize / flow.getThroughput()), &MySender::Send, this, flow);
	}
	else
	{
		this->HandleFlowCompletion(flow);
		if (flowList.size() < currentFlowGoal)
		{
			this->createFlow();
		}
	}
	
}

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