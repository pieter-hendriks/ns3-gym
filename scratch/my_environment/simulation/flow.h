#pragma once
#ifndef INC_FLOW_SPEC_H_
#define INC_FLOW_SPEC_H_
#include <string>
#include "../include/json.hpp"

#include "ns3/simulator.h"

struct FlowSpec
{
	// Contains the spec information for a flow to follow.
	std::string type;
	uint32_t value;
	ns3::Time period;
	// double maxLatency_s;
	double minThroughput_bps;
	// double maxJitter_s;
	// double maxLoss_pct;
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

		bool operator==(const Flow& o) const;
		bool operator!=(const Flow& o) const;
	private:
		FlowSpec spec; 
		unsigned id;
		unsigned destination;
		ns3::Time creationTime;
};	

#endif