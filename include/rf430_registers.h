#pragma once
#include <Arduino.h>

namespace RF430 {
constexpr uint8_t BLOCK_CONTROL = 0;
constexpr uint8_t BLOCK_ADC_CONFIG = 2;
constexpr uint8_t BLOCK_DATA_START = 9;
constexpr uint8_t BLOCK_SIZE = 8;
constexpr uint8_t RAW_MASK = 0x3F;
constexpr uint16_t RAW_MAX = 0x3FFF;

constexpr uint8_t GENERAL_START = 1U << 0;
constexpr uint8_t GENERAL_RESET = 1U << 7;
constexpr uint8_t STATUS_STATE_MASK = 0x03;
constexpr uint8_t SENSOR_ADC1 = 1U << 0;
constexpr uint8_t SENSOR_ADC2 = 1U << 1;
constexpr uint8_t SENSOR_ADC0 = 1U << 2;
constexpr uint8_t SENSOR_INTERNAL = 1U << 3;
constexpr uint8_t INTERRUPT_INFINITE = 1U << 0;
constexpr uint8_t ERROR_USING_THERMISTOR = 1U << 6;

enum class SamplingState : uint8_t { Idle = 0, Sampling = 1, DataReady = 2, Error = 3 };
enum class AdcGain : uint8_t { X1 = 0, X2 = 1, X4 = 2, X8 = 3 };
enum class AdcFilter : uint8_t { CIC = 0, MovingAverage = 1 };
enum class AdcReference : uint8_t { SVSS = 0, AVSS = 1 };
enum class ChannelRole : uint8_t { Ignore = 0, Sensor = 1, Reference = 2 };
}
