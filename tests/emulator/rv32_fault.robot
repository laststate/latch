*** Settings ***
Library           RenodeLibrary

*** Variables ***
${SCRIPT}         ${CURDIR}/rv32_fault.resc
${UART}           sysbus.uart0
${ELF}            ${CURDIR}/../../build/emulator/latch-renode-rv32.elf

*** Test Cases ***
Illegal Instruction Reaches RV32 Trap
    Execute Script         ${SCRIPT}
    Execute Command        sysbus LoadELF @${ELF}
    Execute Command        cpu PC `sysbus GetSymbolAddress "_start"`
    Create Terminal Tester    ${UART}
    Start Emulation
    Wait For Line On Uart     LATCH:PASS:RV32_ILLEGAL_INSTRUCTION
