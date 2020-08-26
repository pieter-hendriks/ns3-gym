#pragma once
#ifndef INC_FLOW_TAG_H_
#define INC_FLOW_TAG_H_
#include "../simulation/flow.h"
#include <ns3/tag.h>
#include <ns3/type-id.h>
#include <ns3/simulator.h>
class FlowTag : public ns3::Tag 
{
public:
  static ns3::TypeId GetTypeId (void);
  virtual ns3::TypeId GetInstanceTypeId (void) const;

  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (ns3::TagBuffer i) const;
  virtual void Deserialize (ns3::TagBuffer i);

  // these are our accessors to our tag structure
  void setId (unsigned id);
  unsigned getId () const;
	void setFlowSpec(const FlowSpec& spec);
	const FlowSpec& getFlowSpec() const;

  void Print (std::ostream &os) const;

private:
  unsigned flowId;
	const FlowSpec* flowSpec;
};
#endif