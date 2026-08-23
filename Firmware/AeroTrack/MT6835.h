// MT6835.h - Minimal driver for MagnTek MT6835 21-bit SPI magnetic encoder
// Protocol reference: MT6835 Datasheet Rev.1.3 (2022.12), Section 7.6
//
// SPI mode: MODE3 (CPOL=1, CPHA=1)
// Burst angle read command: 0b1010, register address 0x003
// Frame: [C3 C2 C1 C0 A11..A0] (16 bits header) then 4 data bytes:
//   0x003 = ANGLE[20:13]
//   0x004 = ANGLE[12:5]
//   0x005 = ANGLE[4:0] | STATUS[2:0]
//   0x006 = CRC[7:0]  (CRC-8, poly x^8+x^2+x+1 = 0x07, over the 24 angle+status bits)
//
// NOTE on CRC init value: the datasheet specifies the polynomial but not the
// shift-register initial value explicitly. This implementation assumes init=0x00,
// which is the common default for this polynomial family. If crcOk is false on
// every read despite a stable, correctly-wired sensor, that init assumption is
// the first thing to re-check against a logic analyzer capture.

#pragma once
#include <SPI.h>

class MT6835 {
public:
    MT6835(uint8_t csPin, SPIClass &spiBus, uint32_t spiHz = 1000000)
        : _cs(csPin), _spi(spiBus), _hz(spiHz) {}

    void begin() {
        pinMode(_cs, OUTPUT);
        digitalWrite(_cs, HIGH);
    }

    // Reads the current angle. Returns true if the CRC check passed.
    // angleDeg: 0.0 - 360.0 (only valid if this call returns true)
    // status: STATUS[2:0] bits - bit0 overspeed, bit1 weak field, bit2 undervoltage
    bool readAngle(float &angleDeg, uint8_t &status) {
        uint8_t buf[4]; // will hold data from 0x003, 0x004, 0x005, 0x006

        SPISettings settings(_hz, MSBFIRST, SPI_MODE3);
        _spi.beginTransaction(settings);
        digitalWrite(_cs, LOW);

        // Header: command=1010, address=0x003 -> 16 bits total
        // byte0 = C3 C2 C1 C0 A11 A10 A9 A8 = 1010 0000 = 0xA0
        // byte1 = A7 A6 A5 A4 A3 A2 A1 A0   = 0000 0011 = 0x03
        _spi.transfer(0xA0);
        _spi.transfer(0x03);

        // Burst-clock out the 4 data bytes (0x003..0x006)
        for (int i = 0; i < 4; i++) {
            buf[i] = _spi.transfer(0x00);
        }

        digitalWrite(_cs, HIGH);
        _spi.endTransaction();

        uint8_t angleHi   = buf[0]; // ANGLE[20:13]
        uint8_t angleMid  = buf[1]; // ANGLE[12:5]
        uint8_t angleLoSt = buf[2]; // ANGLE[4:0] | STATUS[2:0]
        uint8_t crcRx     = buf[3];

        status = angleLoSt & 0x07;

        uint32_t angle21 = ((uint32_t)angleHi << 13) |
                            ((uint32_t)angleMid << 5) |
                            ((uint32_t)(angleLoSt >> 3) & 0x1F);

        angleDeg = (angle21 * 360.0f) / 2097152.0f; // 2^21

        uint8_t crcCalc = crc8(angleHi, angleMid, angleLoSt);
        return crcCalc == crcRx;
    }

private:
    uint8_t _cs;
    SPIClass &_spi;
    uint32_t _hz;

    // CRC-8, polynomial x^8+x^2+x+1 (0x07), MSB-first, init = 0x00
    static uint8_t crc8(uint8_t b0, uint8_t b1, uint8_t b2) {
        uint8_t data[3] = {b0, b1, b2};
        uint8_t crc = 0x00;
        for (int i = 0; i < 3; i++) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; bit++) {
                if (crc & 0x80) crc = (crc << 1) ^ 0x07;
                else crc <<= 1;
            }
        }
        return crc;
    }
};
