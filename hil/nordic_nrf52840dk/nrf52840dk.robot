*** Settings ***
Library           RenodeLibrary

*** Variables ***
${SCRIPT}         ${CURDIR}/nrf52840dk.resc
${UART}           sysbus.uart0
${ELF}            ${CURDIR}/../../build/emulator/latch-renode-nrf52840.elf

*** Test Cases ***
nRF52840 UsageFault Reaches Handler
    Execute Script         ${SCRIPT}
    Execute Command        sysbus LoadELF @${ELF}
    Create Terminal Tester    ${UART}
    Start Emulation
    Wait For Line On Uart     HIL:ARMED:USAGEFAULT    timeout=3
    Wait For Line On Uart     HIL:PASS:USAGEFAULT     timeout=3
