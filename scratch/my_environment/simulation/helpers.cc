#include "mysocket.h"
#include "flowspec.h"
#include "helpers.h"
#include "../node/sendapplication.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <tuple>

bool isPerformingWell (const SendApplication& app)
{
	// 1 packet tolerance for in-transit things. For now, we assume latency higher than that = bad.
	// This isn't scalable/true, necessarily, but okay for now.

	// We can probably do the math for this - determine how much this offset is supposed to be.
	// Should probably pass reference to simulationenvironment as well in that case, simpler than each parameter separately.
	if (app.getRecvCount() < app.getSentCount() - 1)
		return false;
	// If it's the same or -1, we have gotten (almost) all packets. So doing good:
	return true;

	// Could simply return the inequality - but compiler should optimize.
}
bool isPerformingBad (const SendApplication& app)
{
	// This one only gets called if more than one packet has dropped already
	if (app.getRecvSize() * 1.0 / app.getSentSize() > (1 - app.getThreshold()))
		return true; // If not dropping more than threshold (in percentage) packets, we're doing okay-ish. This parameter is set in flowspec input.
	return false; // If we do drop more than that, performance is bad.

	// Could simply return the inequality - but compiler should optimize.
}

std::tuple<unsigned, unsigned, unsigned> checkApplicationPerformance(const std::vector<SendApplication>& applications)
{
	unsigned well = 0, okay = 0, bad = 0;
	for (const auto& application : applications)
	{
		if (isPerformingWell(application))
			++well;
		else if (isPerformingBad(application))
			++bad;
		else
			++okay;
	}
	return {well, okay, bad};
}
void socketReceive(ns3::Ptr<ns3::Socket> sockPtr)
{
	((MySocket*)(ns3::PeekPointer(sockPtr)))->receive(((MySocket*)(ns3::PeekPointer(sockPtr)))->get()->Recv(std::numeric_limits<uint32_t>::max(), 0));
}

FlowSpec readFlowSpec(const std::string& file)
{
	std::ifstream in (file);
	std::stringstream ss;
	ss << in.rdbuf();
	nlohmann::json JSON = nlohmann::json::parse(ss.str());
	return FlowSpec {
		JSON["goal_type"],
		JSON["point_value"].get<unsigned>(),
		JSON["hold_period"].get<unsigned>(),
		// JSON["requirements"]["max_latency_s"],
		JSON["requirements"]["min_throughput_bps"].get<double>()
		// JSON["requirements"]["max_jitter_s"],
		// JSON["requirements"]["max_loss_percentage"]
	};
}