#pragma once
#include "node_base.hpp"

namespace domoc {

// MASTER/ROOT — root mesh fissa, registry nodi, routing OTA, forwarding HMI.
// Stub: la logica completa è documentata in Document/nodes/master.md.
class MasterNode : public NodeBase {
public:
    MasterNode();

protected:
    const NodeDescriptor& descriptor() const override;
    size_t build_status_payload(uint8_t* out, size_t max) override;
    // Il MASTER non risponde a comandi dall'HMI: on_command non ridefinito.

private:
    static const NodeDescriptor kDesc;
};

} // namespace domoc
