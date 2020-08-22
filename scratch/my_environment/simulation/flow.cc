#include "flow.h"
#include <ns3/simulator.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <random>
#include <ctime>
#include <iomanip>
std::default_random_engine FlowSpec::generator (std::time(nullptr));
Flow::Flow(const FlowSpec* fs, unsigned dest) : spec(fs), destination(dest)
{
	static unsigned _id;
	id = _id++;
	creationTime = ns3::Simulator::Now();
	period = spec->getPeriod();
	//std::cout << "Activated flow " << id << std::endl;
}
unsigned Flow::getDestination() const {
	return destination;
}
float Flow::getProgress() const {
	auto lifetime = ns3::Simulator::Now().GetMilliSeconds() - creationTime.GetMilliSeconds();
	auto progress = lifetime / (1.0 * period.GetMilliSeconds());
	return (progress > 1) ? 1 : progress;
}
const FlowSpec& Flow::getSpec() const {
	return *spec;
}
bool Flow::isCompleted() const {
	return static_cast<unsigned>(this->getProgress()) == 1;
}

double Flow::getThroughput() const {
	return spec->minThroughput_bps;
}

unsigned Flow::getValue() const {
	return spec->value;
}
unsigned Flow::getId() const {
	return id;
}
bool Flow::operator==(const Flow& o) const{
	return id == o.id;
}
bool Flow::operator!=(const Flow& o) const {
	return id != o.id;
}
FlowSpec::FlowSpec(std::string t, double v, double bps, double periodMean, double periodSD, double FRDP, double SRDP, double SRVP, double BRVP, double CRVP) 
: type(std::move(t)), value(v), minThroughput_bps(bps), fullRewardDropPercentage(FRDP), smallRewardDropPercentage(SRDP), smallRewardValuePercentage(SRVP), 
	badRewardValuePercentage(BRVP), cancelRewardValuePercentage(CRVP), periodDistribution(std::make_unique<std::normal_distribution<double>>(periodMean, periodSD))
{
	static unsigned idCounter = 0;
	id = idCounter++;
	if (this->badRewardValuePercentage < -6 || this->badRewardValuePercentage > 1)
	{
		std::cout << "bad init: " << badRewardValuePercentage << std::endl;
		throw std::runtime_error("U fucking wot mate");
	}
};
FlowSpec::FlowSpec(const FlowSpec& o)
: id(o.id), type(o.type), value(o.value), minThroughput_bps(o.minThroughput_bps), fullRewardDropPercentage(o.fullRewardDropPercentage),
 smallRewardDropPercentage(o.smallRewardDropPercentage), smallRewardValuePercentage(o.smallRewardValuePercentage),
 badRewardValuePercentage(o.badRewardValuePercentage), cancelRewardValuePercentage(o.cancelRewardValuePercentage),
 periodDistribution(std::make_unique<std::normal_distribution<double>>(*o.periodDistribution))  {}

FlowSpec& FlowSpec::operator=(const FlowSpec& o)
{
	id = o.id;
	type = o.type;
	value = o.value;
	minThroughput_bps = o.minThroughput_bps;
	periodDistribution = std::make_unique<std::normal_distribution<double>>(*o.periodDistribution);
	fullRewardDropPercentage = o.fullRewardDropPercentage;
	smallRewardDropPercentage = o.smallRewardDropPercentage;
	smallRewardValuePercentage = o.smallRewardValuePercentage;
	badRewardValuePercentage = o.badRewardValuePercentage;
	return *this;
}
ns3::Time FlowSpec::getPeriod() const
{
	double value = FlowSpec::periodDistribution->operator()(FlowSpec::generator);
	//std::cout << "Returning flow duration = " << std::setw(5) << value << " seconds." << std::endl;
	return ns3::Time::FromDouble(value, ns3::Time::Unit::S);

}
std::vector<FlowSpec> readFlowsInput(const std::string& file)
{
	std::ifstream in (file);
	std::stringstream ss;
	ss << in.rdbuf();
	nlohmann::json JSON = nlohmann::json::parse(ss.str());

	auto QoSJSON = JSON["QoS"];
	auto BEJSON = JSON["BE"];
	return { readFlowSpec(QoSJSON), readFlowSpec(BEJSON) };
}
FlowSpec readFlowSpec(const nlohmann::json& JSON)
{
	unsigned holdTime = JSON["hold_period"].get<unsigned>();
	double periodMean = holdTime;
	double periodSD = holdTime / 10.0;
	return FlowSpec {
		JSON["goal_type"],
		JSON["point_value"].get<double>(),
		JSON["requirements"]["min_throughput_bps"].get<double>(),
		periodMean, periodSD,
		JSON["requirements"]["full_reward_max_drop"].get<double>(),
		JSON["requirements"]["small_reward_max_drop"].get<double>(),
		JSON["requirements"]["small_reward_percentage"].get<double>(),
		JSON["requirements"]["bad_result_percentage"].get<double>(),
		JSON["requirements"]["cancel_result_percentage"].get<double>()
	};
}
bool FlowSpec::operator==(const FlowSpec& o) const 
{
	return o.type == this->type;
}
bool FlowSpec::operator!=(const FlowSpec& o) const
{
	return o.type != this->type;
}