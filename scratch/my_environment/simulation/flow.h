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
	FlowSpec(std::string t, uint32_t v, double bps, double periodMean, double periodSD);
	FlowSpec(const FlowSpec&);
	FlowSpec& operator=(const FlowSpec&);
	std::string type;
	uint32_t value;
	ns3::Time getPeriod() const;
	// double maxLatency_s;
	double minThroughput_bps;
	// double maxJitter_s;
	// double maxLoss_pct;
	private:
	std::unique_ptr<std::normal_distribution<double>> periodDistribution;
	static std::default_random_engine generator;

};
FlowSpec readFlowSpec(const std::string& file);

class Flow {
	public:
		Flow(const FlowSpec& spec, unsigned dest);
		unsigned getDestination() const;
		float getProgress() const;
		double getThroughput() const;
		bool isCompleted() const;
		unsigned getValue() const;
		unsigned getId() const;
		const FlowSpec& getSpec() const;

		bool operator==(const Flow& o) const;
		bool operator!=(const Flow& o) const;
	private:
		FlowSpec spec; 
		unsigned id;
		unsigned destination;
		ns3::Time period;
		ns3::Time creationTime;
};	

#endif