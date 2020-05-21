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
	std::uint64_t lostPackets;
};

struct Flow
{
	Flow() = default;
	Flow(const std::string& str, unsigned index);
	Flow(const json& input, unsigned index);
	Flow(std::string&& goaltype, double reward, unsigned id, double period, double latency, double throughput, double jitter, double loss);
	Flow(Flow&& o);
	Flow(const Flow& o);
	virtual ~Flow()  = default;
	std::string goal_type;
	double point_value;
	double hold_period;
	std::uint64_t flow_uid;
	Requirement requirements;
	FlowState state;

	bool isCompleted();
	void addSentPacket(std::uint64_t size);
	bool noLoss();
	bool smallLoss();
	bool muchLoss();
};


auto getFlows(const std::string& str) -> std::vector<Flow>;

#endif