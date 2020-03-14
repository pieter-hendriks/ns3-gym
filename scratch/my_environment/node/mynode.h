#pragma once
#ifndef INC_MYENV_MYNODE_H_
#define INC_MYENV_MYNODE_H_

#include "ns3/core-module.h"
#include "ns3/opengym-module.h"
#include "ns3/node.h"
class MyNode : public ns3::Node
{
	public:
	MyNode () { };
	MyNode (ns3::Node&& base) : ns3::Node(std::move(base)) { };

	virtual ~MyNode() {};

};

#endif