#pragma once
#ifndef INC_MY_SENDER_H_
#define INC_MY_SENDER_H_
#include "apps.h"

#include "../simulation/flow.h"

#include <deque>
class SimulationEnvironment;
class MySender : public Sender
{
	public:
		//TypeId GetTypeId (void);
		MySender();
		MySender(ns3::Ptr<SimulationEnvironment> ptr, const std::vector<Ipv4Address>& addresses);
		virtual ~MySender();
		void SetActiveFlows(unsigned newFlowCount);
		void Send(const Flow& flow);

		unsigned getActiveCount() const;

	private:
		void HandleFlowCompletion(const Flow& flow);


		std::vector<Ipv4Address> receivers;
		unsigned currentFlowIndex;
		std::deque<Flow> flowList;

		FlowSpec flowspec;
		ns3::Ptr<SimulationEnvironment> env;

};
#endif