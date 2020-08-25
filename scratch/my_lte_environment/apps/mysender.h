#pragma once
#ifndef INC_MY_SENDER_H_
#define INC_MY_SENDER_H_
#include "apps.h"

#include "../simulation/flow.h"
#include "ns3/lte-enb-net-device.h"
#include <deque>
#include <random>
#include <memory>
class SimulationEnvironment;
class MySender : public Sender
{
	public:
		//TypeId GetTypeId (void);
		MySender() = delete;
		MySender(ns3::Ptr<SimulationEnvironment> ptr, const std::vector<Ipv4Address>& addresses, ns3::Ptr<Node> node, double pkSzMean, double pkSzSD, std::vector<int> flowGoal);
		virtual ~MySender();
		void incrementActiveFlows(unsigned index, int32_t flowIncrement);
		void Send(const Flow& flow);

		unsigned getActiveCount(unsigned index) const;
		unsigned getActiveGoal(unsigned index) const;

		void scheduleFlowRecreation();

		const std::vector<FlowSpec>& getFlowSpecs() const;
	private:
		void HandleFlowCompletion(const Flow& flow);
		void createFlow(unsigned index);
		bool active;
		std::vector<Ipv4Address> receivers;
		unsigned currentReceiverIndex;
		std::vector<FlowSpec> flowSpecs;
		std::vector<std::deque<Flow>> flowList;
		ns3::Ptr<SimulationEnvironment> env;
		std::vector<int> currentFlowGoal;
		std::default_random_engine generator;
		std::unique_ptr<std::normal_distribution<double>> packetSizeDist;


};
#endif