

# Config Network

Edit -> `MattzoController_Network_Configuration.h`

```
const char* WIFI_SSID = "xxxxx";
const char* WIFI_PASSWORD = "xxxxx";
const char* MQTT_BROKER_IP = "192.168.1.179";
const int MQTT_BROKER_PORT = 1883;
```

# config MattzoController

The `MattzoLayoutController_Configuration.h` is the configuration of the controller, this maps pins /expends to RocRail ids.
The files `MattzoLayoutController_Configuration_xxxxx.h` are examples with configuration for other use.

For the switch and light control the following parts are important

```c
#define USE_PCA9685 true

// PCA9685 OE pin supported?
bool PCA9685_OE_PIN_INSTALLED = false;  // set to true if OE pin is connected (false if not)
uint8_t PCA9685_OE_PIN = D0;

// Number of chained PCA9685 port extenders
#define NUM_PCA9685s 1


// Number of servos
const int NUM_SERVOS = 4;

struct ServoConfiguration {
  // Digital output pins for switch servos (pins like D0, D1 etc. for ESP-8266 I/O pins, numbers like 0, 1 etc. for pins of the PCA9685)
  uint8_t pin;

  // Type of digital output pins for switch servos
  // 0   : pin on the ESP-8266
  // 0x40: port on the 1st PCA9685
  // 0x41: port on the 2nd PCA9685
  // 0x42: port on the 3rd PCA9685 etc.
  uint8_t pinType;

  // set to true if servo shall be detached from PWM signal a couple of seconds after usage
  // this feature is helpful to prevent blocking servo from burning down, it saves power and reduced servo flattering
  // for bascule bridges, the feature must be switched off!
  bool detachAfterUsage;
} servoConfiguration[NUM_SERVOS] =
{
  {
    .pin = 0,
    .pinType = 0x40,
    .detachAfterUsage = true
  },
  {
    .pin = 1,
    .pinType = 0x40,
    .detachAfterUsage = true
  },
  {
    .pin = 2,
    .pinType = 0x40,
    .detachAfterUsage = true
  },
  {
    .pin = 3,
    .pinType = 0x40,
    .detachAfterUsage = true
  }
};


// LED WIRING CONFIGURATION

// Number of LEDs
// LEDs are used in signals, level crossing lights or bascule bridge lights
// As an example, 2 LEDs are required for a light signal with 2 aspects
const int NUM_LEDS = 2;

struct LEDConfiguration {
  // Digital output pin for signal LED (pins like D0, D1 etc. for ESP-8266 I/O pins, numbers like 0, 1 etc. for pins of the PCA9685)
  uint8_t pin;

  // Type of digital output pins for led
  // 0   : LED output pin on the ESP-8266
  // 0x20: LED port on the 1st MCP23017
  // 0x21: LED port on the 2nd MCP23017
  // 0x22: LED port on the 3rd MCP23017 etc.
  // 0x40: LED port on the 1st PCA9685
  // 0x41: LED port on the 2nd PCA9685
  // 0x42: LED port on the 3rd PCA9685 etc.
  uint8_t pinType;
} ledConfiguration[NUM_LEDS] =
{
  {
    .pin = 8,
    .pinType = 0x40
  },
  {
    .pin = 9,
    .pinType = 0x40
  }
};

```

## Switch configuration to servo ports

rocrail ports start at 1

```c

// SWITCH CONFIGURATION

// Number of switches
const int NUM_SWITCHES = 4;

struct SwitchConfiguration {
  int rocRailPort;
  int servoIndex;

  // servo2Index: servo index for second servo (required for TrixBrix double slip switches). If unused, set to -1.
  // servo2Reverse: set to true if second servo shall be reversed
  int servo2Index;
  bool servo2Reverse;

  // feedback sensors
  // set triggerSensors to true if used
  // The first value in the sensorIndex is the index of the virtual sensor in the sensorConfiguration array for the "straight" sensor, the second is for "turnout".
  // Both referenced sensors must be virtual sensors.
  bool triggerSensors;
  int sensorIndex[2];
} switchConfiguration[NUM_SWITCHES] =
{
  {
    .rocRailPort = 1,
    .servoIndex = 0,
    .servo2Index = -1,
    .servo2Reverse = false,
    .triggerSensors = false,
    .sensorIndex = { -1, -1 }
  },
  {
    .rocRailPort = 2,
    .servoIndex = 1,
    .servo2Index = -1,
    .servo2Reverse = false,
    .triggerSensors = false,
    .sensorIndex = { -1, -1 }
  },
  {
    .rocRailPort = 3,
    .servoIndex = 2,
    .servo2Index = -1,
    .servo2Reverse = false,
    .triggerSensors = false,
    .sensorIndex = { -1, -1 }
  },
  {
    .rocRailPort = 4,
    .servoIndex = 3,
    .servo2Index = -1,
    .servo2Reverse = false,
    .triggerSensors = false,
    .sensorIndex = { -1, -1 }
  }
};

```

Signal configuration, in the simple case we have signals with 2 leds and no servo to move a flag.

```c
// SIGNAL CONFIGURATION

// Number of signals
const int NUM_SIGNALS = 1;
// Maximum number of signal aspects (e.g. red, green, yellow)
const int NUM_SIGNAL_ASPECTS = 2;
// Number of signal LEDs (usually equal to NUM_SIGNAL_ASPECTS)
const int NUM_SIGNAL_LEDS = 2;
// Maximum number of servos for form signals (e.g. one for the primary and another one for the secondary semaphore)
// If no form signals are used, just set to 0
const int NUM_SIGNAL_SERVOS = 0;

struct SignalConfiguration {
  // the port configured in Rocrail for an aspect
  // 0: aspect not supported by this signal
  int aspectRocrailPort[NUM_SIGNAL_ASPECTS];
  // if a LED is configured for this aspect (this is the usual case for light signals), this value represents the index of the LED in the SIGNALPORT_PIN array.
  // -1: no LED configured for this aspect
  int aspectLEDPort[NUM_SIGNAL_LEDS];
  // mappings between aspects and LEDs (often a diagonal matrix)
  // true: LED is mapped for this aspect
  bool aspectLEDMapping[NUM_SIGNAL_ASPECTS][NUM_SIGNAL_LEDS];
  // if a servo is configured for this signal (this is the usual case for form signals), this value represents the index of the servo in the SWITCHPORT_PIN array.
  // -1: no servo configured for this signal
  int servoIndex[NUM_SIGNAL_SERVOS];
  // the desired servo angle for the aspect (for form signals)
  int aspectServoAngle[NUM_SIGNAL_SERVOS][NUM_SIGNAL_ASPECTS];
} signalConfiguration[NUM_SIGNALS] =
{
  // signal 0: light signal with 2 aspects, controlled via Rocrail ports 1 and 2
  {
    .aspectRocrailPort = {1, 2},
    .aspectLEDPort = {0, 1},
    .aspectLEDMapping = {
      {true, false},
      {false, true}
    },
    .servoIndex = {},
    .aspectServoAngle = {}
  },
};
```




# Setting up build env

```
brew install pio
pio pkg install -p espressif8266
```

```
pio pkg install -e esp12e
```

Update platformio.ini and change the env to match the right esp model


# Start building

## Clean the device
`pio run -t cleanall`


## Build image and file system
`pio run -t buildfs`

## prepare the target device, clean 1 time
`pio run -t erase`

! Note only erase if you want to clear all. This resets the EEPROM ID which is used in RocRail. 
When you erase you need to reconfigure rocrail to match the right id for all sensors/switches


## Upload image 
`pio run -t upload`

This is the only command needed to update and upload the configuration

# monitor serial with usb

`pio device monitor -p 115200`

