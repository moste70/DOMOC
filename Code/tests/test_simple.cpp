#include <cstdint>
#include <cstring>
#include <cstdio>

// Include the actual protocol definitions
#include "../Base/include/mesh_protocol.hpp"

using namespace domoc;

int main() {
    printf("=== DomoC Protocol Tests (No GTest) ===\n\n");

    // Test 1: CRC8 empty buffer
    {
        uint8_t empty[0];
        uint8_t crc = crc8(empty, 0);
        printf("Test 1 - CRC8 empty buffer: %s\n", crc == 0x00 ? "PASS" : "FAIL");
    }

    // Test 2: CRC8 single byte
    {
        uint8_t data[] = {0xAA};
        uint8_t crc = crc8(data, sizeof(data));
        printf("Test 2 - CRC8 0xAA: %s (got 0x%02X, expected 0x1B)\n",
               crc == 0x1B ? "PASS" : "FAIL", crc);
    }

    // Test 3: CRC8 deterministic
    {
        uint8_t msg[] = {0x02, 0x01, 0x02, 0x00};
        uint8_t crc1 = crc8(msg, sizeof(msg));
        uint8_t crc2 = crc8(msg, sizeof(msg));
        printf("Test 3 - CRC8 deterministic: %s (0x%02X == 0x%02X)\n",
               crc1 == crc2 ? "PASS" : "FAIL", crc1, crc2);
    }

    // Test 4: Payload sizes
    printf("Test 4 - Payload sizes:\n");
    printf("  CmdPayload: %s (%zu bytes, expected 2)\n",
           sizeof(CmdPayload) == 2 ? "PASS" : "FAIL", sizeof(CmdPayload));
    printf("  RegPayload: %s (%zu bytes, expected 25)\n",
           sizeof(RegPayload) == 25 ? "PASS" : "FAIL", sizeof(RegPayload));
    printf("  KeyOnPayload: %s (%zu bytes, expected 8)\n",
           sizeof(KeyOnPayload) == 8 ? "PASS" : "FAIL", sizeof(KeyOnPayload));
    printf("  MeshMsg: %s (%zu bytes, expected 204)\n",
           sizeof(MeshMsg) == 204 ? "PASS" : "FAIL", sizeof(MeshMsg));

    // Test 5: Message type values
    printf("Test 5 - Message types:\n");
    printf("  MSG_COMMAND: %s (0x%02X == 0x01)\n",
           MSG_COMMAND == 0x01 ? "PASS" : "FAIL", MSG_COMMAND);
    printf("  MSG_REGISTER: %s (0x%02X == 0x05)\n",
           MSG_REGISTER == 0x05 ? "PASS" : "FAIL", MSG_REGISTER);
    printf("  MSG_KEY_ON: %s (0x%02X == 0x11)\n",
           MSG_KEY_ON == 0x11 ? "PASS" : "FAIL", MSG_KEY_ON);

    // Test 6: Node IDs
    printf("Test 6 - Node IDs:\n");
    printf("  NODE_ID_MASTER: %s (0x%02X == 0x01)\n",
           NODE_ID_MASTER == 0x01 ? "PASS" : "FAIL", NODE_ID_MASTER);
    printf("  NODE_ID_STEP: %s (0x%02X == 0x02)\n",
           NODE_ID_STEP == 0x02 ? "PASS" : "FAIL", NODE_ID_STEP);
    printf("  NODE_ID_BROADCAST: %s (0x%02X == 0xFF)\n",
           NODE_ID_BROADCAST == 0xFF ? "PASS" : "FAIL", NODE_ID_BROADCAST);

    // Test 7: Message parsing
    {
        printf("Test 7 - Message parsing:\n");
        MeshMsg msg{};
        msg.msg_type = MSG_COMMAND;
        msg.src_id = 0x01;
        msg.dst_id = 0x02;
        msg.seq_num = 0;
        CmdPayload cmd{};
        cmd.action_code = ACTION_OPEN;
        cmd.param = 0;
        std::memcpy(msg.payload, &cmd, sizeof(cmd));

        CmdPayload reconstructed{};
        std::memcpy(&reconstructed, msg.payload, sizeof(cmd));
        printf("  Command reconstruction: %s (action 0x%02X == 0x01)\n",
               reconstructed.action_code == ACTION_OPEN ? "PASS" : "FAIL",
               reconstructed.action_code);
    }

    printf("\n=== All basic tests completed ===\n");
    return 0;
}
