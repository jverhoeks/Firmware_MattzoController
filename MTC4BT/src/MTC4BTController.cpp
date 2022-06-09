#include "MTC4BTController.h"
#include "MCStatusLed.h"
#include "MCLed.h"
#include "enums.h"
#include "log4MC.h"

// Blink duration in milliseconds. If all hubs if a loco are connected, its lights will blink for this duration.
const uint32_t BLINK_AT_CONNECT_DURATION_IN_MS = 3000;

// BLE scan duration in seconds. If the device isn't found within this timeframe the scan is aborted.
const uint32_t BLE_SCAN_DURATION_IN_SECONDS = 5;

// Duration between BLE discovery and connect attempts in seconds.
const uint32_t BLE_CONNECT_DELAY_IN_SECONDS = 3;

// Sets the watchdog timeout (0D &lt; timeout in 0.1 secs, 1 byte &gt;)
// The purpose of the watchdog is to stop driving in case of an application failure.
// Watchdog starts when the first DRIVE command is issued during a connection.
// Watchdog is stopped when all channels are either set to zero drive, or are braking.
// The value is saved to the persistent store.
// The recommended watchdog frequency is 0.2-0.5 seconds, but a smaller and many larger settings are also available.
// Writing a zero disables the watchdog.
// By default watchdog is set to 5, which means a 0.5 second timeout.
const int8_t WATCHDOG_TIMEOUT_IN_TENS_OF_SECONDS = 3;

MTC4BTController::MTC4BTController() : MController()
{
}

void MTC4BTController::Setup(MTC4BTConfiguration *config)
{
    // Keep controller configuration.
    _config = config;

    // Setup basic MC controller configuration.
    MController::Setup(_config);

    // Setup MTC4BT specific controller configuration.
    initLocomotives(config->Locomotives);

    // Initialize BLE client.
    log4MC::info("Setup: Initializing BLE...");
    NimBLEDevice::init("");

    // Configure BLE scanner.
    _scanner = NimBLEDevice::getScan();
    _scanner->setInterval(45);
    _scanner->setWindow(15);
    _scanner->setActiveScan(true);
    _hubScanner = new BLEHubScanner();

    // Start BLE device discovery task loop (will detect and connect to configured BLE devices).
    xTaskCreatePinnedToCore(this->discoveryLoop, "DiscoveryLoop", 3072, this, 1, NULL, 1);
}

void MTC4BTController::Loop()
{
    // Run the loop from the base MCController class (handles WiFi/MQTT connection monitoring and leds).
    MController::Loop();

    // Handle e-brake on all locomotives.
    for (BLELocomotive *loco : Locomotives)
    {
        loco->EmergencyBrake(GetEmergencyBrake());
    }
}

bool MTC4BTController::HasLocomotive(uint address)
{
    return getLocomotive(address);
}

void MTC4BTController::HandleSys(const bool ebrakeEnabled)
{
    // Update global e-brake status.
    SetEmergencyBrake(ebrakeEnabled);
}

void MTC4BTController::HandleLc(int locoAddress, int speed, int minSpeed, int maxSpeed, char *mode, bool dirForward)
{
    BLELocomotive *loco = getLocomotive(locoAddress);
    if (!loco)
    {
        // Not a loco under our control. Ignore message.
        log4MC::vlogf(LOG_DEBUG, "Ctrl: Loco with address '%u' is not under our control. Lc command ignored.", locoAddress);
        return;
    }

    // Calculate target speed percentage (as percentage if mode is "percent", or else as a percentage of max speed).
    int targetSpeedPerc = strcmp(mode, "percent") == 0 ? speed : (speed * maxSpeed) / 100;

    // Execute drive command.
    int8_t dirMultiplier = dirForward ? 1 : -1;
    loco->Drive(minSpeed, targetSpeedPerc * dirMultiplier);

    if (loco->GetAutoLightsEnabled())
    {
        // TODO: Determine lights on or off based on target motor speed percentage.
        // locos[i]->SetLights(speed != 0);
    }
}

void MTC4BTController::HandleFn(int locoAddress, MCFunction f, const bool on)
{
    BLELocomotive *loco = getLocomotive(locoAddress);
    if (!loco)
    {
        // Not a loco under our control. Ignore message.
        log4MC::vlogf(LOG_DEBUG, "Ctrl: Loco with address '%u' is not under our control. Fn command ignored.", locoAddress);
        return;
    }

    // Get applicable functions from loco.
    for (MCFunctionBinding *fn : loco->GetFn(f))
    {
        // Determine type of port.
        switch (fn->GetPortConfiguration()->GetPortType())
        {
        case PortType::EspPin:
        {
            // Handle function locally on the controller.
            MController::HandleFn(fn, on);
            break;
        }
        case PortType::BleHubChannel:
        {
            // Let loco handle the function.
            loco->HandleFn(fn, on);
            break;
        }
        }
    }
}

void MTC4BTController::discoveryLoop(void *parm)
{
    MTC4BTController *controller = (MTC4BTController *)parm;

    for (;;)
    {
        std::vector<BLEHub *> undiscoveredHubs;

        for (BLELocomotive *loco : controller->Locomotives)
        {
            if (!loco->IsEnabled() || loco->AllHubsConnected())
            {
                // Loco is not in use or all hubs are already connected. Skip to the next loco.
                continue;
            }

            for (BLEHub *hub : loco->Hubs)
            {
                if (!hub->IsEnabled())
                {
                    // Skip to the next Hub.
                    continue;
                }

                if (!hub->IsConnected())
                {
                    if (!hub->IsDiscovered())
                    {
                        // Hub not discovered yet, add to list of hubs to discover.
                        undiscoveredHubs.push_back(hub);
                    }

                    if (hub->IsDiscovered())
                    {
                        // Hub discovered, try to connect now.
                        if (!hub->Connect(WATCHDOG_TIMEOUT_IN_TENS_OF_SECONDS))
                        {
                            // Connect attempt failed. Will retry in next loop.
                            log4MC::warn("Loop: Connect failed. Will retry...");
                        }
                        else
                        {
                            log4MC::vlogf(LOG_INFO, "Loop: Connected to all hubs of loco '%s'.", loco->GetLocoName().c_str());

                            if (loco->AllHubsConnected())
                            {
                                if (controller->GetEmergencyBrake())
                                {
                                    // Pass current e-brake status from controller to loco.
                                    loco->EmergencyBrake(true);
                                }
                                else
                                {
                                    // Blink lights for a while when connected.
                                    loco->BlinkLights(BLINK_AT_CONNECT_DURATION_IN_MS);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (undiscoveredHubs.size() > 0)
        {
            // Start discovery for undiscovered hubs.
            controller->_hubScanner->StartDiscovery(controller->_scanner, undiscoveredHubs, BLE_SCAN_DURATION_IN_SECONDS);

            // Delay next discovery/connect attempts for a while, allowing the background tasks of already connected Hubs to send their periodic drive commands.
            delay(BLE_CONNECT_DELAY_IN_SECONDS * 1000 / portTICK_PERIOD_MS);
        }
    }
}

void MTC4BTController::initLocomotives(std::vector<BLELocomotiveConfiguration *> locoConfigs)
{
    for (BLELocomotiveConfiguration *locoConfig : locoConfigs)
    {
        // Keep an instance of the configured loco and pass it a reference to the controller.
        Locomotives.push_back(new BLELocomotive(locoConfig, this));
    }
}

BLELocomotive *MTC4BTController::getLocomotive(uint address)
{
    for (BLELocomotive *loco : Locomotives)
    {
        if (loco->GetLocoAddress() == address)
        {
            return loco;
        }
    }

    return nullptr;
}