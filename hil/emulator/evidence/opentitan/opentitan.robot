*** Settings ***
Library           RenodeLibrary
Library           String

*** Variables ***
${SCRIPT}         C:/Users/USER/LastState/latch/hil/emulator/evidence/opentitan/opentitan.resc
${ELF}            C:/Users/USER/LastState/latch/build/emulator/latch-renode-opentitan.elf
${MARKER}         0x10001000

*** Keywords ***
Read Word
    [Arguments]    ${addr}
    ${raw}=  Execute Command  sysbus ReadDoubleWord ${addr}
    ${val}=  Strip String  ${raw}
    [Return]    ${val}

Wait For Marker
    ${magic}=  Read Word  ${MARKER}
    Should Be Equal As Strings  ${magic}  0x4C415443
    ${status_raw}=  Read Word  0x10001004
    ${status}=  Convert To Integer  ${status_raw}
    Should Be Equal As Integers  ${status}  0

*** Test Cases ***
opentitan Illegal Instruction Reaches Trap Handler
    Execute Script         ${SCRIPT}
    Execute Command        sysbus LoadELF @${ELF}
    Execute Command        sysbus.cpu0 PC `sysbus GetSymbolAddress "_start"`
    Start Emulation
    Wait Until Keyword Succeeds    10    0.5    Wait For Marker
