# Arduino ESP32 cooperative capture

This Arduino IDE example verifies the native library layout and captures one
explicit error into a RAM-backed Latch spool before sending it to `Serial`.

1. Install the `esp32` platform with Arduino Boards Manager.
2. Install this checkout as an Arduino library, or open it through the
   Arduino IDE's **Sketch > Include Library > Add .ZIP Library...** flow.
3. Open `File > Examples > LastState Latch > arduino_esp32_cooperative`.
4. Select an ESP32 board, upload, and open Serial Monitor at 115200 baud.

Expected output includes a delivered LEP envelope followed by `Latch
cooperative capture complete`.

The sample intentionally uses volatile RAM. It does not install a panic/fault
handler, prove crash persistence, or configure flash storage. For a physical
crash/reboot/ACK walkthrough, use the ESP-IDF `esp32-first-crash` example.
