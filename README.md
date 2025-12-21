# F-UVC-WaterBottle

# UVC Bottle Hardware - ECAD

## Sponsorship

PCB fabrication for this prototype was kindly sponsored by [PCBWay](https://www.pcbway.com/), who covered the cost of the boards and DHL shipping via store credit. This project write-up and repository reflect my own design decisions and experience using their service.

## [Summary/Explanation]


# UVC Bottle Hardware - MCAD

## [Summary/Explanation]

# UVC Bottle Firmware – STM32L031K6

Firmware for a UVC water bottle prototype built around an STM32L031K6 (NUCLEO-L031K6 or equivalent MCU on a custom PCB).  
The code controls a UVC LED driver (via a high-side load switch), enforces lid-closed safety using a reed switch,  
and exposes two timed dose cycles with a single push button and an RGB status LED.

## Behavior

- **Single tap** on the button  
  → Starts a **short UVC cycle (1 minute)**.  
  → RGB LED shows a **warm amber** color (R ≈ 75%, G ≈ 20%, B = 0).

- **Double tap** (two taps within 400 ms)  
  → Starts a **long UVC cycle (3 minutes)**.  
  → RGB LED shows **blue** (B ≈ 75%, R = 0, G = 0).

- **Tap during an active cycle**  
  → Cancels the current cycle immediately.  
  → Load switch is turned OFF, RGB LED returns to **idle (off)**.

- **Reed switch safety (lid detection)**  
  - Reed switch is wired so that **LOW = lid closed / magnet present (safe)**.  
  - If the reed switch ever indicates **lid open** while a cycle is running:
    - UVC load is turned **OFF**.
    - Any pending tap state is cleared.
    - Cycle mode resets to **none**.
    - RGB LED returns to **idle (off)**.

- **Auto-stop**
  - Each cycle tracks its own **end time** (1 or 3 minutes).
  - When the cycle’s timer expires:
    - UVC load is turned **OFF**.
    - Cycle mode resets to **none**.
    - Pending tap state is cleared.
    - RGB LED goes back to **idle (off)**.

- **LED states (summary)**
  - **Idle / no cycle**: RGB LED **off**  
  - **Short cycle**: **amber** (mixed R+G)  
  - **Long cycle**: **blue**  
  - **Error state**: reserved for future use (currently just mapped to **red** in code, but not yet invoked)

## Hardware Assumptions

This firmware assumes (matching our prototype):

- **MCU**: STM32L031K6  
  - Either on a **NUCLEO-L031K6** dev board wired into the system,  
    or soldered as an MCU footprint on a custom PCB with equivalent pinout.

- **Power Path**
  - **Battery / input management**: TI **BQ24074** (USB / DC / solar input + Li-ion charging and power-path).
  - **3.3 V rail**: **MCP1700** LDO providing a regulated 3.3 V rail for the MCU.
  - **UVC LED driver branch**:
    - High-side load switch: **TPS22918**.
    - The UVC LED driver board (e.g., IO Rodeo UVC board) is powered from the TPS22918 output.
    - The firmware drives the **Control_Pin** GPIO to enable/disable this branch.

- **Inputs**
  - **Reed switch** on `REED_SW_Pin` (configured as input with pull-up):  
    - **LOW** → lid closed, safe to run UVC.  
    - **HIGH** → lid open, unsafe, force UVC off.
  - **User button (B2)** on `B2_Pin` (external interrupt, falling edge):  
    - Generates the single / double-tap events that control the cycles.

- **Outputs**
  - **RGB status LED**, common-anode, on STM32 **TIM2 PWM** channels:  
    - `PA3` → TIM2 CH4 → **RED**  
    - `PA1` → TIM2 CH2 → **GREEN**  
    - `PA0` → TIM2 CH1 → **BLUE**  
  - **UVC load switch control** on `Control_Pin` (GPIO output):  
    - `LOW` → load **OFF**  
    - `HIGH` → load **ON** (UVC driver powered)

> Note: SPI1 and some GPIO pins (RST, CS, DC) are still configured in the CubeMX project,
> but the associated OLED code has been removed. They are currently unused / reserved for
> future debug or display features and do not affect UVC functionality.

## Build & Flash (STM32CubeIDE)

- Toolchain: **STM32CubeIDE 1.19.x** (or compatible)
- MCU target: **STM32L031K6Tx**

To rebuild and flash:

1. **Clone** this repository.
2. In **STM32CubeIDE**, go to  
   `File → Import… → Existing Projects into Workspace…`  
   and select the project folder.
3. Build the `Debug` or `Release` configuration.
4. Connect an ST-LINK (onboard NUCLEO ST-LINK or external dongle).
5. Click the **Run / Debug** button in CubeIDE to flash the firmware.

Once flashed, power the board from the regulated **3.3 V rail** (MCP1700 output)  
or via the NUCLEO board’s standard power path, wire up the reed switch, button,  
RGB LED, and TPS22918 load switch, and the behavior above should match.
