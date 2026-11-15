APP_NAME := tests
ARDUINO_LIBS := AUnit
EPOXY_DUINO_DIR ?= ../EpoxyDuino
AUNIT_DIR ?= ../AUnit
ARDUINOJSON_DIR ?= ../ArduinoJson
DRIVERS_DIR ?= ../ESP-Environment-Drivers

CXXFLAGS := -Wextra -Wall -std=gnu++17 -pthread -g
LDFLAGS += -pthread

# stubs/ stands in for the ESP32 core's file systems; the drivers' FakeWire
# stands in for the I2C bus (I2CS_FAKE_WIRE).
CPPFLAGS += -I. -Istubs -I.. -I$(AUNIT_DIR)/src -I$(ARDUINOJSON_DIR)/src
CPPFLAGS += -I$(DRIVERS_DIR)/src -I$(DRIVERS_DIR)/tests -DI2CS_FAKE_WIRE
CPPFLAGS += -MMD -MP

# Firmware sources under test. The test files in this directory are picked up automatically.
OBJS += ../io.o ../SensorContainer.o ../ReadingLog.o ../StationCache.o
OBJS += $(DRIVERS_DIR)/src/I2CManager.o $(DRIVERS_DIR)/src/I2CSensor.o
OBJS += $(DRIVERS_DIR)/src/SHT3X/SHT31D.o $(DRIVERS_DIR)/src/BMP3XX/BMP3XX.o

include $(EPOXY_DUINO_DIR)/EpoxyDuino.mk

-include $(OBJS:.o=.d)
