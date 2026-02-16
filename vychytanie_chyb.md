# Vychytanie chyb (ES32D26_Hardware_Test_ESPIDF)

Tento dokument sumarizuje chyby, ktore sme postupne identifikovali od startu (ked rele necakali) az po finalnu verziu HSV.

## 1) Relays nereaguju (OE/JTAG/pad funkc ia)
**Problem:** Relays nereagovali, hoci kod menil data pre 74HC595.

**Pricina:** GPIO12/GPIO13 su strapping/JTAG piny. V ESP-IDF moze byt pin v inej funkcii a OE (GPIO13) sa realne nedvihal na spravnu uroven.

**Oprava:** Vynutit GPIO funkciu cez `esp_rom_gpio_pad_select_gpio()` pre piny DATA/CLOCK/LATCH/OE.

**Preco:** Takto sa pin odpoji od JTAG/strapping funkcii a zacne reagovat na `gpio_set_level()`.

**Pred (priklad):**
```c
// Bez pad select
// gpio_set_direction(...)
```

**Po (vysledok):**
```c
esp_rom_gpio_pad_select_gpio(RELAY_DATA);
esp_rom_gpio_pad_select_gpio(RELAY_CLOCK);
esp_rom_gpio_pad_select_gpio(RELAY_LATCH);
esp_rom_gpio_pad_select_gpio(RELAY_OE);
```

**Subor:** [src/main.c](src/main.c)

---

## 2) Nespravna polarita OE
**Problem:** OE bolo nastavene na HIGH, co vypina vystupy 74HC595 (OE je active-low).

**Oprava:** Zaviedli sme konstantu `RELAY_OE_ACTIVE_LOW` a OE nastavujeme podla nej.

**Preto:** Na doske je OE aktivne v LOW. Pri HIGH su vystupy vypnute.

**Pred (priklad):**
```c
gpio_set_level(RELAY_OE, 1); // nespravne pre active-low
```

**Po (vysledok):**
```c
#define RELAY_OE_ACTIVE_LOW 1

gpio_set_level(RELAY_OE, RELAY_OE_ACTIVE_LOW ? 0 : 1);
```

**Subor:** [src/main.c](src/main.c)

---

## 3) Debug vypis OE/CLK/DATA ukazoval same nuly
**Problem:** Debug vypis cez `gpio_get_level()` ukazoval 0, hoci rele realne cvakali.

**Pricina:** Piny boli nastavene ako `GPIO_MODE_OUTPUT`, cize vstupna cesta mohla byt vypnuta.

**Oprava:** Nastavili sme `GPIO_MODE_INPUT_OUTPUT` na pinoch relays, aby sa dal realne citat stav pinu.

**Preto:** Takto sa da diagnostikovat realna uroven na pine.

**Pred (priklad):**
```c
gpio_set_direction(RELAY_DATA, GPIO_MODE_OUTPUT);
```

**Po (vysledok):**
```c
gpio_set_direction(RELAY_DATA, GPIO_MODE_INPUT_OUTPUT);
```

**Subor:** [src/main.c](src/main.c)

---

## 4) ADC konfiguracia v cykle
**Problem:** ADC konfiguracia bola volana pri kazdom `printStatus()`.

**Oprava:** Presunuli sme `adc1_config_*` do inicializacie.

**Preto:** Inicializacia ADC sa robi raz, nie v kazdom cykle.

**Pred (priklad):**
```c
void printStatus() {
    adc1_config_width(...);
    adc1_config_channel_atten(...);
}
```

**Po (vysledok):**
```c
void app_main() {
    adc1_config_width(...);
    adc1_config_channel_atten(...);
}
```

**Subor:** [src/main.c](src/main.c)

---

## 5) Flash size mismatch warning (4MB vs 2MB)
**Problem:** Build hlasil, ze ocakava 4MB flash, ale nasiel 2MB.

**Oprava:** V `platformio.ini` sme nastavili `board_build.flash_size = 2MB` a `board_upload.flash_size = 2MB`.

**Preto:** Hardver ma 2MB flash, treba zosuladit build a upload.

**Pred (priklad):**
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
```

**Po (vysledok):**
```ini
board_build.flash_size = 2MB
board_upload.flash_size = 2MB
```

**Subor:** [platformio.ini](platformio.ini)

---

## Poznamky k finalnej verzii HSV
- Vsetky rele sekvencie funguju (ON/OFF postupne, ALL ON, ALL OFF).
- Vypisy vstupov a AI34 zostali pre diagnostiku.
- Pridane komentare pre zaciatocnika a mapy pinov.
