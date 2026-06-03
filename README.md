# SHT41 Industrial I/O (IIO) Linux Kernel Driver

[Final project for "Advanced-Operating-Systems" course of the master degree in "Computer Science and Engineering" held at Politecnico di Milano (2025/2026).]

Out-of-tree Linux kernel device driver module for the Sensirion SHT41 high-accuracy digital temperature and relative humidity sensor. This driver implements the official Linux Industrial I/O (IIO) subsystem framework, managing data transactions via the I2C physical bus.

## 📁 Repository Contents

* `sht41.c` / `sht41.h` - Core Industrial I/O driver source files.
* `Makefile` - Multi-target build file (supports generic host and mini-OS environment using during the course).
* `sht41_read` - POSIX-compliant user-space bash script for getting human readable value of temperature and relative humidity.


## 🛠️ Compilation Instructions

The included `Makefile` supports two entirely distinct workflows: building generically for your local machine or building for the custom containerized `mini-linux-labs` university course environment.

### Target 1: Generic Host Machine Compilation (Recommended for general testing)
If you are evaluating this code on any standard Linux machine (Ubuntu, Debian, Fedora, etc.) with kernel headers installed, you can compile the module cleanly out-of-tree for the current host kernel version without any external containers.

1. Clean the current workspace:
```console
$ make clean
```

2. Compile the driver generically:
```console
$ make generic
```

3. Load the resulting module into your host machine's kernel context:
```console
$ sudo insmod sht41.ko
```

4. Register the driver device
```code
$ echo sht41 0x44 > /sys/bus/i2c/devices/i2c-0/new_device
```

### Target 2: Advanced System Integration (AOS Laboratory Staging)
To build the driver specifically inside the isolated laboratory environment and automatically package it into the compressed virtual RAM disk filesystem image (`initramfs-busybox-amd64.cpio.gz`), use the integrated staging recipe.

*Note: This target assumes the workspace is running inside the project's multi-stage Docker compilation container containing the target kernel sources mapped at `/sources/linux`.*

1. Compile the driver and automatically stage the artifacts to the initramfs filesystem structures (`/staging/initramfs/fs/`):
```console
$ make clean
$ make prepare
$ make all
```

2. Force a full multi-tier micro-OS rebuilding sequence by triggering the master lab targets:
```console
$ make rebuild
```
3. Run the mini os environment and inside it load the module:
```console
$ insmod modules/sht41.ko
```
4. Then register the driver device
```console
$ echo sht41 0x44 > /sys/bus/i2c/devices/i2c-0/new_device
```

*Note: Once the system image is repackaged and launched via QEMU (handling the FTDI FT232H USB-to-I2C bridge passthrough constraints), the driver dynamically exposes the standardized Industrial I/O Sysfs interface.*


## 🚀 Execution & Verification


### 1. Unified Subsystem Interface
The final deployment of the driver registers a clean Virtual File System (VFS) interface under `/sys/bus/iio/devices/iio:device*/`
```code
/sys/bus/iio/devices/iio:device*/
    ├── heater_duration                 [RW] - Configures pulse duration ("1s", "0.1s")
    ├── heater_duration_available       [RO] - Lists permitted hardware duration states
    ├── heater_enable                   [WO] - One-shot execution trigger for the heater
    ├── heater_power                    [RW] - Configures element power ("200mW", "110mW", "20mW")
    ├── heater_power_available          [RO] - Lists permitted hardware power states
    ├── in_humidityrelative_offset      [RO] - Fixed-point integer channel offset
    ├── in_humidityrelative_raw         [RO] - Raw 16-bit relative humidity data from ADC
    ├── in_humidityrelative_scale       [RO] - Fixed-point integer fractional scale factor
    ├── in_temp_offset                  [RO] - Fixed-point integer channel offset
    ├── in_temp_raw                     [RO] - Raw 16-bit temperature data from ADC
    ├── in_temp_scale                   [RO] - Fixed-point integer fractional scale factor
    ├── name                            [RO] - Identifies driver instance ("sht41")
    ├── power/                          [DIR]- Standard kernel runtime power management nodes
    ├── precision                       [RW] - IIO shared channel precision ("high", "medium", "low")
    ├── precision_available             [RO] - Lists permitted hardware precision states
    ├── reset                           [WO] - Hardware soft-reset trigger node
    ├── serial_number                   [RO] - Read-only cached 32-bit unique chip identifier
    ├── subsystem/                      [LNK]- Symbolic link back to the central IIO bus core
    └── uevent                          [WO] - Kernel user-space event signaling interface

```
### 2. Usage

* Manipulate the internal precision sampling matrices on-demand by writing plain-text tokens (the driver transparently manages translation to binary hardware commands) and immediatly trigger a measurement:
```code
$ cat precision_available
    high medium low

$ echo medium > precision

```


* Result of the measurememt are store as raw ambient variables directly from the integrated hardware registers in `*_raw` `*_offset` and `*_scale` file:
```code
$ cat in_temp_raw
    27279

$ cat in_humidityrelative_raw
    30502
```

* Read the static, cached, unique 32-bit identifier string pre-fetched from the silicon die:
```code
$ cat serial_number
```


* Setup micro-heating element to burn off local moisture condensation and perform automatically a high precision measurements: 
```code
$ cat heater_duration_available
    1s  0.1s

$ echo 1s > heater_duration
```

```code
$ cat heater_power_available
    200mW  110mW  20mW

$ echo 200mW > heater_power
```
```code
$ echo 1 > heater_enable        # trigger heater and a high precision measurement 
```

* Issue a soft reset to sensor configuration:
```code
$ echo 1 > reset
```

### 4. Telemetry Scaling & Linux IIO ABI Formula
The driver exposes raw integer streams, fractional scale factors, and integer offsets via Sysfs according to strict Linux Industrial I/O specifications. To evaluate these metrics into standard physical values, user-space applications (such as our `sht41_read` script) implement the standardized Linux IIO ABI calculation matrix:

$$
Real Value = (Raw + Offset) * Scale
$$

Where:

- **Raw**: The direct unscaled register read from the sensor (`in_temp_raw` / `in_humidityrelative_raw`)

- **Offset**: The subsystem baseline coordinate adjustment (`in_temp_offset` / `in_humidityrelative_offset`)

- **Scale**: The fractional fixed-point translation multiplier (`in_temp_scale` / `in_humidityrelative_scale`)

### 5. Automated Fixed-Point Diagnostics

*NOTE: Only for the mini os lab environmen the proivided test bash script is compiled straight into the system execution `$PATH` and immediatly usable*

Execute the validation bash script to resolve raw registers, scales, and offsets using standardized IIO ABI mathematical formats and print user readable measurements:




```console
$ sht41_read

==================================================
SHT41 DIAGNOSTIC SCRIPT (IIO Interface)
Device Directory: /sys/bus/iio/devices/iio:device0
==================================================
TEMPERATURE:
  -> Raw Int:  27279
  -> Readable:  27.8435 °C

RELATIVE HUMIDITY:
  -> Raw Int:  30502
  -> Readable:  52.1782 %RH
==================================================
```