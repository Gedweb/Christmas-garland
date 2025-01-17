#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
#define CONFIG_ESP32_APPTRACE_ENABLE true

#include <WiFi.h>
#include "weight_selector.cpp"
#include "secrets.h"
#include <Arduino.h>
#include <ArduinoHA.h>
#include <WS2812FX.h>

#define ARDUINOHA_DEBUG

#define LED_COUNT 100
#define LED_PIN 13
#define ANALOG_PIN 0
#define ANIMATION_SCALE 512

WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

WiFiClient client;
HADevice device("garlandtree1");
HAMqtt mqtt(client, device, 32);

HALight lightColorHA("GarlandLight", HALight::BrightnessFeature | HALight::RGBFeature);
HANumber animationSpeedHA("GarlandAnimationSpeed");
HANumber rainbowSpeedHA("GarlandRainbowSpeed");
HANumber birghtnessColor("GarlandBrightness");
HANumber modeTimeoutHA("GarlandModeTimeout");
// Splitted because otherwise selector doesn't work
HASelect lightModeHA[5] = { HASelect("GarlandMode0"), HASelect("GarlandMode1"), HASelect("GarlandMode2"), HASelect("GarlandMode3"), HASelect("GarlandMode4")};
HASensor lightModeNameHA("GarlandModeName");

float_t brightnessMultiplier = 1;
uint modeTimeoutMs = 30000;

// Defining the probability of selecting a mode
int *modeRating() {

    static int rate[128];

    rate[FX_MODE_STATIC] = 2;
    rate[FX_MODE_BLINK] = 4;
    rate[FX_MODE_BREATH] = 2;
    rate[FX_MODE_COLOR_WIPE] = 5;
    rate[FX_MODE_COLOR_WIPE_INV] = 3;
    rate[FX_MODE_COLOR_WIPE_REV] = 4;
    rate[FX_MODE_COLOR_WIPE_REV_INV] = 3;
    rate[FX_MODE_COLOR_WIPE_RANDOM] = 5;
    rate[FX_MODE_RANDOM_COLOR] = 5;
    rate[FX_MODE_SINGLE_DYNAMIC] = 9;
    rate[FX_MODE_MULTI_DYNAMIC] = 7;
    rate[FX_MODE_RAINBOW] = 10;
    rate[FX_MODE_RAINBOW_CYCLE] = 10;
    rate[FX_MODE_SCAN] = 0;
    rate[FX_MODE_DUAL_SCAN] = 0;
    rate[FX_MODE_FADE] = 3;
    rate[FX_MODE_THEATER_CHASE] = 8;
    rate[FX_MODE_THEATER_CHASE_RAINBOW] = 6;
    rate[FX_MODE_RUNNING_LIGHTS] = 6;
    rate[FX_MODE_TWINKLE] = 3;
    rate[FX_MODE_TWINKLE_RANDOM] = 6;
    rate[FX_MODE_TWINKLE_FADE] = 6;
    rate[FX_MODE_TWINKLE_FADE_RANDOM] = 3;
    rate[FX_MODE_SPARKLE] = 1;
    rate[FX_MODE_FLASH_SPARKLE] = 5;
    rate[FX_MODE_HYPER_SPARKLE] = 6;
    rate[FX_MODE_STROBE] = 0;
    rate[FX_MODE_STROBE_RAINBOW] = 0;
    rate[FX_MODE_MULTI_STROBE] = 0;
    rate[FX_MODE_BLINK_RAINBOW] = 3;
    rate[FX_MODE_CHASE_WHITE] = 1;
    rate[FX_MODE_CHASE_COLOR] = 6;
    rate[FX_MODE_CHASE_RANDOM] = 7;
    rate[FX_MODE_CHASE_RAINBOW] = 6;
    rate[FX_MODE_CHASE_FLASH] = 0;
    rate[FX_MODE_CHASE_FLASH_RANDOM] = 2;
    rate[FX_MODE_CHASE_RAINBOW_WHITE] = 0;
    rate[FX_MODE_CHASE_BLACKOUT] = 0;
    rate[FX_MODE_CHASE_BLACKOUT_RAINBOW] = 8;
    rate[FX_MODE_COLOR_SWEEP_RANDOM] = 10;
    rate[FX_MODE_RUNNING_COLOR] = 4;
    rate[FX_MODE_RUNNING_RED_BLUE] = 7;
    rate[FX_MODE_RUNNING_RANDOM] = 8;
    rate[FX_MODE_LARSON_SCANNER] = 0;
    rate[FX_MODE_COMET] = 0;
    rate[FX_MODE_FIREWORKS] = 2;
    rate[FX_MODE_FIREWORKS_RANDOM] = 8;
    rate[FX_MODE_MERRY_CHRISTMAS] = 5;
    rate[FX_MODE_FIRE_FLICKER] = 0;
    rate[FX_MODE_FIRE_FLICKER_SOFT] = 0;
    rate[FX_MODE_FIRE_FLICKER_INTENSE] = 5;
    rate[FX_MODE_CIRCUS_COMBUSTUS] = 2;
    rate[FX_MODE_HALLOWEEN] = 5;
    rate[FX_MODE_BICOLOR_CHASE] = 0;
    rate[FX_MODE_TRICOLOR_CHASE] = 3;
    rate[FX_MODE_TWINKLEFOX] = 8;
    rate[FX_MODE_RAIN] = 9;
    rate[FX_MODE_BLOCK_DISSOLVE] = 3;
    rate[FX_MODE_ICU] = 0;
    rate[FX_MODE_DUAL_LARSON] = 0;
    rate[FX_MODE_RUNNING_RANDOM2] = 5;
    rate[FX_MODE_FILLER_UP] = 0;
    rate[FX_MODE_RAINBOW_LARSON] = 0;
    rate[FX_MODE_RAINBOW_FIREWORKS] = 1;
    rate[FX_MODE_TRIFADE] = 5;
    rate[FX_MODE_VU_METER] = 0;
    rate[FX_MODE_HEARTBEAT] = 0;
    rate[FX_MODE_BITS] = 0;
    rate[FX_MODE_MULTI_COMET] = 0;
    rate[FX_MODE_FLIPBOOK] = 0;
    rate[FX_MODE_POPCORN] = 0;
    rate[FX_MODE_OSCILLATOR] = 5;

    return rate;
}

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
    brightnessMultiplier = static_cast<float_t>(value.getBaseValue()) / 100;
    Serial.printf("Brightness factor: %f\n", brightnessMultiplier);
    uint32_t hexColor = ws2812fx.getColor();
    HALight::RGBColor color = HALight::RGBColor((hexColor >> 16) & 0xFF, (hexColor >> 8) & 0xFF, hexColor & 0xFF);
    ws2812fx.setColor(calcColorWIthBrighness(color));
}

void onBrightnessCommand(uint8_t brightness, HALight* sender) {
    ws2812fx.setBrightness(brightness);
}

void onAnimationSpeedCommand(HANumeric speed, HANumber* sender) {
    ws2812fx.setSpeed(ANIMATION_SCALE - log10(speed.getBaseValue()) * ANIMATION_SCALE / 2);
    Serial.printf("Animation speed: %d\n", ws2812fx.getSpeed());
}

void onSelectModeCommand(int8_t index, HASelect* sender) {

    if (index < 1) {
        return;
    }
    
    for (int i = 0; i < 5; i++) {
        if (sender == &lightModeHA[i]) {
            ws2812fx.setMode((index - 1) * 5 + i);
        } else {
            lightModeHA[i].setState(0, true);
        }
    }

    String modeName = ws2812fx.getModeName(ws2812fx.getMode());
    lightModeNameHA.setValue(modeName.c_str());

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

void onSetModeTimeoutCommand(HANumeric timeout, HANumber* sender) {
    modeTimeoutMs = max(5u, static_cast<uint>(timeout.getBaseValue())) * 1000u;
}

void TaskPickRainbowColor(void *pvParameters);
void TaskWiFiConnect(void *pvParameters);
void TaskAutoswitchMode(void *pvParameters);

void setup() {
    Serial.begin(115200);

    // you don't need to verify return status
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // set device's details (optional)
    device.setName("Christmas garland");
    device.setSoftwareVersion("1.0.0");

    Serial.println("light color");
    lightColorHA.setBrightnessScale(255);
    lightColorHA.setName("Color");
    lightColorHA.onStateCommand(onStateCommand);
    lightColorHA.onBrightnessCommand(onBrightnessCommand); // optional
    lightColorHA.onRGBColorCommand(onRGBColorCommand); // optional
    lightColorHA.setOptimistic(true);
    lightColorHA.setRetain(true);

    Serial.println("animation speed");
    animationSpeedHA.setMin(1);
    animationSpeedHA.setMax(100);
    animationSpeedHA.setStep(1);
    animationSpeedHA.setMode(HANumber::ModeSlider);
    animationSpeedHA.setName("Animation speed");
    animationSpeedHA.onCommand(onAnimationSpeedCommand);
    animationSpeedHA.setOptimistic(true);
    animationSpeedHA.setRetain(true);

    Serial.println("rainbow speed");
    rainbowSpeedHA.setMin(1);
    rainbowSpeedHA.setMax(100);
    rainbowSpeedHA.setStep(1);
    rainbowSpeedHA.setMode(HANumber::ModeSlider);
    rainbowSpeedHA.setName("Rainbow speed");
    rainbowSpeedHA.onCommand(onRainbowSpeedCommand);
    rainbowSpeedHA.setOptimistic(true);
    rainbowSpeedHA.setRetain(true);

    Serial.println("change mode timeout");
    modeTimeoutHA.setMax(300);
    modeTimeoutHA.setMin(5);
    modeTimeoutHA.setStep(5);
    modeTimeoutHA.setMode(HANumber::ModeSlider);
    modeTimeoutHA.setName("Mode timeout");
    modeTimeoutHA.onCommand(onSetModeTimeoutCommand);
    modeTimeoutHA.setOptimistic(true);
    modeTimeoutHA.setRetain(true);

    Serial.println("brightness multiplier");
    birghtnessColor.setMin(1);
    birghtnessColor.setMax(100);
    birghtnessColor.setStep(1);
    birghtnessColor.setMode(HANumber::ModeSlider);
    birghtnessColor.setName("Brightness color");
    birghtnessColor.onCommand(onBrightnessMultiplier);
    birghtnessColor.setOptimistic(true);
    birghtnessColor.setRetain(true);

    // selectColor.onBrightnessCommand

    Serial.println("mode");
    String modeList[5] = {"None", "None", "None", "None", "None"};
    for (int i = 0; i < ws2812fx.getModeCount(); i++) {
        modeList[i%5] += ';';
        modeList[i%5] += ws2812fx.getModeName(i);
    }

    for (int i = 0; i < 5; i++) {
        lightModeHA[i].setOptions(modeList[i].c_str());
        lightModeHA[i].setName("Mode");
        lightModeHA[i].onCommand(onSelectModeCommand);
        lightModeHA[i].setOptimistic(true);
        lightModeHA[i].setRetain(false);
    }
    
    Serial.println("mode name");
    lightModeNameHA.setName("Mode name");

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
        TaskWiFiConnect, "Check Wi-Fi connection", 4096, NULL, 2, NULL
    );
    
    xTaskCreate(
        TaskPickRainbowColor, "Pick rainbow color", 4096, NULL, 2, NULL
    );
    
    xTaskCreate(
        TaskAutoswitchMode, "Pick random mode by weight", 4096, NULL, 3, NULL
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
            vTaskDelay(100); // waiting for the connection
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

void TaskAutoswitchMode(void *pvParameters) {
    WeightedRandomSelector selector(modeRating(), ws2812fx.getModeCount());
    while (true)
    {
        auto mode = selector.getRandomItem();
        ws2812fx.setMode(mode);

        String modeName = ws2812fx.getModeName(ws2812fx.getMode());
        lightModeNameHA.setValue(modeName.c_str());
        // Serial.printf("%s - %d\n", modeName.c_str(), modeRating()[mode]);

        vTaskDelay(modeTimeoutMs);
    }
    
}

#pragma clang diagnostic pop