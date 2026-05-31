#include "master_node.hpp"
#include <cstring>

namespace domoc {

// Il MASTER non si auto-descrive all'HMI (nessuna icona nel carosello).
const NodeDescriptor MasterNode::kDesc = {};

MasterNode::MasterNode()
    : NodeBase(NODE_ID_MASTER, NodeType::MASTER, "MASTER") {}

const NodeDescriptor& MasterNode::descriptor() const { return kDesc; }

size_t MasterNode::build_status_payload(uint8_t* out, size_t max) {
    (void)out; (void)max;
    return 0;
}

} // namespace domoc
