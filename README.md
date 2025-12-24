# F-UVC-WaterBottle (Prototype)

**F-UVC-WaterBottle** is an open-source prototype for a **portable, bottle-scale water treatment concept** that stages:
1) **Filtration** (to reduce turbidity/particulates and improve UV effectiveness), and  
2) **UV-C LED exposure** (to target microorganisms in the treated volume).

This repository currently focuses on the **STM32 firmware bring-up** for a lid-integrated UV-C control system (timed cycles + safety interlock). Mechanical and electrical design artifacts will be added/expanded as the hardware matures.

> ⚠️ **Safety / Disclaimer (read first)**  
> - UV-C is hazardous to eyes/skin. This is a prototype and is **not a certified disinfection device**.  
> - Battery-powered systems can fail dangerously if miswired (shorts/overcurrent/overcharge).  
> - This project is for engineering documentation and learning; **do not rely on it to produce safe drinking water**.

---

## Repository layout

- **/Software/** — STM32L031 firmware project (STM32CubeIDE)  
- **/Hardware/** — placeholders / in-progress hardware artifacts  
  - **/Hardware/ECAD/** — (planned) schematics + PCB files, power path, MCU + interlocks  
  - **/Hardware/MCAD/** — (planned) CAD for bottle geometry, lid enclosure, interlock placement

---

## System overview (high level)

### Functional blocks
- **Power path + charging:** Li-ion battery + power-path/charging management (USB / DC / solar input).
- **3.3 V logic rail:** regulated supply for STM32L031K6.
- **UV-C branch:** UV-C LED driver powered through a **high-side load switch** controlled by the MCU.
- **Safety interlock:** **reed switch + magnet** lid-closed detection (UV-C is disabled if lid-open is detected).
- **User interface:** a single button selects a timed cycle; an RGB LED indicates state.

### Why a gated UV-C branch?
The UV-C driver is treated as a **separately power-gated subsystem**. The MCU controls a load switch so that:
- UV-C power is **OFF by default** at boot,
- UV-C power can be **dropped immediately** on fault / lid-open,
- inrush can be shaped (soft-start) to avoid brownouts.

---

## Firmware: STM32L031K6

Firmware for a UV-C water bottle prototype built around an **STM32L031K6** (NUCLEO-L031K6 or equivalent MCU on a custom PCB).  
The code:
- controls a UV-C driver branch via a **high-side load switch**,
- enforces lid-closed safety via a **reed switch**,
- implements **two timed UV-C cycles** using a **single push button** and an **RGB status LED**.

### Behavior (user-facing)

**Single tap**
- Starts a **short UV-C cycle (1 minute)**  
- RGB LED shows **amber** (warm: R + G)

**Double tap** (two taps within ~400 ms)
- Starts a **long UV-C cycle (3 minutes)**  
- RGB LED shows **blue**

**Tap during an active cycle**
- Cancels the current cycle immediately  
- UV-C branch is turned OFF and LED returns to idle

### Reed switch safety (lid detection)

The reed switch is wired/configured so that:

- **LOW = lid closed / magnet present (safe to run)**
- **HIGH = lid open (unsafe)**

If the firmware ever detects **lid open while a cycle is active**, it will:
- turn the UV-C branch **OFF immediately**
- clear pending tap state
- reset the cycle mode to none
- return LED to **idle (off)**

### Auto-stop

Each cycle tracks its own end time. When the timer expires:
- UV-C branch is turned OFF
- cycle mode resets to none
- pending tap state is cleared
- LED returns to idle

### LED states (summary)
- **Idle:** RGB LED off  
- **Short cycle:** amber (R + G)  
- **Long cycle:** blue  
- **Error:** reserved for future use (code may map “error” to red, but it is not yet fully invoked)

---

## Hardware assumptions (prototype target)

This firmware assumes a hardware stack similar to:

### MCU
- **STM32L031K6**
  - Either on a **NUCLEO-L031K6** dev board during bring-up, or
  - Soldered on a custom PCB with equivalent pinout.

### Power path (concept)
- **Charging + power-path:** TI **BQ24074** class device / module (USB/DC/solar input + Li-ion charging + load sharing)
- **3.3 V regulation:** **MCP1700** LDO (or equivalent)
- **UV-C branch gating:** **TPS22918** high-side load switch (or equivalent)
- **UV-C LED driver board:** a constant-current UV-C driver module powered from the gated branch (varies by prototype revision)

> Note: pin mapping and exact rails depend on the specific revision. The firmware documents the *logic expectations* (reed switch polarity, enable polarity, etc.).

---

## I/O and pin mapping (firmware expectations)

### Inputs
- **Reed switch** on `REED_SW_Pin` (input with pull-up)
  - **LOW → lid closed (safe)**
  - **HIGH → lid open (force UV-C off)**
- **User button** on `B2_Pin` (external interrupt, falling edge)
  - firmware expects an **active-low press** event for tap timing

### Outputs
- **UV-C load switch control** on `Control_Pin` (GPIO output)
  - **LOW → UV-C branch OFF**
  - **HIGH → UV-C branch ON**
- **RGB status LED** driven by TIM2 PWM channels (common-anode assumed)
  - `PA3 → TIM2 CH4 → RED`
  - `PA1 → TIM2 CH2 → GREEN`
  - `PA0 → TIM2 CH1 → BLUE`

### Reserved / unused
Some pins may remain configured in CubeMX (e.g., SPI1 + OLED-related GPIO) even if the display code is not currently included. These are reserved for future debug/UI and do not affect UV-C control behavior.

---

## Build & flash (STM32CubeIDE)

**Toolchain:** STM32CubeIDE (tested with 1.19.x; nearby versions should work)  
**Target MCU:** STM32L031K6Tx

### Steps
1. Clone the repository.
2. Open **STM32CubeIDE** → **File → Import… → Existing Projects into Workspace…**
3. Select the firmware project under `/Software/`
4. Build **Debug** or **Release**
5. Connect **ST-LINK**
   - NUCLEO onboard ST-LINK or external dongle
6. Click **Run** or **Debug** to flash

After flashing:
- Power the MCU from a **regulated 3.3 V rail** (or standard NUCLEO power path during early bring-up),
- Connect the reed switch, button, RGB LED, and load-switch control line,
- Verify that behavior matches the “Behavior” section above.

---
## Sponsorship (PCB fabrication)

<p align="center">
  <img
    src="https://camo.githubusercontent.com/f9c8ca4b3ebe6b7096d61655e481bd8495ea12bbd70d12b42d5d4f17fa019f94/68747470733a2f2f667265696768742e636172676f2e736974652f772f3830302f692f613933313639303230356332373136323437363231336238626363313731353835616164396438346436356364633132316361343235653831333131343132312f3078302e706e67"
    alt="PCBWay Sponsorship"
    width="720"
  />
</p>


<p align="center">
  <img
    src="Hardware/ECAD/photos/pcb_top.jpg"
    alt="F-UVC cap PCB – top side"
    width="400"
  />
  <img
    src="Hardware/ECAD/photos/pcb_populated.jpg"
    alt="F-UVC cap PCB – partially populated"
    width="400"
  />
</p>


![F-UVC cap PCB – top side](https://i.imgur.com/lHTRejb.jpeg)

PCB fabrication for this prototype was kindly sponsored by
[PCBWay](https://www.pcbway.com/), who covered the cost of the
4-layer boards and DHL shipping via store credit.

From a bring-up perspective the experience was exactly what I was hoping for:

- The 4-layer FR-4 S1000H TG150 stack-up (78 × 78 mm, 1.6 mm, ENIG, 1 oz Cu inner/outer)
  matched the KiCad design with no surprises.
- Drill hits, solder mask, and silkscreen registration were all on-point; pads were fully
  exposed and silkscreen stayed off fine-pitch pads.
- ENIG pads wetted cleanly during hot-air/QFN reflow, and the USB-C, BQ24074, and header
  footprints all fit mechanically without any bodge-wires or footprint hacks.

This repository and write-up reflect my own design decisions, testing, and documentation.
Sponsorship support did not include editorial control or performance claims; any results
here are based on my own prototyping and are shared for transparency and learning.



---

## Roadmap / next steps (WIP)

Planned expansions to this repo:
- Add **ECAD**: schematics + PCB files for the power path, MCU, interlocks, and UV-C gating
- Add **MCAD**: lid enclosure, interlock placement, sealing strategy, serviceability notes
- Log actual **measured rail stability**, inrush behavior, and thermal observations
- Add explicit **error handling** states (fault latch, blink codes, charging status UI)
- Optional: add **UV-C emission verification** sensing (future revision)

---

## License

MIT License — see `LICENSE`.
