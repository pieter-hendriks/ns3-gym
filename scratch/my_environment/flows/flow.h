#pragma once
#ifndef INC_MYENV_FLOWS_FLOW_H_
#define INC_MYENV_FLOWS_FLOW_H_
#include <string>
#include <cstdint>
#include "../include/json.hpp"
#include <iostream>

#include "ns3/simulator.h"

using json = nlohmann::json;
struct Requirement
{
	Requirement() = default;
	Requirement(double lat, double through, double jitter, double loss) : max_latency_s(lat), min_throughput_bps(through), max_jitter_s(jitter), max_loss_percentage(loss) { };
	Requirement(const Requirement& o)  : max_latency_s(o.max_latency_s), min_throughput_bps(o.min_throughput_bps), max_jitter_s(o.max_jitter_s), max_loss_percentage(o.max_loss_percentage) { };
	Requirement(Requirement&& o) : max_latency_s(o.max_latency_s), min_throughput_bps(o.min_throughput_bps), max_jitter_s(o.max_jitter_s), max_loss_percentage(o.max_loss_percentage) { };
	Requirement& operator=(const Requirement& o) { max_latency_s = o.max_latency_s; min_throughput_bps = o.min_throughput_bps; max_jitter_s = o.max_jitter_s; max_loss_percentage = o.max_loss_percentage; return *this; };
	double max_latency_s;
	double min_throughput_bps;
	double max_jitter_s;
	double max_loss_percentage;
};

struct FlowState
{
	FlowState() : period(0), sent(0), lastReset(0), completed(false) { };
	double period;
	std::uint64_t sent;
	std::uint64_t lastReset;
	bool completed;
};

struct Flow
{
	Flow() = default;
	Flow(const std::string& str, unsigned index)
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
	Flow(const json& input, unsigned index)
		: goal_type{input["goal_type"]},
			point_value{input["point_value"]},
			hold_period{input["hold_period"].get<double>() * 1000}, // express in ms
			flow_uid{index},
			requirements{Requirement(
				input["requirements"]["max_latency_s"].get<double>(), input["requirements"]["min_throughput_bps"].get<double>(),
				input["requirements"]["max_jitter_s"].get<double>(), input["requirements"]["max_loss_percentage"].get<double>()
			)}
	{ }

	Flow(std::string&& goaltype, double reward, unsigned id, double period, double latency, double throughput, double jitter, double loss)
	: goal_type(std::move(goaltype)), point_value(reward), hold_period(period), flow_uid(id), requirements{latency, throughput, jitter, loss}
	{

	};

	Flow(Flow&& o) : goal_type(std::move(o.goal_type)), point_value(std::move(o.point_value)), hold_period(std::move(o.hold_period)), flow_uid(std::move(o.flow_uid)), requirements(std::move(o.requirements)) { };
	Flow(const Flow& o) : goal_type(o.goal_type), point_value(o.point_value), hold_period(o.hold_period), flow_uid(o.flow_uid), requirements(o.requirements) { };
	virtual ~Flow()  {};
	std::string goal_type;
	double point_value;
	double hold_period;
	std::uint64_t flow_uid;
	Requirement requirements;

	FlowState state;
	bool isCompleted()
	{
		this->state.completed = this->state.completed || (this->state.sent >= this->requirements.min_throughput_bps * this->hold_period * 1000
																											&& this->state.period >= this->hold_period);
		return this->state.completed;
	}
	void addSentPacket(std::uint64_t size)
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
	double getCurrentSentFraction()
	{
		return this->state.sent / (this->requirements.min_throughput_bps * this->hold_period / 1000.0);
	}
	double getCurrentPeriodFraction()
	{
		return this->state.period / (1.0 * this->hold_period);
	}
};


auto getFlows(const std::string& str) -> std::vector<Flow>;

#endif