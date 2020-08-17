#include "mysender.h"
#include "../simulation/simulationenvironment.h"
#include "apps.h"


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
MySender::MySender() { throw std::runtime_error("Needed for ns3, shouldn't be called"); };
MySender::MySender(ns3::Ptr<SimulationEnvironment> ptr, const std::vector<Ipv4Address>& addresses) 
: receivers(std::move(addresses)), currentFlowIndex(0), flowList(), flowspec(readFlowSpec("scratch/my_environment/input/flow.json")), env(std::move(ptr))  {};
MySender::~MySender() 
{
	NS_LOG_FUNCTION_NOARGS ();
}
void MySender::SetActiveFlows(unsigned newFlowCount)
{
	auto flowCount = flowList.size();
	if (flowCount < newFlowCount)
	{
		for (unsigned i = 0; i < newFlowCount - flowCount; ++i)
		{
			// Add flows to the back of the list.
			auto& addedFlow = flowList.emplace_back(Flow(flowspec, currentFlowIndex++));
			this->Send(addedFlow);
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
	m_destAddr = receivers[flow.getDestination()];
	this->SendPacket();
	env->AddSentPacket();
	// Schedule delay = packetsize/throughput
	if (!flow.isCompleted())
	{
		ns3::Simulator::Schedule(ns3::Seconds(m_pktSize / flow.getThroughput()), &MySender::Send, this, flow);
	}
	else
	{
		this->HandleFlowCompletion(flow);
	}
	
}

void MySender::HandleFlowCompletion(const Flow& flow)
{
	env->AddScore(flow.getValue());
	auto it = flowList.begin(); 
	while (flow != *it) ++it;
	if (it == flowList.end()) throw std::runtime_error("Erased non-existing flow.");
	flowList.erase(it);
}

unsigned MySender::getActiveCount() const
{
	return flowList.size();
}