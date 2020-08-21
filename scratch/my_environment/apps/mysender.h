#pragma once
#ifndef INC_MY_SENDER_H_
#define INC_MY_SENDER_H_
#include "apps.h"

#include "../simulation/flow.h"

#include <deque>
#include <random>
#include <memory>
class SimulationEnvironment;
class MySender : public Sender
{
	public:
		//TypeId GetTypeId (void);
		MySender() = delete;
		MySender(ns3::Ptr<SimulationEnvironment> ptr, const std::vector<Ipv4Address>& addresses, ns3::Ptr<Node> node, double pkSzMean, double pkSzSD, unsigned flowGoal);
		virtual ~MySender();
		void incrementActiveFlows(int32_t flowIncrement);
		void Send(const Flow& flow);

		unsigned getActiveCount() const;

	private:
		void HandleFlowCompletion(const Flow& flow);
		void createFlow();
		void scheduleCreateFlow();
		bool active;
		std::vector<Ipv4Address> receivers;
		unsigned currentReceiverIndex;
		std::deque<Flow> flowList;
		unsigned flowsToRecreate;
		FlowSpec flowspec;
		ns3::Ptr<SimulationEnvironment> env;
		int currentFlowGoal;
		std::default_random_engine generator;
		std::unique_ptr<std::normal_distribution<double>> packetSizeDist;


};
#endif