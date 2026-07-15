#ifndef LASTSTATE_PERIPHERAL_H
#define LASTSTATE_PERIPHERAL_H

#include <stdint.h>
#include "event.h"
typedef enum {
    LS_PERIPHERAL_I2C,
    LS_PERIPHERAL_SPI,
    LS_PERIPHERAL_UART,
    LS_PERIPHERAL_CAN,
    LS_PERIPHERAL_USB,
    LS_PERIPHERAL_STORAGE,
    LS_PERIPHERAL_NETWORK
} ls_peripheral_domain_t;
typedef enum {
    LS_I2C_NACK,
    LS_I2C_TIMEOUT,
    LS_I2C_BUS_BUSY,
    LS_I2C_ARBITRATION_LOST,
    LS_I2C_CLOCK_STRETCH,
    LS_I2C_SDA_STUCK,
    LS_I2C_SCL_STUCK,
    LS_I2C_RECOVERY_FAILED
} ls_i2c_fault_t;
typedef enum {
    LS_SPI_TIMEOUT,
    LS_SPI_CRC,
    LS_SPI_OVERRUN,
    LS_SPI_UNDERRUN,
    LS_SPI_DMA_FAILURE,
    LS_SPI_INVALID_FRAME
} ls_spi_fault_t;
typedef enum {
    LS_UART_FRAMING,
    LS_UART_PARITY,
    LS_UART_NOISE,
    LS_UART_OVERRUN,
    LS_UART_BUFFER_OVERFLOW,
    LS_UART_TIMEOUT
} ls_uart_fault_t;
typedef enum {
    LS_CAN_BUS_OFF,
    LS_CAN_ERROR_PASSIVE,
    LS_CAN_ARBITRATION_LOST,
    LS_CAN_RX_OVERFLOW,
    LS_CAN_TX_FAILURE,
    LS_CAN_FD_PROTOCOL
} ls_can_fault_t;
typedef enum {
    LS_USB_ENUMERATION,
    LS_USB_ENDPOINT_STALL,
    LS_USB_TRANSFER_TIMEOUT,
    LS_USB_VBUS_LOSS,
    LS_USB_DISCONNECT
} ls_usb_fault_t;
typedef enum {
    LS_STORAGE_WRITE,
    LS_STORAGE_ERASE,
    LS_STORAGE_ECC,
    LS_STORAGE_CRC,
    LS_STORAGE_FILESYSTEM,
    LS_STORAGE_BAD_BLOCK,
    LS_STORAGE_WEAR,
    LS_STORAGE_POWER_LOSS
} ls_storage_fault_t;
typedef enum {
    LS_NET_WIFI_DISCONNECT,
    LS_NET_DHCP,
    LS_NET_DNS,
    LS_NET_TLS,
    LS_NET_MQTT,
    LS_NET_CELLULAR_REGISTRATION,
    LS_NET_SIM,
    LS_NET_LORAWAN_JOIN
} ls_network_fault_t;
typedef struct {
    ls_peripheral_domain_t domain;
    uint16_t fault;
    uint16_t instance;
    uint32_t status;
    uint32_t address;
    uint32_t reg;
    uint32_t timeout_ms;
    uint32_t auxiliary[4];
} ls_peripheral_fault_t;
void ls_peripheral_fault(const ls_peripheral_fault_t *fault);
void ls_i2c_fault(ls_i2c_fault_t fault, uint16_t address, uint16_t reg);
void ls_spi_fault(ls_spi_fault_t fault);
void ls_uart_fault(ls_uart_fault_t fault);
void ls_can_fault(ls_can_fault_t fault, uint16_t tec, uint16_t rec, uint32_t status);
void ls_usb_fault(ls_usb_fault_t fault, uint8_t endpoint, uint32_t status);
void ls_storage_fault(ls_storage_fault_t fault, uint32_t address, uint32_t status);
void ls_network_fault(ls_network_fault_t fault, uint32_t status);
#endif
