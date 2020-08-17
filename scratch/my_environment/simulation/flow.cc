#include "flow.h"
#include <ns3/simulator.h>
#include <fstream>
#include <iostream>
#include <sstream>

Flow::Flow(const FlowSpec& fs, unsigned dest) : spec(fs), destination(dest) {
	static unsigned _id;
	id = _id++;
	creationTime = ns3::Simulator::Now();
}
unsigned Flow::getDestination() const {
	return destination;
}

float Flow::getProgress() const {
	auto lifetime = ns3::Simulator::Now().GetMilliSeconds() - creationTime.GetMilliSeconds();
	auto progress = lifetime / (1.0 * spec.period.GetMilliSeconds());
	return (progress > 1) ? 1 : progress;
}

bool Flow::isCompleted() const {
	return static_cast<unsigned>(this->getProgress()) == 1;
}

double Flow::getThroughput() const {
	return spec.minThroughput_bps;
}

unsigned Flow::getValue() const {
	return spec.value;
}

bool Flow::operator==(const Flow& o) const{
	return id == o.id;
}
bool Flow::operator!=(const Flow& o) const {
	return id != o.id;
}

FlowSpec readFlowSpec(const std::string& file)
{
	std::ifstream in (file);
	std::stringstream ss;
	ss << in.rdbuf();
	nlohmann::json JSON = nlohmann::json::parse(ss.str());
	ns3::Time period = ns3::Time::FromInteger(JSON["hold_period"].get<unsigned>(), ns3::Time::Unit::S);
	return FlowSpec {
		JSON["goal_type"],
		JSON["point_value"].get<unsigned>(),
		period,
		// JSON["requirements"]["max_latency_s"],
		JSON["requirements"]["min_throughput_bps"].get<double>()
		// JSON["requirements"]["max_jitter_s"],
		// JSON["requirements"]["max_loss_percentage"]
	};
}