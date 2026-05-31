#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>

// Include the actual protocol definitions
#include "../Base/include/mesh_protocol.hpp"

using namespace domoc;

// Test suite for CRC8 function
class CRC8Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing to set up for CRC8 tests
    }
};

TEST_F(CRC8Test, EmptyBuffer) {
    uint8_t empty[0];
    uint8_t crc = domoc::crc8(empty, 0);
    EXPECT_EQ(crc, 0x00);
}

TEST_F(CRC8Test, SingleByte) {
    uint8_t data[] = {0xAA};
    uint8_t crc = domoc::crc8(data, sizeof(data));
    // CRC-8/MAXIM of 0xAA = 0x1B (known value)
    EXPECT_EQ(crc, 0x1B);
}

TEST_F(CRC8Test, SimplMessage) {
    uint8_t msg[] = {0x02, 0x01, 0x02, 0x00}; // type, src, dst, seq
    uint8_t crc = domoc::crc8(msg, sizeof(msg));
    // Verify CRC is deterministic
    uint8_t crc2 = domoc::crc8(msg, sizeof(msg));
    EXPECT_EQ(crc, crc2);
}

TEST_F(CRC8Test, MutationDetection) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t crc1 = domoc::crc8(data, sizeof(data));

    // Change one byte and verify CRC changes
    data[2] = 0xFF;
    uint8_t crc2 = domoc::crc8(data, sizeof(data));

    EXPECT_NE(crc1, crc2);
}

// Test suite for message payload structures
class PayloadTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Verify packed sizes
    }
};

TEST_F(PayloadTest, CmdPayloadSize) {
    EXPECT_EQ(sizeof(domoc::CmdPayload), 2);
}

TEST_F(PayloadTest, RegPayloadSize) {
    EXPECT_EQ(sizeof(domoc::RegPayload), 25); // 16 + 1 + 1 + 6 + 1 pad
}

TEST_F(PayloadTest, KeyOnPayloadSize) {
    EXPECT_EQ(sizeof(domoc::KeyOnPayload), 8); // float + uint32
}

TEST_F(PayloadTest, StepOpenPayloadSize) {
    EXPECT_EQ(sizeof(domoc::StepOpenPayload), 10); // uint8 + uint8 + float
}

TEST_F(PayloadTest, MeshMsgSize) {
    EXPECT_EQ(sizeof(domoc::MeshMsg), 204); // 4 + 200
}

// Test suite for message dispatching logic
class MessageDispatchTest : public ::testing::Test {
protected:
    domoc::MeshMsg create_msg(domoc::MsgType type, uint8_t src, uint8_t dst, uint8_t seq) {
        domoc::MeshMsg msg{};
        msg.msg_type = type;
        msg.src_id = src;
        msg.dst_id = dst;
        msg.seq_num = seq;
        return msg;
    }

    void SetUp() override {
        // Nothing to set up
    }
};

TEST_F(MessageDispatchTest, CommandMessageParsing) {
    auto msg = create_msg(domoc::MSG_COMMAND, 0x01, 0x02, 0);
    domoc::CmdPayload cmd{};
    cmd.action_code = domoc::ACTION_OPEN;
    cmd.param = 0;
    std::memcpy(msg.payload, &cmd, sizeof(cmd));

    // Verify we can reconstruct the payload
    domoc::CmdPayload reconstructed{};
    std::memcpy(&reconstructed, msg.payload, sizeof(cmd));
    EXPECT_EQ(reconstructed.action_code, domoc::ACTION_OPEN);
    EXPECT_EQ(reconstructed.param, 0);
}

TEST_F(MessageDispatchTest, RegisterPayloadParsing) {
    auto msg = create_msg(domoc::MSG_REGISTER, 0x02, 0x01, 0);
    domoc::RegPayload reg{};
    std::strncpy(reg.name, "STEP", sizeof(reg.name) - 1);
    reg.node_type = static_cast<uint8_t>(domoc::NodeType::STEP);
    reg.reconnect = 0;
    std::memcpy(msg.payload, &reg, sizeof(reg));

    domoc::RegPayload reconstructed{};
    std::memcpy(&reconstructed, msg.payload, sizeof(reg));
    EXPECT_STREQ(reconstructed.name, "STEP");
    EXPECT_EQ(reconstructed.node_type, static_cast<uint8_t>(domoc::NodeType::STEP));
}

TEST_F(MessageDispatchTest, BroadcastDetection) {
    auto msg = create_msg(domoc::MSG_KEY_ON, 0x01, domoc::NODE_ID_BROADCAST, 0);
    EXPECT_EQ(msg.dst_id, 0xFF);
    EXPECT_EQ(msg.dst_id, domoc::NODE_ID_BROADCAST);
}

// Test suite for state machine logic
class StepStateMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing to set up
    }
};

TEST_F(StepStateMachineTest, StateEnumValues) {
    // Verify state enum values are as expected
    EXPECT_EQ(static_cast<int>(domoc::StepState::STATE_CLOSED), 1);
    EXPECT_EQ(static_cast<int>(domoc::StepState::STATE_OPEN), 2);
    EXPECT_EQ(static_cast<int>(domoc::StepState::STATE_OPENING), 3);
    EXPECT_EQ(static_cast<int>(domoc::StepState::STATE_CLOSING), 4);
    EXPECT_EQ(static_cast<int>(domoc::StepState::STATE_ERROR), 6);
}

TEST_F(StepStateMachineTest, StepStatusPacked) {
    // Verify StepStatus layout is correct for HMI offset parsing
    domoc::StepStatus s{};
    EXPECT_EQ(offsetof(domoc::StepStatus, state), 0);
    EXPECT_EQ(offsetof(domoc::StepStatus, temperature), 7);
    EXPECT_EQ(offsetof(domoc::StepStatus, humidity), 11);
    EXPECT_EQ(sizeof(domoc::StepStatus), 15);
}

// Test suite for action codes
class ActionCodeTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(ActionCodeTest, ActionCodeValues) {
    EXPECT_EQ(domoc::ACTION_OPEN, 0x01);
    EXPECT_EQ(domoc::ACTION_CLOSE, 0x02);
    EXPECT_EQ(domoc::ACTION_LIGHT_ON, 0x05);
    EXPECT_EQ(domoc::ACTION_LIGHT_OFF, 0x06);
}

TEST_F(ActionCodeTest, NodeIdValues) {
    EXPECT_EQ(domoc::NODE_ID_MASTER, 0x01);
    EXPECT_EQ(domoc::NODE_ID_STEP, 0x02);
    EXPECT_EQ(domoc::NODE_ID_BROADCAST, 0xFF);
}
