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
  return sizeof(unsigned) + sizeof(const FlowSpec*);
}
void FlowTag::Serialize (TagBuffer i) const
{
  i.Write ((const uint8_t*)&flowId, sizeof(unsigned));
	i.Write ((const uint8_t*)&flowSpec, sizeof(const FlowSpec*));
}
void FlowTag::Deserialize (TagBuffer i)
{
	i.Read((uint8_t*)&flowId, sizeof(unsigned));
	i.Read((uint8_t*)&flowSpec, sizeof(const FlowSpec*));

}

void FlowTag::setId (unsigned id)
{
  flowId = id;
}
unsigned FlowTag::getId (void) const
{
  return flowId;
}
void FlowTag::setFlowSpec(const FlowSpec& spec)
{
	flowSpec = &spec;
}
const FlowSpec& FlowTag::getFlowSpec() const
{
	return *flowSpec;
}

void FlowTag::Print (std::ostream &os) const
{
  os << "(flowId=" << flowId << ",flowSpec=" << flowSpec << ")";
}
