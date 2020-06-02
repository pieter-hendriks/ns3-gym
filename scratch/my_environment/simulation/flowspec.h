#pragma once
#ifndef INC_FLOW_SPEC_H_
#define INC_FLOW_SPEC_H_
#include <string>
#include "../include/json.hpp"

struct FlowSpec
{
	// Contains the spec information for a flow to follow.
	std::string type;
	uint32_t value;
	uint32_t period;
	// double maxLatency_s;
	double minThroughput_bps;
	// double maxJitter_s;
	// double maxLoss_pct;
};
FlowSpec readFlowSpec(const std::string& file);

#endif