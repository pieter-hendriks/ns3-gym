#pragma once
#ifndef INC_MYENV_FLOWGENERATOR_H_
#define INC_MYENV_FLOWGENERATOR_H_
#include "ns3/application.h"
#include "ns3/random-variable-stream.h"
#include "ns3/double.h"
#include "ns3/socket.h"
#include "ns3/type-id.h"
#include "ns3/address.h"
#include "ns3/simulator.h"
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "flow.h"
#include "../mygym.h"
#include <assert.h>
namespace ns3
{
	class MyGymEnv;
}
class FlowGenerator;
using FlowGeneratorContainer = std::vector<FlowGenerator*>;
class FlowGenerator : public ns3::Application
{
public:
	friend class ns3::MyGymEnv;
	FlowGenerator() = delete;
	FlowGenerator(ns3::MyGymEnv* gym, Flow& f, std::uint64_t pSize) : flow{f}, packetSize(pSize), myGym{*gym}, isRunning{false} { };
	void SetRemote (const std::string& socketType, ns3::Address remote)
	{
		ns3::TypeId tid = ns3::TypeId::LookupByName (socketType);
		m_socket = ns3::Socket::CreateSocket (GetNode (), tid);
		m_socket->Bind ();
		m_socket->ShutdownRecv ();
		m_socket->Connect (remote);
	};
	void DoGenerate ()
	{
		ns3::Ptr<ns3::Packet> p = ns3::Create<ns3::Packet> (packetSize);
		myGym.addSentPacket(packetSize, flow);
		m_socket->Send (p);

		if (!flow.isCompleted())
			m_next = ns3::Simulator::Schedule (ns3::Seconds (1 / (flow.requirements.min_throughput_bps / packetSize)), &FlowGenerator::DoGenerate, this);
	};
	static FlowGeneratorContainer Install(ns3::MyGymEnv* gym, std::vector<std::pair<std::reference_wrapper<Flow>, std::uint64_t>>&& flows, ns3::Node& node)
	{
		FlowGeneratorContainer applications;
		for (unsigned i = 0; i < flows.size(); ++i)
		{
			auto application = ns3::CreateObject<FlowGenerator>(gym, std::move(std::get<0>(flows[i])), std::get<1>(flows[i]));
			node.AddApplication(application);
			applications.push_back(&(*application));
		}
		return applications;
	};

	virtual void StartApplication ()
	{
		if (!isRunning)
		{
			m_next = ns3::Simulator::Schedule(ns3::Seconds(0.0), &FlowGenerator::DoGenerate, this);
			isRunning = true;
		}
	};
	virtual void StopApplication ()
	{
		if (isRunning)
		{
			ns3::Simulator::Cancel(m_next);
			isRunning = false;
		}
	};
	Flow& getFlow() {
		return flow;
	}
private:
	Flow& flow;
	std::uint64_t packetSize;
	ns3::Ptr<ns3::Socket> m_socket;
	ns3::EventId m_next;
	ns3::MyGymEnv& myGym;
	bool isRunning;

};



#endif