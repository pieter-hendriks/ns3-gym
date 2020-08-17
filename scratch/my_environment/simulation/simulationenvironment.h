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

class SimulationEnvironment : public ns3::OpenGymEnv
{
public:

friend class TypeId;
	SimulationEnvironment() { throw std::runtime_error("Needed for ns3 stuff, but shouldn't ever be called."); };
	SimulationEnvironment(unsigned inter);
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
	
	void AddScore(unsigned s);
	void AddSentPacket();
	void AddReceivedPacket();

private:
	void setupDefaultEnvironment();
	void readFlowSpec();
	unsigned interval;
	uint64_t nextFlowId;
	unsigned score, sent, recv;
	ns3::Ptr<MySender> sendApplication;
};

#endif