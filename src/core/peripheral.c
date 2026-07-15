#include "internal.h"
void ls_peripheral_fault(const ls_peripheral_fault_t *fault) {
    if (!fault)
        return;
    static const char *domains[] = {"i2c", "spi", "uart", "can", "usb", "storage", "network"};
    const char *domain =
        fault->domain <= LS_PERIPHERAL_NETWORK ? domains[fault->domain] : "peripheral";
    ls_event_t event = {.type = LS_EVENT_PERIPHERAL,
                        .priority = LS_PRIORITY_ERROR,
                        .timestamp_ms = ls_uptime_ms(),
                        .domain = domain,
                        .code = fault->fault,
                        .severity = LS_SEVERITY_ERROR,
                        .message = "peripheral_fault",
                        .peripheral = fault,
                        .capture_level = LS_CAPTURE_SNAPSHOT};
    (void)ls_capture_event(&event);
}
void ls_i2c_fault(ls_i2c_fault_t fault, uint16_t address, uint16_t reg) {
    ls_peripheral_fault_t detail = {LS_PERIPHERAL_I2C, (uint16_t)fault, 0, 0, address, reg, 0,
                                    {0, 0, 0, 0}};
    ls_peripheral_fault(&detail);
}
void ls_spi_fault(ls_spi_fault_t fault) {
    ls_peripheral_fault_t detail = {LS_PERIPHERAL_SPI, (uint16_t)fault, 0, 0, 0, 0, 0,
                                    {0, 0, 0, 0}};
    ls_peripheral_fault(&detail);
}
void ls_uart_fault(ls_uart_fault_t fault) {
    ls_peripheral_fault_t detail = {LS_PERIPHERAL_UART, (uint16_t)fault, 0, 0, 0, 0, 0,
                                    {0, 0, 0, 0}};
    ls_peripheral_fault(&detail);
}
void ls_can_fault(ls_can_fault_t fault, uint16_t tec, uint16_t rec, uint32_t status) {
    ls_peripheral_fault_t detail = {LS_PERIPHERAL_CAN, (uint16_t)fault, 0, status, 0, 0, 0,
                                    {tec, rec, 0, 0}};
    ls_peripheral_fault(&detail);
}
void ls_usb_fault(ls_usb_fault_t fault, uint8_t endpoint, uint32_t status) {
    ls_peripheral_fault_t detail = {LS_PERIPHERAL_USB, (uint16_t)fault, endpoint, status, 0, 0, 0,
                                    {0, 0, 0, 0}};
    ls_peripheral_fault(&detail);
}
void ls_storage_fault(ls_storage_fault_t fault, uint32_t address, uint32_t status) {
    ls_peripheral_fault_t detail = {
        LS_PERIPHERAL_STORAGE, (uint16_t)fault, 0, status, address, 0, 0, {0, 0, 0, 0}};
    ls_peripheral_fault(&detail);
}
void ls_network_fault(ls_network_fault_t fault, uint32_t status) {
    ls_peripheral_fault_t detail = {LS_PERIPHERAL_NETWORK, (uint16_t)fault, 0, status, 0, 0, 0,
                                    {0, 0, 0, 0}};
    ls_peripheral_fault(&detail);
}
