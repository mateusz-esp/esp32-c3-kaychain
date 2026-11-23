# ESP32-C3 Keychain

Animowany brelok oparty na ESP32-C3 z wyświetlaczem OLED 128x64 oraz modułem
AHT20 + BMP280. Oczy mrugają, zmieniają kierunek spojrzenia i animują różne
kształty. W górnej ramce wyświetlane są temperatura, wilgotność i ciśnienie.

## Sprzęt

- ESP32-C3 DevKitM-1
- OLED SSD1306 128x64 I2C
- Moduł AHT20 + BMP280 I2C (4 piny)

### Połączenia I2C

Wyświetlacz i moduł czujników korzystają ze wspólnej magistrali:

| Sygnał | ESP32-C3 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 6 |
| SCL | GPIO 7 |

Podłącz SDA obu modułów razem do GPIO 6, a SCL obu modułów razem do GPIO 7.
Typowe adresy I2C to `0x3C` dla OLED, `0x38` dla AHT20 oraz `0x76` lub `0x77`
dla BMP280.

## Budowanie i wgrywanie

Otwórz projekt w PlatformIO i uruchom:

```sh
pio run
pio run --target upload
pio device monitor
```

Serial Monitor jest skonfigurowany na 115200 baud. Odczyty czujników są
odświeżane co 2 sekundy.

## Zdjęcia

Zdjęcia projektu można dodać do katalogu [`images/`](images/). Gdy będą
dodane, umieść je tutaj w galerii.

## Licencja

Projekt jest udostępniony na licencji Apache-2.0. Szczegóły znajdują się w
pliku [`LICENSE`](LICENSE).
