# Native transport and secure-element integrations

## TLS and cellular

`ports/mbedtls` turns an already handshaken `mbedtls_ssl_context` into a retry-safe Latch transport and can require a verified peer. `ports/zephyr/zephyr_tls_socket` performs DNS resolution, creates a TLS 1.2 socket, installs the security tag, requires peer verification and connects it. On Zephyr cellular targets using modem socket offload, the same driver sends through the LTE-M, NB-IoT or cellular modem's real network path.

## BLE GATT

`zephyr_ble_gatt.c` defines a 128-bit Latch service, encrypted notification characteristic, encrypted CCC and encrypted control-write characteristic. Envelopes are split at the negotiated ATT MTU and queued with `bt_gatt_notify()`.

## CAN and CAN-FD

`zephyr_can.c` uses the controller's `can_send()` API. It supports standard or extended identifiers, CAN FD and bitrate switching, and fragments one envelope into indexed frames with a transfer identifier. The receiver must reassemble all frames from one transfer before validating LEP/AEAD.

## LoRaWAN

`zephyr_lorawan.c` calls the joined Zephyr LoRaWAN stack directly. It honors the configured regional payload limit, fragments envelopes, and supports confirmed or unconfirmed uplinks. Envelope AEAD remains enabled because LoRaWAN link security and end-to-end application security are separate concerns.

## CryptoAuthLib

Build with `LS_BUILD_CRYPTOAUTHLIB_PORT=ON`. After initializing CryptoAuthLib for the target ATECC/SHA device, configure `ls_cryptoauthlib_context_t` with the private-key slot, HMAC/KDF slot, certificate definition and signer public key. The resulting `ls_secure_element_t` provides hardware random bytes, P-256 signatures, reconstructed DER certificates and hardware HMAC-derived 256-bit keys. It can be passed directly as the Latch CSPRNG through `ls_secure_element_random`.

These adapters call the vendor stacks; they do not implement a BLE controller, LoRaWAN MAC, CAN controller, cellular modem firmware or TLS protocol internally. Board configuration, credentials, radio certification and vendor-stack lifecycle remain platform responsibilities.
