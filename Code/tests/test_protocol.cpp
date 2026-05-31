#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>

// Include the actual protocol definitions (self-contained, no ESP-IDF dependencies)
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
    uint8_t crc = crc8(empty, 0);
    EXPECT_EQ(crc, 0x00);
}

TEST_F(CRC8Test, SingleByte) {
    uint8_t data[] = {0xAA};
    uint8_t crc = crc8(data, sizeof(data));
    // CRC-8/MAXIM of 0xAA = 0x1B (known value)
    EXPECT_EQ(crc, 0x1B);
}

TEST_F(CRC8Test, SimplMessage) {
    uint8_t msg[] = {0x02, 0x01, 0x02, 0x00}; // type, src, dst, seq
    uint8_t crc = crc8(msg, sizeof(msg));
    // Verify CRC is deterministic
    uint8_t crc2 = crc8(msg, sizeof(msg));
    EXPECT_EQ(crc, crc2);
}

TEST_F(CRC8Test, MutationDetection) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t crc1 = crc8(data, sizeof(data));

    // Change one byte and verify CRC changes
    data[2] = 0xFF;
    uint8_t crc2 = crc8(data, sizeof(data));

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
    EXPECT_EQ(sizeof(CmdPayload), 2);
}

TEST_F(PayloadTest, RegPayloadSize) {
    EXPECT_EQ(sizeof(RegPayload), 25); // 16 + 1 + 1 + 6 + 1 pad
}

TEST_F(PayloadTest, KeyOnPayloadSize) {
    EXPECT_EQ(sizeof(KeyOnPayload), 8); // float + uint32
}

TEST_F(PayloadTest, StepOpenPayloadSize) {
    EXPECT_EQ(sizeof(StepOpenPayload), 10); // uint8 + uint8 + float
}

TEST_F(PayloadTest, MeshMsgSize) {
    EXPECT_EQ(sizeof(MeshMsg), 204); // 4 + 200
}

// Test suite for message dispatching logic
class MessageDispatchTest : public ::testing::Test {
protected:
    MeshMsg create_msg(MsgType type, uint8_t src, uint8_t dst, uint8_t seq) {
        MeshMsg msg{};
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
    auto msg = create_msg(MSG_COMMAND, 0x01, 0x02, 0);
    CmdPayload cmd{};
    cmd.action_code = ACTION_OPEN;
    cmd.param = 0;
    std::memcpy(msg.payload, &cmd, sizeof(cmd));

    // Verify we can reconstruct the payload
    CmdPayload reconstructed{};
    std::memcpy(&reconstructed, msg.payload, sizeof(cmd));
    EXPECT_EQ(reconstructed.action_code, ACTION_OPEN);
    EXPECT_EQ(reconstructed.param, 0);
}

TEST_F(MessageDispatchTest, RegisterPayloadParsing) {
    auto msg = create_msg(MSG_REGISTER, 0x02, 0x01, 0);
    RegPayload reg{};
    std::strncpy(reg.name, "STEP", sizeof(reg.name) - 1);
    reg.node_type = static_cast<uint8_t>(NodeType::STEP);
    reg.reconnect = 0;
    std::memcpy(msg.payload, &reg, sizeof(reg));

    RegPayload reconstructed{};
    std::memcpy(&reconstructed, msg.payload, sizeof(reg));
    EXPECT_STREQ(reconstructed.name, "STEP");
    EXPECT_EQ(reconstructed.node_type, static_cast<uint8_t>(NodeType::STEP));
}

TEST_F(MessageDispatchTest, BroadcastDetection) {
    auto msg = create_msg(MSG_KEY_ON, 0x01, NODE_ID_BROADCAST, 0);
    EXPECT_EQ(msg.dst_id, 0xFF);
    EXPECT_EQ(msg.dst_id, NODE_ID_BROADCAST);
}

// Test suite for action codes
class ActionCodeTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(ActionCodeTest, ActionCodeValues) {
    EXPECT_EQ(ACTION_OPEN, 0x01);
    EXPECT_EQ(ACTION_CLOSE, 0x02);
    EXPECT_EQ(ACTION_LIGHT_ON, 0x05);
    EXPECT_EQ(ACTION_LIGHT_OFF, 0x06);
}

TEST_F(ActionCodeTest, NodeIdValues) {
    EXPECT_EQ(NODE_ID_MASTER, 0x01);
    EXPECT_EQ(NODE_ID_STEP, 0x02);
    EXPECT_EQ(NODE_ID_BROADCAST, 0xFF);
}

// Test suite for message types
class MessageTypeTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(MessageTypeTest, MessageTypeValues) {
    EXPECT_EQ(MSG_COMMAND, 0x01);
    EXPECT_EQ(MSG_STATUS, 0x02);
    EXPECT_EQ(MSG_REGISTER, 0x05);
    EXPECT_EQ(MSG_KEY_ON, 0x11);
    EXPECT_EQ(MSG_KEY_OFF, 0x12);
}

TEST_F(MessageTypeTest, OTAMessageTypes) {
    EXPECT_EQ(MSG_OTA_START, 0x20);
    EXPECT_EQ(MSG_OTA_CHUNK, 0x21);
    EXPECT_EQ(MSG_OTA_END, 0x22);
    EXPECT_EQ(MSG_OTA_ACK, 0x23);
}

// Test suite for node types
class NodeTypeTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(NodeTypeTest, NodeTypeValues) {
    EXPECT_EQ(static_cast<uint8_t>(NodeType::MASTER), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(NodeType::STEP), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(NodeType::GREY_WATER), 0x03);
    EXPECT_EQ(static_cast<uint8_t>(NodeType::FRESH_WATER), 0x04);
}

// Test suite for error codes
class ErrorCodeTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(ErrorCodeTest, ErrorCodeValues) {
    EXPECT_EQ(ERR_NONE, 0x00);
    EXPECT_EQ(ERR_TIMEOUT, 0x01);
    EXPECT_EQ(ERR_ENDSTOP, 0x02);
    EXPECT_EQ(ERR_OVERCURRENT, 0x03);
    EXPECT_EQ(ERR_SENSOR, 0x04);
}
