#include "flow.h"
#include "ns3/simulator.h"
#include "../include/json.hpp"
namespace
{

auto handleFlowCreation(std::vector<Flow>& flows, const json& category) -> void
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
}

Flow::Flow(const std::string& str, unsigned index)
{
	auto input = json::parse(str);
	goal_type = input["goal_type"];
	point_value = input["point_value"];
	hold_period = input["hold_period"].get<double>() * 1000; // express in ms
	flow_uid = index;
	requirements = Requirement(input["requirements"]["max_latency_s"].get<double>(), input["requirements"]["min_throughput_bps"].get<double>(),
														input["requirements"]["max_jitter_s"].get<double>(), input["requirements"]["max_loss_percentage"].get<double>()
								);
}

Flow::Flow(const json& input, unsigned index)
	: goal_type{input["goal_type"]},
		point_value{input["point_value"]},
		hold_period{input["hold_period"].get<double>() * 1000}, // express in ms
		flow_uid{index},
		requirements{Requirement(
			input["requirements"]["max_latency_s"].get<double>(), input["requirements"]["min_throughput_bps"].get<double>(),
			input["requirements"]["max_jitter_s"].get<double>(), input["requirements"]["max_loss_percentage"].get<double>()
		)}
{ }

Flow::Flow(std::string&& goaltype, double reward, unsigned id, double period, double latency, double throughput, double jitter, double loss)
	: goal_type(std::move(goaltype)), point_value(reward), hold_period(period), flow_uid(id), requirements{latency, throughput, jitter, loss}
{ }

Flow::Flow(Flow&& o) : goal_type(std::move(o.goal_type)), point_value(std::move(o.point_value)), hold_period(std::move(o.hold_period)), flow_uid(std::move(o.flow_uid)), requirements(std::move(o.requirements)) { };
Flow::Flow(const Flow& o) : goal_type(o.goal_type), point_value(o.point_value), hold_period(o.hold_period), flow_uid(o.flow_uid), requirements(o.requirements) { };

bool Flow::isCompleted()
{
	this->state.completed = this->state.completed || (this->state.sent >= this->requirements.min_throughput_bps * this->hold_period * 1000
																										&& this->state.period >= this->hold_period);
	return this->state.completed;
}

void Flow::addSentPacket(std::uint64_t size)
{
	if (!isCompleted())
	{
		std::uint64_t currentTimestampMs = ns3::Simulator::Now().GetMilliSeconds();
		this->state.sent += size;
		this->state.period = (currentTimestampMs - this->state.lastReset);
		//std::cout << "DATA: " << state.sent << ", " << state.period << " --> After packet send, before reset" << std::endl;
		if (this->state.sent / (this->state.period / 1000.0) < this->requirements.min_throughput_bps)
		{
			this->state.sent = 0;
			this->state.period = 0;
			this->state.lastReset = currentTimestampMs;
		}
		//std::cout << "DATA: " << state.sent << ", " << state.period << " --> After reset" << std::endl;
	}
}
bool Flow::noLoss()
{
	return this->state.lostPackets == 0;
}
bool Flow::smallLoss()
{
	return (this->state.lostPackets / this->state.sent) <= this->requirements.max_loss_percentage;
} 
bool Flow::muchLoss()
{
	return (this->state.lostPackets / this->state.sent) > this->requirements.max_loss_percentage;
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