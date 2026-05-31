# DomoC Host-Based Unit Tests

Test suite per la logica core del sistema DomoC senza dipendenze da ESP-IDF.

## Cosa viene testato

- **CRC8**: Verifica l'integrità dei messaggi, correttezza dell'algoritmo
- **Messaggi mesh**: Parsing e serializzazione dei payload
- **State machine**: Logica della macchina a stati dello STEP
- **Strutture dati**: Dimensioni, offset, layout packed

## Compilare ed eseguire

### Prerequisiti
- CMake 3.16+
- C++ compiler (g++, clang, MSVC)
- Internet (per scaricare Google Test automaticamente)

### Esecuzione veloce

```bash
cd Code/tests
bash run_tests.sh
```

### Compilazione manuale

```bash
cd Code/tests
mkdir -p build
cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

## Test disponibili

### CRC8Test
- `EmptyBuffer`: CRC di buffer vuoto
- `SingleByte`: CRC su singolo byte
- `SimplMessage`: CRC su messaggio semplice
- `MutationDetection`: Verifica che cambiamenti rilevabili modificano il CRC

### PayloadTest
- `CmdPayloadSize`: Verifica dimensione comando (2 byte)
- `RegPayloadSize`: Verifica dimensione registrazione (25 byte)
- `KeyOnPayloadSize`: Verifica KEY_ON payload (8 byte)
- `StepOpenPayloadSize`: Verifica STEP_OPEN payload (10 byte)
- `MeshMsgSize`: Verifica header + payload mesh (204 byte)

### MessageDispatchTest
- `CommandMessageParsing`: Parsing di messaggi comando
- `RegisterPayloadParsing`: Parsing di registrazione nodo
- `BroadcastDetection`: Riconoscimento messaggi broadcast

### StepStateMachineTest
- `StateEnumValues`: Verifica valori enum stati
- `StepStatusPacked`: Verifica layout packed di StepStatus per offset HMI

### ActionCodeTest
- `ActionCodeValues`: Verifica codici azione
- `NodeIdValues`: Verifica ID nodi

## Output atteso

```
[==========] X tests from Y test suites ran.
[  PASSED  ] All tests passed
```

## Aggiungere nuovi test

1. Creare una nuova classe di test che eredita da `::testing::Test`
2. Implementare `SetUp()` se necessario
3. Aggiungere `TEST_F(ClassName, TestName)` per ogni test
4. Compilare e eseguire con `bash run_tests.sh`

Esempio:

```cpp
class MyTest : public ::testing::Test {
protected:
    void SetUp() override { /* init */ }
};

TEST_F(MyTest, Description) {
    EXPECT_EQ(actual, expected);
}
```

## Link utili

- [Google Test documentation](https://google.github.io/googletest/)
- DomoC protocol: `Document/messaggi_mesh.md`
- Node descriptor: `Document/comunicazione_nodi.md`
