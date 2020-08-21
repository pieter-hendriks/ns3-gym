/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Piotr Gawlowicz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Piotr Gawlowicz <gawlowicz.p@gmail.com>
 *
 */

#include "simulation/simulationenvironment.h"


#include "ns3/core-module.h"
#include "ns3/opengym-module.h"

#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/timer.h"

#include <ctime>
#include <cstdlib>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OpenGym");

int
main (int argc, char *argv[])
{
	// Seed std::rand
	std::srand(std::time(nullptr));

	// Parameters of the scenario
	uint32_t simSeed = 1;
	double simulationTime = 600;//600; //seconds
	double envStepTime = 5; //seconds, ns3gym env step time interval
	uint32_t openGymPort = 5555;
	uint32_t testArg = 0;

	CommandLine cmd;
	// required parameters for OpenGym interface
	cmd.AddValue ("openGymPort", "Port number for OpenGym env. Default: 5555", openGymPort);
	cmd.AddValue ("simSeed", "Seed for random generator. Default: 1", simSeed);
	// optional parameters
	cmd.AddValue ("simTime", "Simulation time in seconds. Default: 10s", simulationTime);
	cmd.AddValue ("stepTime", "Gym Env step time in seconds. Default: 0.1s", envStepTime);
	cmd.AddValue ("testArg", "Extra simulation argument. Default: 0", testArg);
	cmd.Parse (argc, argv);

	NS_LOG_UNCOND("Ns3Env parameters:");
	NS_LOG_UNCOND("--simulationTime: " << simulationTime);
	NS_LOG_UNCOND("--openGymPort: " << openGymPort);
	NS_LOG_UNCOND("--envStepTime: " << envStepTime);
	NS_LOG_UNCOND("--seed: " << simSeed);
	NS_LOG_UNCOND("--testArg: " << testArg);

	RngSeedManager::SetSeed (std::time(nullptr));

	std::cout << "Starting simulation:\n\tstepTime = " << envStepTime << "\n\tsimTime = " << simulationTime << "\n\tPort = " << openGymPort << std::endl;
	// OpenGym Env
	Ptr<OpenGymInterface> openGymInterface = CreateObject<OpenGymInterface> (openGymPort);
	Ptr<SimulationEnvironment> myGym = CreateObject<SimulationEnvironment>(envStepTime);
	myGym->SetOpenGymInterface(openGymInterface);
	NS_LOG_UNCOND ("Simulation start");
	//std::function<void(MyGymEnv&, MyNode&, MyReceiverNode&)> check = &checkFunction;
	Time::SetResolution(ns3::Time::Unit::US);
	//Simulator::ScheduleNow(check);
	//Simulator::ScheduleNow(&SimulationEnvironment::setupDefaultEnvironment, &(*myGym));
	Simulator::Schedule(ns3::Time::FromInteger(100, ns3::Time::Unit::MS), &SimulationEnvironment::setupDefaultEnvironment, &(*myGym));

	Simulator::Stop (Seconds (simulationTime));
	Simulator::Run ();
	NS_LOG_UNCOND ("Simulation stop");

	openGymInterface->NotifySimulationEnd();
	Simulator::Destroy ();

}


