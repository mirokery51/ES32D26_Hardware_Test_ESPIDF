# ES32D26 Hardware Test (ESP-IDF)

Tento projekt testuje dosku ES32D26:
- relays (74HC595) sa zapinaju/vypinaju v testovacej sekvencii
- digitalne vstupy (74HC165) sa citaju a vypisuju
- analogovy vstup AI0 (GPIO34) sa cita cez ADC

## Obsah
- [Popis](#popis)
- [Pouzitie](#pouzitie)
- [Mapy pinov](#mapy-pinov)
- [Sekvencia rele](#sekvencia-rele)

## Popis
Firmware je postaveny na ESP-IDF (PlatformIO). Vypisuje stav vstupov a postupne
spina rele, aby bolo mozne overit funkcnost celeho PLC.

## Pouzitie
1. Build: `platformio run -e esp32dev`
2. Upload + monitor: `platformio run -e esp32dev -t upload -t monitor`

## Mapy pinov
- 74HC595 (rele): DATA=GPIO12, CLOCK=GPIO22, LATCH=GPIO23, OE=GPIO13
- 74HC165 (vstupy): DATA=GPIO15, CLOCK=GPIO2, LOAD=GPIO0
- AI piny: AI0=GPIO34, AI1=GPIO39, AI2=GPIO35, AI3=GPIO36
- DAC piny: DAC0=GPIO25, DAC1=GPIO26

## Sekvencia rele
- Step 0: R0 -> R7 ON (mask 0x01, 0x03, ..., 0xFF)
- Step 1: R7 -> R0 OFF (mask 0x7F, 0x3F, ..., 0x00)
- Step 2: ALL ON (0xFF)
- Step 3: ALL OFF (0x00)
- Step 4: STOP (0x00)

## Verzovanie
- HSV: verzia H=hodinky, S=s, V=vodotryskom
