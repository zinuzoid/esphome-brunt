import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import cover, sensor, uart
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
)

from . import brunt_ns

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor"]

BruntCover = brunt_ns.class_("BruntCover", cover.Cover, cg.Component, uart.UARTDevice)

CONF_UP_PIN = "up_pin"
CONF_DOWN_PIN = "down_pin"
CONF_PRESS_DURATION = "press_duration"
CONF_STOP_LATENCY = "stop_latency"
CONF_MOTOR_LOAD = "motor_load"
CONF_ENCODER = "encoder"

if hasattr(cover, "cover_schema"):
    _BASE = cover.cover_schema(BruntCover)
else:  # older ESPHome
    _BASE = cover.COVER_SCHEMA.extend({cv.GenerateID(): cv.declare_id(BruntCover)})

CONFIG_SCHEMA = (
    _BASE.extend(
        {
            cv.Required(CONF_UP_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_DOWN_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_PRESS_DURATION, default="200ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_STOP_LATENCY, default="100ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MOTOR_LOAD): sensor.sensor_schema(
                accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT
            ),
            cv.Optional(CONF_ENCODER): sensor.sensor_schema(
                accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "brunt", baud_rate=9600, require_rx=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
    await uart.register_uart_device(var, config)

    up = await cg.gpio_pin_expression(config[CONF_UP_PIN])
    cg.add(var.set_up_pin(up))
    down = await cg.gpio_pin_expression(config[CONF_DOWN_PIN])
    cg.add(var.set_down_pin(down))
    cg.add(var.set_press_duration(config[CONF_PRESS_DURATION]))
    cg.add(var.set_stop_latency(config[CONF_STOP_LATENCY]))

    if CONF_MOTOR_LOAD in config:
        s = await sensor.new_sensor(config[CONF_MOTOR_LOAD])
        cg.add(var.set_motor_load_sensor(s))
    if CONF_ENCODER in config:
        s = await sensor.new_sensor(config[CONF_ENCODER])
        cg.add(var.set_encoder_sensor(s))
