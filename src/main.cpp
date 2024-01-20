#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
#define CONFIG_ESP32_APPTRACE_ENABLE true

#include "secrets.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoHA.h>
#include <WS2812FX.h>
#include <cmath>

#define ARDUINOHA_DEBUG

#define LED_COUNT 100
#define LED_PIN 13
#define ANALOG_PIN 0

WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

WiFiClient client;
HADevice device("garlandtree1");
HAMqtt mqtt(client, device, 32);

HALight light("GarlandLight", HALight::BrightnessFeature | HALight::RGBFeature);
HANumber animationSpeed("GarlandAnimationSpeed");
HANumber rainbowSpeed("GarlandRainbowSpeed");
HANumber birghtnessColor("GarlandBrightness");
// Splitted because otherwise doesn't work
HASelect lightMode[5] = { HASelect("GarlandMode0"), HASelect("GarlandMode1"), HASelect("GarlandMode2"), HASelect("GarlandMode3"), HASelect("GarlandMode4")};
HASensor lightModeName("GarlandModeName");

float_t brightnessMultiplier = 1;

uint32_t calcColorWIthBrighness(HALight::RGBColor color) {
    unsigned char red = static_cast<unsigned char>(color.red * brightnessMultiplier);
    unsigned char green = static_cast<unsigned char>(color.green * brightnessMultiplier);
    unsigned char blue = static_cast<unsigned char>(color.blue * brightnessMultiplier);

    return red << 16 | green << 8 | blue;
}

void onRGBColorCommand(HALight::RGBColor color, HALight* sender) {
    ws2812fx.setColor(calcColorWIthBrighness(color));
    Serial.printf("Color %x\n", ws2812fx.getColor());
}

void onBrightnessMultiplier(HANumeric value, HANumber* sender) {
    brightnessMultiplier = static_cast<float_t>(value.getBaseValue())/100;
    Serial.printf("Brightness factor: %f\n", brightnessMultiplier);
    uint32_t hexColor = ws2812fx.getColor();
    HALight::RGBColor color = HALight::RGBColor((hexColor >> 16) & 0xFF, (hexColor >> 8) & 0xFF, hexColor & 0xFF);
    ws2812fx.setColor(calcColorWIthBrighness(color));
}

void onBrightnessCommand(uint8_t brightness, HALight* sender) {
    ws2812fx.setBrightness(brightness);
}

void onAnimationSpeedCommand(HANumeric speed, HANumber* sender) {
    ws2812fx.setSpeed(max(1u, static_cast<uint>(speed.getBaseValue() * 10)));
    Serial.printf("Animation speed: %d\n", ws2812fx.getSpeed());
}

void onSelectModeCommand(int8_t index, HASelect* sender) {

    if (index < 1) {
        return;
    }
    
    for (int i = 0; i < 5; i++) {
        if (sender == &lightMode[i]) {
            ws2812fx.setMode((index - 1) * 5 + i);
        } else {
            lightMode[i].setState(0, true);
        }
    }

    String modeName = ws2812fx.getModeName(ws2812fx.getMode());
    lightModeName.setValue(modeName.c_str());

    Serial.printf("Mode: %d %s\n", index, modeName);
}

// Rainbow colors

uint8_t colorId = 0;
uint animationSpeedDelay = 50;
bool isRainbowEnabled;

void onStateCommand(bool state, HALight* sender) {
    Serial.printf("Rainbow %d\n", state);
    isRainbowEnabled = !state;
}

void onRainbowSpeedCommand(HANumeric speed, HANumber* sender) {
    animationSpeedDelay = max(1u, static_cast<uint>(speed.getBaseValue() * 3));
    Serial.printf("Rainbow speed: %d\n", animationSpeedDelay);
}

void TaskPickRainbowColor(void *pvParameters);
void TaskWiFiConnect(void *pvParameters);

void setup() {
    Serial.begin(115200);

    // you don't need to verify return status
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // set device's details (optional)
    device.setName("Christmas garland");
    device.setSoftwareVersion("1.0.0");

    Serial.println("light color");
    light.setBrightnessScale(255);
    light.setName("Color");
    light.onStateCommand(onStateCommand);
    light.onBrightnessCommand(onBrightnessCommand); // optional
    light.onRGBColorCommand(onRGBColorCommand); // optional
    light.setOptimistic(true);
    light.setRetain(true);

    Serial.println("animation speed");
    animationSpeed.setMin(1);
    animationSpeed.setMax(100);
    animationSpeed.setStep(1);
    animationSpeed.setMode(HANumber::ModeSlider);
    animationSpeed.setName("Animation speed");
    animationSpeed.onCommand(onAnimationSpeedCommand);
    animationSpeed.setOptimistic(true);
    animationSpeed.setRetain(true);

    Serial.println("rainbow speed");
    rainbowSpeed.setMin(1);
    rainbowSpeed.setMax(100);
    rainbowSpeed.setStep(1);
    rainbowSpeed.setMode(HANumber::ModeSlider);
    rainbowSpeed.setName("Rainbow speed");
    rainbowSpeed.onCommand(onRainbowSpeedCommand);
    rainbowSpeed.setOptimistic(true);
    rainbowSpeed.setRetain(true);

    Serial.println("brightness multiplier");
    birghtnessColor.setMin(1);
    birghtnessColor.setMax(100);
    birghtnessColor.setStep(1);
    birghtnessColor.setMode(HANumber::ModeSlider);
    birghtnessColor.setName("Brightness color");
    birghtnessColor.onCommand(onBrightnessMultiplier);
    birghtnessColor.setOptimistic(true);
    birghtnessColor.setRetain(true);

    Serial.println("mode");
    String modeList[5] = {"None", "None", "None", "None", "None"};
    for (int i = 0; i < ws2812fx.getModeCount(); i++) {
        modeList[i%5] += ';';
        modeList[i%5] += ws2812fx.getModeName(i);
    }

    for (int i = 0; i < 5; i++) {
        lightMode[i].setOptions(modeList[i].c_str());
        lightMode[i].setName("Mode");
        lightMode[i].onCommand(onSelectModeCommand);
        lightMode[i].setOptimistic(true);
        lightMode[i].setRetain(false);
    }
    
    Serial.println("mode name");
    lightModeName.setName("Mode name");

    Serial.println("strip");
    ws2812fx.init();
    ws2812fx.setBrightness(200);
    ws2812fx.setSpeed(1500);
    ws2812fx.setColor(0x9de7d7);
    ws2812fx.setMode(FX_MODE_FLASH_SPARKLE);
    ws2812fx.start();

    Serial.println("broker");
    mqtt.begin(BROKER_ADDR);

    Serial.println("done!");

    xTaskCreate(
        TaskWiFiConnect, "Pick rainbow color", 4096, NULL, 2, NULL
    );
    
    xTaskCreate(
        TaskPickRainbowColor, "Pick rainbow color", 4096, NULL, 2, NULL
    );
}

void loop() {
    ws2812fx.service();
}

void TaskWiFiConnect(void *pvParameters) {
    while (true) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.print("Connecting to wifi");
        }
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            vTaskDelay(500); // waiting for the connection
        }

        mqtt.loop();
        vTaskDelay(500);
    }
}

void TaskPickRainbowColor(void *pvParameters) {

    while (true) {
        if (isRainbowEnabled) {
            uint32_t hexColor = ws2812fx.color_wheel(colorId);
            HALight::RGBColor color = HALight::RGBColor((hexColor >> 16) & 0xFF, (hexColor >> 8) & 0xFF, hexColor & 0xFF);
            ws2812fx.setColor(calcColorWIthBrighness(color));
            if (++colorId > 255) {
                colorId = 0;
            }
        }

        vTaskDelay(animationSpeedDelay);
    }
}

#pragma clang diagnostic pop