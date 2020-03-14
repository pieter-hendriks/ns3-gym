#include "flow.h"
#include "ns3/simulator.h"
#include "../include/json.hpp"

auto handleFlowCreation(std::vector<Flow>& flows, const json& category)
{
	static auto flowCountSoFar = 0ULL;
	auto count = category["flow_count"].get<unsigned>();
	flows.reserve(count);
	for (unsigned i = flowCountSoFar; i < flowCountSoFar + count; ++i)
	{
		flows.emplace_back(
			category, i // Category has actual flows specs that'll get duplicated for each flow, i provides a unique ID to identify flows.
		);
	}
	flowCountSoFar += count;
}

auto getFlows(const std::string& str) -> std::vector<Flow>
{
	std::vector<Flow> flows;
	json input = json::parse(str);
	for (const auto& flowCategory : input)
	{
		handleFlowCreation(flows, flowCategory);
	}
	return flows;
}