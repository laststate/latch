*** Settings ***
Library           RenodeLibrary
Library           String

*** Variables ***
${SCRIPT}         C:/Users/USER/LastState/latch/hil/emulator/evidence/s32k118/s32k118.resc
${ELF}            C:/Users/USER/LastState/latch/build/emulator/latch-renode-s32k118.elf
${MARKER}         0x20001000

*** Keywords ***
Read Word
    [Arguments]    ${addr}
    ${raw}=  Execute Command  sysbus ReadDoubleWord ${addr}
    ${val}=  Strip String  ${raw}
    [Return]    ${val}

Wait For Marker
    ${magic}=  Read Word  ${MARKER}
    Should Be Equal As Strings  ${magic}  0x4C415443
    ${status_raw}=  Read Word  0x20001004
    ${status}=  Convert To Integer  ${status_raw}
    Should Be Equal As Integers  ${status}  0

*** Test Cases ***
s32k118 USAGEFAULT Reaches Production Handler
    Execute Script         ${SCRIPT}
    Execute Command        sysbus LoadELF @${ELF}
    Execute Command        sysbus.cpu Reset
    Start Emulation
    Wait Until Keyword Succeeds    10    0.5    Wait For Marker
