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
		MyReceiver(ns3::Ptr<SimulationEnvironment> ptr);
		virtual ~MyReceiver();
		virtual void Receive (Ptr<Socket> socket);
	private: 
		ns3::Ptr<SimulationEnvironment> env;
};
#endif