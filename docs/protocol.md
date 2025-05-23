USB Control Protocol
====================

The Printalyzer Densitometer presents itself to a host computer as a USB CDC
device, which is essentially a virtual serial port. Through this interface,
it is possible to configure and control all aspects of the device.
This document covers that protocol.

## Data Formats

All command and response lines are terminated by a CRLF (`\r\n`).

### Command Format

Each command is sent in a standardized form:

`<TYPE><CATEGORY> <ACTION>[,<ARGS>]`
* **TYPE** values:
  * `S` - Set (change a value or state)
  * `G` - Get (return a value or state)
  * `I` - Invoke (perform some action on the system)
* **CATEGORY** values:
  * `S` - System (top-level system commands)
  * `M` - Measurement (commands related to density measurements)
  * `C` - Calibration (commands related to device calibration)
  * `D` - Diagnostics (general diagnostic commands)
* **ACTION** values are specific to each category, and documented in the following sections
* **ARGS** values are specific to each action, and not all actions may have them

### Response Format

Command responses typically fit a format that is a mirror of the command that
triggered them, except the `[,<ARGS>]` field is changed to represent
information specific to that particular command response.

The common formats for these response arguments are:
* `OK` - The command completed successfully, with no further information
* `ERR` - The command failed
* `NAK` - The command was either unrecognized or can't be processed in the current system state
* `A,B,C,D` - A comma-separated list of elements
  * Strings in this list may be quoted
  * Floating point values are returned in a little-endian hex encoded format
* A multi-line response as follows:
  ```
  TC ACTION,[[\r\n
  Line 1
  Line 2
  ...
  ]]\r\n  
  ```


### Density Format

Density readings are sent out-of-band whenever a reading is taken on the device,
and are not sent in response to commands. They follow the format of:

`<R/T/U><+/->#.##D`

An example of a density reading would be something like `R+0.20D`, `T+2.85D`, or `U+1.90D`

### Logging Format

Redirected log messages have a unique prefix of `L/`, where "L" is the logging level.
By looking for this pattern, they can be filtered out from the rest of the command
protocol. By default, log messages are not redirected out the USB CDC interface.

Expected log levels include: `A`, `E`, `W`, `I`, `D`, and `V`

## Commands

Note: Commands that could conflict with the local device user interface
may require that the device first be placed into "remote control" mode.
If sent outside of this mode, they may fail with a **NAK**. Commands of
this type are marked below.

Commands that lack a documented response format will return either `OK` or `ERR`.

### System Commands

* `GS V` - Get project name and version
  * Response: `GS V,<Project name>,<Version>`
* `GS B` - Get firmware build information
  * Response: `GS B,<Build date>,<Build describe>,<Checksum>`
* `GS DEV`  - Get device information
  * Response: `GS DEV,<HAL Version>,<MCU Device ID>,<MCU Revision ID>,<SysClock Frequency>`
* `GS RTOS` - Get FreeRTOS information
  * Response: `GS RTOS,<FreeRTOS Version>,<Heap Free>,<Heap Watermark>,<Task Count>`
* `GS UID`  - Get device unique ID
  * Response: `GS UID,<UID>`
* `GS ISEN` - Internal sensor readings
  * Response: `GS ISEN,<VDDA>,<MCU_Temp>,<Sensor_Temp>`
  * Note: Response elements have unit suffixes appended, so it looks like "3300mV,24.5C,22.0C"
* `IS REMOTE,n` - Invoke remote control mode (enable = 1, disable = 0)
  * Response: `IS REMOTE,n`
* `SS DISP,"text"` - Write the provided text to the display ***(remote mode)***
  * Note: Line breaks are sent as the literal text "\n"
    and backslashes are sent as the literal text "\\".
    Double-quote characters inside the string are not supported. ***(remote mode)***
* `SS DISP, n` - Set the display to be enabled or disabled (enable = 1, disable = 0)

### Measurement Commands

* `GM REFL` - Get last VIS reflection measurement
  * Response: `GM REFL,<D>`
* `GM TRAN` - Get last VIS transmission measurement
  * Response: `GM TRAN,<D>`
* `GM UVTR` - Get last UV transmission measurement
  * Response: `GM TRAN,<D>`
* `SM FORMAT,x` - Change measurement output format
  * Possible measurement formats are:
    * `BASIC` - The default format, which just includes the measurement mode
      and density value in a human-readable form, to 2 decimal places
    * `EXT` - Appends the density, zero offset, and gain adjusted basic count
      readings in the hex encoded floating point format
  * Note: The active format will revert to **BASIC** upon disconnect
* `SM UNCAL,x` - Allow measurements without target calibration (0=false, 1=true)
  * Note: This setting will revert to false upon disconnect

### Calibration Commands

* `IC GAIN` - Invoke the sensor gain calibration process ***(remote mode)***
  * This is a long process in which the device must be held closed.
    It will automatically abort if the hinge detect switch is released
    during the process.
  * The result of this process will be updated measurement light and
    sensor gain calibration values.
  * While running, there will be a series of `STATUS` responses followed by
    a single `OK` or `ERR` response.
  * Status is reported via the following responses:
    * `IC GAIN,STATUS,n,m` where n is an enumerated value as follows:
      * 0 = initializing
      * 1 = finding gain measurement brightness (m = gain level)
      * 2 = waiting between measurements
      * 3 = measuring gain level (m = gain being measured)
      * 4 = calibration process failed
      * 5 = calibration process complete
    * `IC GAIN,OK` - Gain calibration process is complete
    * `IC GAIN,ERR` - Gain calibration process has failed
* `GC GAIN` - Get sensor gain calibration values
  * Response: `GC GAIN,<G0>,<G1>,<G2>,<G3>,<G4>,<G5>,<G6>,<G7>,<G8>,<G9>`
  * Note: `<G0..9>` represent gain values, doubling with each increment, from 0.5x to 256x
* `SC GAIN,<G0>,<G1>,<G2>,<G3>,<G4>,<G5>,<G6>,<G7>,<G8>,<G9>` - Set sensor gain calibration values
* `GC VTEMP` - Get VIS sensor temperature calibration values
  * Response: `GC VTEMP,<B0_0>,<B0_1>,<B0_2>,<B1_0>,<B1_1>,<B1_2>,<B2_0>,<B2_1>,<B2_2>`
* `SC VTEMP,<B0_0>,<B0_1>,<B0_2>,<B1_0>,<B1_1>,<B1_2>,<B2_0>,<B2_1>,<B2_2>` - Set VIS sensor temperature calibration values
  * _Note: There is no on-device way to perform temperature calibration.
    It is performed using an externally controlled process that captures
    device measurements while the device is inside a thermal test chamber,
    does a series of calculations, then provides a set of coefficients
    that are loaded onto the device using this command._
* `GC UTEMP` - Get UV sensor temperature calibration values
  * Response: `GC VTEMP,<B0_0>,<B0_1>,<B0_2>,<B1_0>,<B1_1>,<B1_2>,<B2_0>,<B2_1>,<B2_2>`
* `SC UTEMP,<B0_0>,<B0_1>,<B0_2>,<B1_0>,<B1_1>,<B1_2>,<B2_0>,<B2_1>,<B2_2>` - Set UV sensor temperature calibration values
* `GC REFL` - Get VIS reflection density calibration values
  * Response: `GC REFL,<LD>,<LREADING>,<HD>,<HREADING>`
* `SC REFL,<LD>,<LREADING>,<HD>,<HREADING>` - Set VIS reflection density calibration values
  * The reading values are assumed to be in gain adjusted basic counts
* `GC TRAN` - Get VIS transmission density calibration values
  * Response: `GC TRAN,<LD>,<LREADING>,<HD>,<HREADING>`
* `SC TRAN,<LD>,<LREADING>,<HD>,<HREADING>` - Get VIS transmission density calibration values
  * The reading values are assumed to be in gain adjusted basic counts
  * Note: `<HD>` is always zero, and only included here for the sake of consistency
* `GC UVTR` - Get UV transmission density calibration values
  * Response: `GC UVTR,<LD>,<LREADING>,<HD>,<HREADING>`
* `SC UVTR,<LD>,<LREADING>,<HD>,<HREADING>` - Get UV transmission density calibration values
  * The reading values are assumed to be in gain adjusted basic counts
  * Note: `<HD>` is always zero, and only included here for the sake of consistency

### Diagnostic Commands

* `GD DISP` - Get display screenshot
  * Response is XBM data in the multi-line format described above
* `GD LMAX` -> Get maximum light duty cycle value
* `SD LR,nnn` -> Set VIS reflection light duty cycle (nnn/LMAX) ***(remote mode)***
  * Light sources are mutually exclusive. To turn all off, set any to 0.
    To turn on to full brightness, set to LMAX.
* `SD LT,nnn` -> Set VIS transmission light duty cycle (nnn/LMAX) ***(remote mode)***
  * Light sources are mutually exclusive. To turn all off, set any to 0.
    To turn on to full brightness, set to LMAX.
* `SD LTU,nnn` -> Set UV transmission light duty cycle (nnn/LMAX) ***(remote mode)***
  * Light sources are mutually exclusive. To turn all off, set any to 0.
    To turn on to full brightness, set to LMAX.
* `ID S,START` - Invoke sensor start ***(remote mode)***
  * When the sensor task is started via this diagnostic command, there is an
    implicit "Get Reading" command every time a result is available. Thus,
    results are periodically returned as long as the task is running.
  * Result format: `GD S,<DATA>,<GAIN>,<TIME>,<COUNT>`
* `ID S,STOP` - Invoke sensor stop ***(remote mode)***
* `SD S,MODE,<M>` - Set sensor spectrum measurement mode ***(remote mode)***
  * `<M>` - Sensor photodiode configuration
    * `0` - Default configuration
    * `1` - Visual (Photopic) mode
    * `2` - UV-A mode
* `SD S,CFG,g,t,c` - Set sensor gain (n = [0-9]), integration time (t = [0-2047]), and integration count (c = [0-2047]) ***(remote mode)***
* `SD AGCEN,c` - Enable automatic gain control with sample count (c = [0-2047]) ***(remote mode)***
* `SD AGCDIS` - Disable automatic gain control
* `ID READ,<L>,<nnn>,<M>,<G>,<T>,<C>` - Perform controlled sensor target read ***(remote mode)***
  * `<L>` - Measurement light source
    * `0` - Light off
    * `R` - VIS Reflection light, full power
    * `T` - VIS Transmission light, full power
    * `U` - UV Transmission light, full power
  * `<nnn>` - Measurement light duty cycle (nnn/LMAX)
    * 0 is off, LMAX is full brightness
  * `<M>` - Sensor photodiode configuration
    * `0` - Default configuration
    * `1` - Visual (Photopic) mode
    * `2` - UV-A mode
  * `<G>` - Sensor gain (0-9)
  * `<T>` - Sensor integration sample time (0-2047)
  * `<C>` - Sensor integration sample count (0-2047)
  * Result format: `ID READ,<DATA>` (Result is raw sensor counts)
  * _Note: This operation behaves similarly to a real measurement, synchronizing
    read light control to the measurement loop, and averaging across a number
    of readings. Unlike a real measurement it uses preconfigured gain and
    integration settings, rather than automatically adjusting them as part
    of the cycle, and returns raw sensor data. It is intended for use as part of
    device characterization routines where repeatable measurement conditions
    are necessary._
* `ID MEAS,<L>,<nnn>` - Perform normal density measurement read cycle ***(remote mode)***
  * `<L>` - Measurement light and sensor mode
    * `R` - VIS Reflection measurement
    * `T` - VIS Transmission measurement
    * `U` - UV Transmission measurement
  * `<nnn>` - Measurement light duty cycle (nnn/LMAX)
    * 0 is off, LMAX is full brightness
  * Result format: `ID MEAS,<DATA>` (Result is floating-point "Basic counts")
  * _Note: This operation behaves exactly the same as a real measurement,
    including automatic gain selection and repeatable timing. Unlike a
    real measurement, it provides lower-level sensor data instead of a user
    visible density result.
    It is intended for use as part of device characterization routines which
    need an automated way to take measurements without having to hard-code
    sensor parameters._
* `ID WIPE,<UID>,<CKSUM>` - Factory reset of configuration memory ***(remote mode)***
  * `<UIDw2>` is the last 4 bytes of the device UID, in hex format
  * `<CKSUM>` is the 4 byte checksum of the current firmware image, in hex format
  * _Note: After acknowledging this command, the device will perform the wipe
    and then reset itself. The connection will be lost in the process._
* `SD LOG,U` -> Set logging output to USB CDC device
* `SD LOG,D` -> Set logging output to debug port UART (default)
