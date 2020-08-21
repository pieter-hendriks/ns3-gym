#pragma once
#ifndef INC_SIMULATION_ENVIRONMENT_H_
#define INC_SIMULATION_ENVIRONMENT_H_

// #include "../node/receivenode.h"
// #include "../node/sendnode.h"
// #include "../node/sendapplication.h"
#include "../apps/mysender.h"
#include "ns3/type-id.h"
#include "ns3/opengym_env.h"
#include "ns3/ptr.h"

#include "flow.h"

#include <map>

class SimulationEnvironment : public ns3::OpenGymEnv
{
public:

friend class TypeId;
	SimulationEnvironment() { throw std::runtime_error("Needed for ns3 stuff, but shouldn't ever be called."); };
	SimulationEnvironment(double inter);
	SimulationEnvironment(SimulationEnvironment&) = delete;
	SimulationEnvironment& operator=(SimulationEnvironment&) = delete;

	virtual ~SimulationEnvironment() = default;
	static ns3::TypeId GetTypeId();


	ns3::Ptr<ns3::OpenGymSpace> GetActionSpace();
	bool ExecuteActions(ns3::Ptr<ns3::OpenGymDataContainer> action);
	ns3::Ptr<ns3::OpenGymSpace> GetObservationSpace();
	ns3::Ptr<ns3::OpenGymDataContainer> GetObservation();
	bool GetGameOver();
	float GetReward();
	std::string GetExtraInfo();

	void StateRead();
	
	void AddCompletedFlow(unsigned id, unsigned s);
	void HandleFlowCancellation(std::vector<unsigned>& flows);

	// AddFlowId adds the flow for env to keep track of, 
	void AddFlowId(unsigned id);
	// These two increment the counters.
	void AddSentPacket(unsigned flowId, unsigned packetSize);
	void AddReceivedPacket(unsigned flowId);

	void setupDefaultEnvironment();
private:
	void CreateApplications(ns3::Ptr<ns3::NetDevice> noiseDevice);
	void readFlowSpec();
	void handleCancelledFlows();
	double interval;
	uint64_t nextFlowId;
	int64_t score, sent, recv, sentSize;
	std::map<unsigned, unsigned> sentPacketMap;
	std::map<unsigned, unsigned> recvPacketMap;
	std::vector<unsigned> completedFlows, cancelledFlows;
	ns3::Ptr<MySender> sendApplication;
	NodeContainer nodes;
	ns3::Ptr<Node> noiseNode;
};

#endif