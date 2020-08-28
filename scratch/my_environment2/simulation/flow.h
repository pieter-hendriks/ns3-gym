#pragma once
#ifndef INC_FLOW_SPEC_H_
#define INC_FLOW_SPEC_H_
#include "../include/json.hpp"

#include <string>
#include <random>

#include "ns3/simulator.h"

struct FlowSpec
{
	// Contains the spec information for a flow to follow.
	FlowSpec(std::string t, double v, double bps, double periodMean, double periodSD, double FRDP, double SRDP, double SRVP, double BRVP, double CRVP);
	FlowSpec(const FlowSpec&);
	FlowSpec& operator=(const FlowSpec&);
	
	ns3::Time getPeriod() const;
	unsigned id;
	std::string type;
	double value;
	double minThroughput_bps;
	double fullRewardDropPercentage;
	double smallRewardDropPercentage;
	double smallRewardValuePercentage;
	double badRewardValuePercentage;
	double cancelRewardValuePercentage;
	
	bool operator==(const FlowSpec& o) const;
	bool operator!=(const FlowSpec& o) const;

	private:
	std::unique_ptr<std::normal_distribution<double>> periodDistribution;
	static std::default_random_engine generator;

};
std::vector<FlowSpec> readFlowsInput(const std::string& file);
FlowSpec readFlowSpec(const nlohmann::json& file);

class Flow {
	public:
		Flow(const FlowSpec* spec, unsigned dest);
		unsigned getDestination() const;
		float getProgress() const;
		double getThroughput() const;
		bool isCompleted() const;
		double getValue() const;
		unsigned getId() const;
		const FlowSpec& getSpec() const;

		bool operator==(const Flow& o) const;
		bool operator!=(const Flow& o) const;
	private:
		// Non-owning pointer
		const FlowSpec* spec; 
		unsigned id;
		unsigned destination;
		ns3::Time period;
		ns3::Time creationTime;
};	

#endif