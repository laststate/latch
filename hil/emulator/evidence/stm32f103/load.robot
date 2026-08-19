*** Settings ***
Library           RenodeLibrary

*** Variables ***
${SCRIPT}         ${CURDIR}/stm32f103.resc

*** Test Cases ***
Load
    Execute Script         ${SCRIPT}