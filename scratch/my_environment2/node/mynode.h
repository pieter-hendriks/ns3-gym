#pragma once
#ifndef INC_MY_NODE_H_
#define INC_MY_NODE_H_
#include "sendapplication.h"

#include "ns3/node.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4.h"

class MyNode : public ns3::Node
{
public:
	MyNode() = default;
	virtual ~MyNode() = default;
	// void addApplication(SendApplication&& app)
	// {
	// 	applications.emplace_back(std::move(app));
	// }

	ns3::Ipv4Address getIP() const
	{
		return this->GetObject<ns3::Ipv4>()->GetAddress(0,0).GetLocal();
	}
private:
	// std::vector<SendApplication> applications;
};

#endif