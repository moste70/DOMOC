#include "hmi_node.hpp"

namespace domoc {

// L'HMI non ha un descriptor proprio: riceve i descriptor degli altri nodi
// e costruisce il carosello dinamicamente (Document/comunicazione_nodi.md).
const NodeDescriptor HmiNode::kDesc = {};

HmiNode::HmiNode() : NodeBase(NODE_ID_HMI, NodeType::HMI, "HMI") {}

const NodeDescriptor& HmiNode::descriptor() const { return kDesc; }

size_t HmiNode::build_status_payload(uint8_t* out, size_t max) {
    (void)out; (void)max;
    return 0;
}

} // namespace domoc
