#pragma once
#ifndef INC_MY_NOISE_MACHINE_H_
#define INC_MY_NOISE_MACHINE_H_
#include "ns3/application.h"
#include "ns3/event-id.h"
class MyNoiseMachine : public ns3::Application
{
	public:
		MyNoiseMachine(ns3::Ptr<ns3::NetDevice> noiseDevice);
		void StartApplication();
		void StopApplication();
		void Send();
	private:
		ns3::Ptr<ns3::NetDevice> dev;
		ns3::EventId event;
};
#endif