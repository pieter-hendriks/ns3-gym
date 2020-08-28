#pragma once
#ifndef INC_MY_RECEIVER_H_
#define INC_MY_RECEIVER_H_
#include "ns3/ptr.h"
#include "apps.h"
class SimulationEnvironment;
class MyReceiver : public Receiver 
{
	public:
		MyReceiver();
		MyReceiver(ns3::Ptr<SimulationEnvironment> ptr, ns3::Ptr<Node> node);
		virtual ~MyReceiver();
		virtual void Receive (Ptr<Socket> socket);
	private: 
		bool active;
		ns3::Ptr<SimulationEnvironment> env;
};
#endif