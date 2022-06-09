# Setting up build env

```
brew install pio
pio pkg install -p espressif32
```

Depending on the platform / esp32 board

Find the right board, i have a wrover kit: esp-wrover-kit
`pio boards | grep ESP32`

```
pio pkg install -e esp-wrover-kit
```
Update platformio.ini and change the env


# Start building 

## Clean the device
`pio run -t cleanall`


## Build image and file system
`pio run -t buildfs`

## prepare the target device, clean 1 time
`pio run -t erase`



After device is working, use this commands to upload the firmware and update files

## Upload image 
`pio run -t upload`

## Upload Filesystem configuration
`pio run -t uploadfs`
