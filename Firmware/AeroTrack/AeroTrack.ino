//MAKE SURE TO SELECT ESP32 DEV BOARD TO COMPILE MR REVIWER

#define STEP_AZ   25
#define DIR_AZ    26
#define EN_AZ     27

#define STEP_EL   32
#define DIR_EL    33
#define EN_EL     14

#define CS_ENC_AZ 5
#define CS_ENC_EL 4

#define TMC_SERIAL   Serial2
#define TMC_RX_PIN   16
#define TMC_TX_PIN   17
#define R_SENSE      0.11f   // verify against your specific TMC2209 breakout

#include <TMCStepper.h>
#include <SPI.h>
#include "MT6835.h"

TMC2209Stepper driverAz(&TMC_SERIAL, R_SENSE, 0);
TMC2209Stepper driverEl(&TMC_SERIAL, R_SENSE, 1);

MT6835 encAz(CS_ENC_AZ, SPI);
MT6835 encEl(CS_ENC_EL, SPI);

// ---- Tuning parameters ----
const float DEADBAND_DEG      = 0.15f;   // stop stepping once within this error
const float EL_MIN_DEG        = 0.0f;    // mechanical safety limits for elevation
const float EL_MAX_DEG        = 90.0f;
const uint32_t MIN_STEP_US    = 400;     // fastest step pulse interval (large error)
const uint32_t MAX_STEP_US    = 4000;    // slowest step pulse interval (near target)
const float FULL_SPEED_ERR_DEG = 20.0f;  // error magnitude at which we hit MIN_STEP_US

// ---- Axis state ----
struct Axis {
    uint8_t stepPin, dirPin;
    float targetDeg = 0.0f;
    float currentDeg = 0.0f;
    bool haveReading = false;
    uint32_t lastStepMicros = 0;
};

Axis axisAz = {STEP_AZ, DIR_AZ};
Axis axisEl = {STEP_EL, DIR_EL};

// Shortest signed angular distance from a to b, result in (-180, 180]
float angleDiff(float a, float b) {
    float d = fmodf(b - a + 540.0f, 360.0f) - 180.0f;
    return d;
}

void configureDriver(TMC2209Stepper &drv) {
    drv.begin();
    drv.toff(4);
    drv.rms_current(800);       // mA - tune to your motor's rated current
    drv.microsteps(16);
    drv.pwm_autoscale(true);
    drv.en_spreadCycle(false);  // StealthChop for quiet tracking motion
}

void serviceAxis(Axis &ax, float measuredDeg, bool clampEl) {
    ax.currentDeg = measuredDeg;
    ax.haveReading = true;

    float target = ax.targetDeg;
    if (clampEl) target = constrain(target, EL_MIN_DEG, EL_MAX_DEG);

    float err = angleDiff(ax.currentDeg, target);

    // Safety: never drive elevation further past its mechanical limit
    if (clampEl) {
        if (ax.currentDeg >= EL_MAX_DEG && err > 0) err = 0;
        if (ax.currentDeg <= EL_MIN_DEG && err < 0) err = 0;
    }

    if (fabsf(err) < DEADBAND_DEG) return; // within deadband, hold position

    digitalWrite(ax.dirPin, err > 0 ? HIGH : LOW);

    // Scale step rate with error magnitude - faster when far, slower near target
    float mag = fminf(fabsf(err), FULL_SPEED_ERR_DEG) / FULL_SPEED_ERR_DEG;
    uint32_t interval = MAX_STEP_US - (uint32_t)(mag * (MAX_STEP_US - MIN_STEP_US));

    uint32_t now = micros();
    if (now - ax.lastStepMicros >= interval) {
        digitalWrite(ax.stepPin, HIGH);
        delayMicroseconds(3); // TMC2209 min step pulse width
        digitalWrite(ax.stepPin, LOW);
        ax.lastStepMicros = now;
    }
}

// ---- Serial command parsing ----
String serialBuf;

void handleCommand(const String &line) {
    int azIdx = line.indexOf("AZ:");
    int elIdx = line.indexOf("EL:");

    if (azIdx >= 0) {
        axisAz.targetDeg = line.substring(azIdx + 3).toFloat();
    }
    if (elIdx >= 0) {
        axisEl.targetDeg = constrain(line.substring(elIdx + 3).toFloat(), EL_MIN_DEG, EL_MAX_DEG);
    }

    Serial.printf("Target -> AZ:%.2f EL:%.2f\n", axisAz.targetDeg, axisEl.targetDeg);
}

void setup() {
    Serial.begin(115200);
    TMC_SERIAL.begin(115200, SERIAL_8N1, TMC_RX_PIN, TMC_TX_PIN);

    pinMode(EN_AZ, OUTPUT);
    pinMode(EN_EL, OUTPUT);
    digitalWrite(EN_AZ, LOW); // TMC2209 EN is active-low
    digitalWrite(EN_EL, LOW);

    pinMode(STEP_AZ, OUTPUT);
    pinMode(DIR_AZ, OUTPUT);
    pinMode(STEP_EL, OUTPUT);
    pinMode(DIR_EL, OUTPUT);

    configureDriver(driverAz);
    configureDriver(driverEl);

    SPI.begin(); // default VSPI pins: SCK=18, MOSI=23, MISO=19
    encAz.begin();
    encEl.begin();

    Serial.println("AeroTrack ready. Send commands like: AZ:135.0 EL:42.5");
}

uint32_t lastStatusPrint = 0;

void loop() {
    // Read serial commands
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            handleCommand(serialBuf);
            serialBuf = "";
        } else if (c != '\r') {
            serialBuf += c;
        }
    }

    // Read encoders
    float azDeg, elDeg;
    uint8_t azStatus, elStatus;
    bool azOk = encAz.readAngle(azDeg, azStatus);
    bool elOk = encEl.readAngle(elDeg, elStatus);

    if (azOk) serviceAxis(axisAz, azDeg, false);
    if (elOk) serviceAxis(axisEl, elDeg, true);

    // Periodic status print for tuning/debugging
    if (millis() - lastStatusPrint > 500) {
        lastStatusPrint = millis();
        Serial.printf("AZ: cur=%.2f tgt=%.2f (crc=%s)  EL: cur=%.2f tgt=%.2f (crc=%s)\n",
            axisAz.currentDeg, axisAz.targetDeg, azOk ? "ok" : "FAIL",
            axisEl.currentDeg, axisEl.targetDeg, elOk ? "ok" : "FAIL");
    }
}
