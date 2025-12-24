# F-UVC Water Bottle – STM32L031 Firmware

Firmware for a prototype UVC water bottle controller built around an **STM32L031K6** MCU (typically on a **NUCLEO-L031K6** dev board or an equivalent custom PCB).

> ⚠️ **Safety warning – read first**
>
> - This firmware is part of a **UV-C water disinfection prototype**.
> - UV-C is hazardous to **eyes and skin**.
> - Do **not** power any UV-C LEDs without a proper enclosure, interlocks, and safety testing.
> - This code is provided for **educational and prototyping** purposes only. Do **not** rely on it to produce safe drinking water.

---

## Behavior (user-facing)

The firmware implements a simple tap-based UI and a lid interlock:

- **Single tap** on the button  
  - Starts a **short UV-C cycle (~1 minute)**  
  - UV-C load switch is **enabled**  
  - RGB status LED shows **amber** (R + G)

- **Double tap** (two taps within ~400 ms)  
  - Starts a **long UV-C cycle (~3 minutes)**  
  - UV-C load switch is **enabled**  
  - RGB status LED shows **blue**

- **Tap during an active cycle**  
  - **Cancels** the current cycle immediately  
  - UV-C load switch turns **OFF**  
  - RGB LED returns to **idle (off)**

- **Lid safety (reed switch interlock)**  
  - Reed switch is wired so **LOW = lid closed / magnet present (safe)**  
  - **HIGH = lid open (unsafe)**  
  - If the firmware ever detects **lid open** while a cycle is active, it will:
    - Turn the UV-C load switch **OFF**
    - **Abort** the running cycle
    - Clear any pending tap state
    - Block new cycles until the lid is closed again (reed back LOW)

---

## Hardware assumptions (logical)

This firmware expects a hardware stack roughly matching:

- **MCU**: STM32L031K6 (NUCLEO-L031K6 or equivalent custom board)
- **UV-C branch**: UV-C LED driver powered through a **high-side load switch**
- **Lid interlock**: **reed switch + magnet** detecting lid-closed state
- **UI**:  
  - 1 × **momentary push button** (active-low, interrupt-driven)  
  - 1 × **RGB LED** driven from TIM2 PWM channels

Exact pin names (e.g., `REED_SW_Pin`, `B2_Pin`, `Control_Pin`, RGB LED pins) are defined in the CubeMX-generated project and should be kept in sync with your hardware.

---

## Build & flash (STM32CubeIDE)

**Toolchain:** STM32CubeIDE  
**Target:** STM32L031K6Tx

1. Clone the repository and navigate to the `Software/` directory.
2. In **STM32CubeIDE**, go to  
   `File → Import… → Existing Projects into Workspace…`
3. Select the `Software` folder (or the STM32 project folder inside it) as the **root directory**.
4. Import the project and build the **Debug** (or **Release**) configuration.
5. Connect a **NUCLEO-L031K6** (onboard ST-LINK) or an external ST-LINK to your custom board.
6. Use the provided **Debug/Run configuration** (or create your own) to flash the firmware.

This project uses the **STM32 HAL** and the CubeMX-generated linker script  
`STM32L031K6TX_FLASH.ld`.
