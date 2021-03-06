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
#include "ns3/lte-enb-net-device.h"
#include "flow.h"
#include "ns3/lte-helper.h"

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
	
	void AddCompletedFlow(unsigned id, const FlowSpec& flowSpec);
	void HandleFlowCancellation(std::vector<unsigned>& flows, const FlowSpec& spec);

	// AddFlowId adds the flow for env to keep track of, 
	void AddFlowId(unsigned id);
	// These two increment the counters.
	void AddSentPacket(unsigned flowId, unsigned packetSize, const FlowSpec& spec);
	void AddReceivedPacket(unsigned flowId, const FlowSpec& spec);

	void setupDefaultEnvironment();
	void Activate();
	void CreateApplications();
private:
	void SetupLTEEnvironment();
	void SetupWifiEnvironment();
	void readFlowSpec();
	void handleCancelledFlows();
	double interval;
	uint64_t nextFlowId;
	std::vector<double> score;
	std::vector<int64_t> sent, recv, sentSize;
	std::map<unsigned, unsigned> sentPacketMap;
	std::map<unsigned, unsigned> recvPacketMap;
	std::vector<unsigned> completedFlows, cancelledFlows;
	ns3::Ptr<MySender> sendApplication;
	NodeContainer nodes;
	ns3::Ptr<Node> noiseNode;
	NetDeviceContainer noiseDevice;
	ns3::Ptr<LteEnbNetDevice> enbDevice;
	ns3::Ptr<LteHelper> lteHelper;
	NodeContainer sendNode;
};

#endif