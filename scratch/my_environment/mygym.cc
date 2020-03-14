/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Technische Universität Berlin
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
 * Author: Piotr Gawlowicz <gawlowicz@tkn.tu-berlin.de>
 */

#include "mygym.h"

#include "flows/flow.h"
#include "flows/flowgenerator.h"

#include "ns3/object.h"
#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/node-list.h"
#include "ns3/log.h"
#include "ns3/packet-socket-factory.h"
#include "ns3/packet-socket.h"
#include "ns3/packet-socket-client.h"
#include <sstream>
#include <iostream>
#include <filesystem>
#include <limits>
#include <algorithm>
#include <utility>
#include <stdexcept>
#define MYGYM_FLOW_INPUTFILE_LOCATION "scratch/my_environment/input/flow.json"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MyGymEnv");

NS_OBJECT_ENSURE_REGISTERED (MyGymEnv);

MyGymEnv::MyGymEnv() {
  throw std::runtime_error("Default constructor called.");
}

void MyGymEnv::initializeFlows(Ipv4Address address)
{
  std::stringstream ss;
	std::ifstream inputFile(MYGYM_FLOW_INPUTFILE_LOCATION);
	ss << inputFile.rdbuf();

	flows = getFlows(ss.str());

  std::vector<std::pair<std::reference_wrapper<Flow>, std::uint64_t>> pairs;
  ns3::NodeContainer nodes;
  nodes.Add(myNode);
  for (auto& flow : flows)
  {
    pairs.emplace_back(std::make_pair<std::reference_wrapper<Flow>, std::uint64_t>(std::ref(flow), 4608ULL));
  }
  this->applications = FlowGenerator::Install(this, std::move(pairs), *myNode);
  for (auto application : applications)
  {
    application->SetRemote("ns3::PacketSocketFactory", address);
  }
}

// MyGymEnv::MyGymEnv () // = delete;
// {
//   NS_LOG_FUNCTION (this);
//   m_interval = Seconds (0.1);
//   Simulator::Schedule (Seconds (0.0), &MyGymEnv::ScheduleNextStateRead, this);

// 	initializeFlows();
// }

MyGymEnv::MyGymEnv (Time stepTime, double linkspeed, MyNode* node, Ipv4Address address)
: droppedPacketSize(0), sentPacketSize{0}, linkSpeed{linkspeed}, myNode{node}
{
  std::cout << "Setting linkspeed = " << linkSpeed << std::endl;
  NS_LOG_FUNCTION (this);
  m_interval = stepTime;
  Simulator::Schedule (Seconds (0.0), &MyGymEnv::ScheduleNextStateRead, this);

  initializeFlows(address);
}

void
MyGymEnv::ScheduleNextStateRead ()
{
  NS_LOG_FUNCTION (this);
  Simulator::Schedule (m_interval, &MyGymEnv::ScheduleNextStateRead, this);
  Notify ();
}

MyGymEnv::~MyGymEnv ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
MyGymEnv::GetTypeId (void)
{
  static TypeId tid = TypeId ("MyGymEnv")
                          .SetParent<OpenGymEnv> ()
                          .SetGroupName ("OpenGym")
                          .AddConstructor<MyGymEnv> ();
  return tid;
}

void
MyGymEnv::DoDispose ()
{
  NS_LOG_FUNCTION (this);
}

std::uint64_t getSimulatorTimestampInSeconds()
{
  return Simulator::Now().ToInteger(ns3::Time::Unit::S);
}

void MyGymEnv::addSentPacket(std::uint64_t size, Flow& flow)
{
  // std::cout << "Add sent packet: " << size << ", " << flow.flow_uid << std::endl;
  // std::cout << "sentPacketSize: " << sentPacketSize << "\ntimestamp: " << getSimulatorTimestampInSeconds() << "\nspeed: " << linkSpeed << std::endl;
  // std::cout << (this->sentPacketSize + size) / double(getSimulatorTimestampInSeconds()) << " > " << linkSpeed << " ?" <<std::endl;
  if ((this->sentPacketSize + size) / (double(getSimulatorTimestampInSeconds()) + 1) > this->linkSpeed)
  {
    //NS_LOG_UNCOND("Dropping packet for flow " << flow.flow_uid << ", of size " << size);
    this->droppedPacketSize += size;
  }
  else
  {
    //NS_LOG_UNCOND("Sending packet for flow " << flow.flow_uid << ", of size " << size);
    this->sentPacketSize += size;
    flow.addSentPacket(size);
  }
}

/*
Define observation space
*/
Ptr<OpenGymSpace>
MyGymEnv::GetObservationSpace ()
{
  auto shape = std::vector<std::uint32_t>{static_cast<unsigned>(flows.size()) * 3,};

  Ptr<OpenGymBoxSpace> flowBox = CreateObject<OpenGymBoxSpace>(0, std::numeric_limits<float>::infinity(), shape, TypeNameGet<float> ());
  return flowBox;
}

/*
Define action space
*/
Ptr<OpenGymSpace>
MyGymEnv::GetActionSpace ()
{
  // Allow as many flows as it wants; up to 2^32, which we'll assume is bigger than will ever be relevant.
  Ptr<OpenGymDiscreteSpace> flowToggleSpace = CreateObject<OpenGymDiscreteSpace>(std::numeric_limits<unsigned>::max());
  return flowToggleSpace;
}

/*
Define game over condition
*/
bool
MyGymEnv::GetGameOver ()
{
  NS_LOG_UNCOND("Game over check: ");
  if (std::all_of(flows.begin(), flows.end(), [](auto& flow) -> bool { return flow.isCompleted(); }))
  {
    NS_LOG_UNCOND("Game over because all flows are completed.");
    return true;
  }
  // if (this->droppedPacketSize > 100000000)
  // {
  //   NS_LOG_UNCOND("Game over because too much packet loss.");
  //   return true; // Stop simming if dropped > 100Mb
  // }
  NS_LOG_UNCOND("Game over = false");
  return false;
}

/*
Collect observations
*/
Ptr<OpenGymDataContainer>
MyGymEnv::GetObservation ()
{
  auto shape = std::vector<std::uint32_t>{static_cast<unsigned>(flows.size()) * 3};
  Ptr<OpenGymBoxContainer<float> > flowBox = CreateObject<OpenGymBoxContainer<float> >(shape);

  for (auto& flow : flows)
  {
    std::cout << "flow fractions (" << flow.flow_uid << "): " << flow.getCurrentPeriodFraction() << ", " << flow.getCurrentSentFraction() << std::endl;
    flowBox->AddValue(flow.getCurrentSentFraction());
    flowBox->AddValue(flow.getCurrentPeriodFraction());
    if (flow.isCompleted())
      flowBox->AddValue(1);
    else
      flowBox->AddValue(0);
  }
  return flowBox;
}

/*
Define reward function
*/
float
MyGymEnv::GetReward ()
{
  float reward = 0.0;
  for (auto& flow : flows)
  {
    if (flow.isCompleted())
    {
      reward += flow.point_value;
    }
  }

  reward -= this->droppedPacketSize / 1000000.0;
  reward += this->sentPacketSize / 100000000.0;

  return reward;
}

/*
Define extra info. Optional
*/
std::string
MyGymEnv::GetExtraInfo ()
{
//   NS_LOG_UNCOND("Empty extra info returned.");
  return "";
}

/*
Execute received actions
*/
bool
MyGymEnv::ExecuteActions (Ptr<OpenGymDataContainer> action)
{
  Ptr<OpenGymBoxContainer<float>> flowToggles = DynamicCast<OpenGymBoxContainer<float>> (action);

  // NS_LOG_UNCOND ("MyExecuteActions: " << action);
  auto data = flowToggles->GetData();
  NS_ASSERT(data.size() == this->applications.size());
  for (auto i = 0UL; i < data.size(); ++i)
  {
    NS_ASSERT(this->applications[i]->getFlow().flow_uid == flows[i].flow_uid);
    if (data[i] > 0)
    {
      this->applications[i]->StartApplication();
    }
    else if (data[i] <= 0)
    {
      this->applications[i]->StopApplication();
    }
    else
    {
      std::cout << (data[i]) << std::endl;
      NS_ASSERT(false); // We fucked up - constants comparison is failing
    }
  }

  // NS_LOG_UNCOND ("MyExecuteActions: " << action);
  // NS_LOG_UNCOND ("TOGGLES: " << flowToggles);
  return true;
}

} // namespace ns3