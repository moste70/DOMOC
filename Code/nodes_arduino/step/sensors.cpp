#include "sensors.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_SHT31.h>

static Adafruit_SHT31 sht31;

bool sensors_init() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    return sht31.begin(0x44);
}

bool sensors_read(float &temp_c, float &humidity_rh) {
    temp_c      = sht31.readTemperature();
    humidity_rh = sht31.readHumidity();
    return !isnan(temp_c) && !isnan(humidity_rh);
}
