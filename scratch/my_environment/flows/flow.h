#pragma once
#ifndef INC_MYENV_FLOWS_FLOW_H_
#define INC_MYENV_FLOWS_FLOW_H_
#include <string>
#include <cstdint>
#include "../include/json.hpp"
using json = nlohmann::json;
// Flow input structure: 
// {"goal_type": "VOIP", "point_value": 4, "goal_set": "Voice", "hold_period": 10, "flow_uid": 5027, "requirements": {"max_latency_s": 0.37, "min_throughput_bps": 36504.0}}
struct Requirement
{
	Requirement() = default;
	Requirement(double lat, double through) : max_latency_s(lat), min_throughput_bps(through) { }
	double max_latency_s;
	double min_throughput_bps;
};

struct Flow
{
	Flow() = default;
	Flow(const std::string& str)
	{
		json input = str;
		goal_type = input["goal_type"];
		point_value = input["point_value"];
		goal_set = input["goal_set"];
		hold_period = input["hold_period"];
		flow_uid = input["flow_uid"];
		requirements = Requirement(input["requirements"]["max_latency_s"], input["requirements"]["min_throughput_bps"]);
	}
	Flow(const json& input)
	  : goal_type{input["goal_type"]},
			point_value{input["point_value"]},
			goal_set{input["goal_set"]},
			hold_period{input["hold_period"]},
			flow_uid{input["flow_uid"]},
			requirements{Requirement(input["requirements"]["max_latency_s"], input["requirements"]["min_throughput_bps"])}
	{ }

	std::string goal_type;
	double point_value;
	std::string goal_set;
	double hold_period;
	std::uint64_t flow_uid;
	Requirement requirements;
};

struct FlowState
{
	FlowState(std::uint64_t ID, double p = 0.0) : uid{ID}, period{p} { };
	std::uint64_t uid;
	double period;
};


auto getFlows(const std::string& str) -> std::vector<Flow>
{
	std::vector<Flow> flows;
	json input = str;
	for (const auto& flow : input)
	{
		flows.emplace_back(flow);
	}
	return std::move(flows);
}
auto getFlowStates (const std::vector<Flow>& flows) -> std::vector<FlowState>
{
	std::vector<FlowState> states;
	for(const auto& flow : flows)
	{
		states.emplace_back(flow.flow_uid, 0.0);
	}
	return std::move(states);
}

#endif