#include "flowtag.h"
using namespace ns3;
TypeId FlowTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("FlowTag")
    .SetParent<Tag> ()
    .AddConstructor<FlowTag> ()
    // .AddAttribute ("flowId",
    //                "Id of a flow between two nodes!",
    //                EmptyAttributeValue (),
    //                MakeEmptyAttributeAccessor(),
    //                MakeEmptyAttributeChecker())
  ;
  return tid;
}
TypeId FlowTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t FlowTag::GetSerializedSize (void) const
{
  return 4;
}
void FlowTag::Serialize (TagBuffer i) const
{
  i.Write ((const uint8_t *)&flowId, 4);
}
void FlowTag::Deserialize (TagBuffer i)
{
	i.Read((uint8_t*)&flowId, 4);
}

void FlowTag::setId (unsigned id)
{
  flowId = id;
}
unsigned FlowTag::getId (void) const
{
  return flowId;
}

void FlowTag::Print (std::ostream &os) const
{
  os << "flowId=" << flowId;
}
