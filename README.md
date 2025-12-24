# F-UVC-WaterBottle (Prototype)

**F-UVC-WaterBottle** is a prototype for a **portable, bottle-scale water treatment concept** that stages:

1. **Filtration** to handle turbidity and many dissolved contaminants, then  
2. **UVC LED exposure** to target microorganisms in the treated volume.

The current repo centers on **STM32 firmware** and a **lid PCB** that controls a gated UVC branch, backed by early **MCAD** for a two-chamber bottle (Outer dirty shell + inner filtered/UVC chamber) and a refillable filter cartridge.

> ⚠️ **Safety / Disclaimer**
>
> - This is **not** a certified disinfection device and **not** a minimum viable / usable product.  
> - **UVC is hazardous** to eyes and skin. Misuse can cause injury.  
> - Current printed parts are **not food-safe** and **not reliably watertight**.  
> - Battery-powered systems can fail dangerously if miswired (overcurrent, shorts, thermal runaway).  
> - This project exists for **engineering documentation and learning only**.  
>   **Do not rely on it for drinking water.**

---

## What this project is trying to explore

**User story (target scenario)**

> “As someone who travels, camps, or works where tap water isn’t always viable,  
> I need a compact bottle that can handle **turbidity, taste, and biological contamination**,  
> so I can safely drink without hauling single-use filters, chemicals, or a stove.”

Rather than choosing **filtration *or* UVC**, this prototype explores a **staged approach** inspired by:

- **Press-to-filter systems** (e.g. Grayl GeoPress) for **multi-barrier filtration**, and  
- **Cap-integrated UVC** (e.g. Larq PureVis 2) for **in-bottle optical disinfection**.

The long-term research question is:  
Can a bottle-scale system combine **filter-first** architecture with **geometry-aware UVC dosing** and still be **usable** (Cycle times, charging, cleaning, and cost)?

For the full background, math, and validation plan, see the accompanying [project write-up](https://colbydanielallen.com/portfolio/filtration-uvc-water-bottle/).

---

## Repository layout

- **/Software/** – STM32L031 firmware project (STM32CubeIDE)  
- **/Hardware/** – hardware artifacts  
  - **/Hardware/ECAD/** – KiCad PCB files for the lid/control board (power path, MCU, interlocks, UVC gating)  
  - **/Hardware/MCAD/** – SolidWorks models & STEP files for the bottle's inner chamber, outer chamber, lid, and filter cartridge  
- **/Docs/** *(planned)* – validation notes, test logs, and design references

---

## System overview

### Functional architecture

**Treatment path**

1. **Outer chamber (Dirty water)**  
   - Filled from the source (Tap, stream, questionable faucet).  
   - Houses a **threaded, refillable filter cartridge**.

2. **Filter cartridge (FairCap-inspired)**  
   - Printable shell intended to be packed with **cotton + activated carbon + media**.  
   - Goal: Reduce particulates, some metals/organics, and improve UV transmittance (UVT).

3. **Inner chamber (Filtered water)**  
   - Receives water through the filter cartridge.  
   - Interior will be designed to be **reflective** and geometrically favorable for UVC dose delivery.

4. **Lid UVC subsystem**  
   - Contains **UVC LED + driver**, **charger/power-path board**, **MCU**, **load switch**, **reed switch**, and **status LED(s)**.  
   - UVC LED shines through a **small UVC-transmissive window** into the inner chamber.

**Electronics & control**

- **Li-ion battery** + **BQ24074-class charger** for USB/DC/solar input and load sharing.  
- **3.3 V rail** for logic (STM32L031K6).  
- **High-side load switch** for the UVC branch (Gated by MCU and interlocks).  
- **Single push button** for cycle control.  
- **RGB LED** for state indication.  
- **Reed switch + magnet** for lid-closed detection (firmware + hardware gating).

---

## Hardware stack

### MCAD (Bottle chambers, lid, and filter)

The MCAD branch (SolidWorks) currently includes:

- **Outer chamber**: Holds untreated water.  
- **Inner chamber**: Receives filtered water and forms the UVC treatment volume.  
- **Filter cartridge**: A threaded, refillable block inspired by FairCap, intended to be packed with media (cotton + activated carbon + salt, etc.).  
- **Lid**: Houses the electronics, UVC lens/window, RGB light pipes, and reed-switch magnet pocket.

**Status / caveats**

- Current prints are **PETG** and intended as **“looks-like” prototypes only**.  
- Magnet pocket and wall thickness are still being refined (Early prints exposed the magnet cavity into the threads).  
- Future revisions will move away from epoxy and toward **M2 fasteners and gaskets** for serviceability and sealing.  
- Long-term, a **metal inner chamber** (Steel) would be preferable for UVC compatibility and cleanliness.

### ECAD (Lid PCB, rev A)

The ECAD branch uses **KiCad** and currently implements:

- **Power path & charging**
  - **BQ24074-class charger** (Adafruit reference design)  
  - Input: **USB-C or solar (5-10 V)**  
  - Load sharing (Run from input while charging battery)  
  - Input dynamic power management to avoid solar brown-out  
  - Status: **PGOOD**, **CHG** exposed for future MCU use

- **Logic rail**
  - **MCP1700** 3.3 V LDO (or similar) from charger **SYS/OUT** rail  
  - Local decoupling and layout tuned for stability

- **UVC branch gating**
  - **TPS22918** high-side load switch
  - VIN from SYS rail, VOUT to UVC driver module  
  - MCU-controlled **EN**, default-OFF on reset/boot  
  - Optional CT pin for soft-start (In-rush shaping)

- **MCU & I/O**
  - **STM32L031K6**: Initially hosted on a **NUCLEO-L031K6** dev board, with the custom PCB providing power and I/O connectivity.  
  - **Reed switch**: Input for lid-closed detection.  
  - **Single user button**: User input.  
  - **RGB LED** (tri-color, PWM-driven): For state indication.

A separate constant-current UVC LED board (i.e. **IO Rodeo 275nm module**) currently handles LED drive. In future revisions its topology could be folded into a dedicated UVC daughterboard or an integrated lid PCB.

---

## Firmware: STM32L031K6

Firmware lives under **`/Software/`** and targets **STM32L031K6Tx**, typically on a **NUCLEO-L031K6** for bring-up.

### High-level behavior

- **Gated UVC branch**
  - Controls the UVC load-switch **EN** line.  
  - **OFF by default** at boot/reset.  
  - Forced OFF on:
    - lid-open (reed switch)  
    - cycle cancel  
    - (future) fault conditions

- **Cycle selection (single button)**
  - **Single tap** → short cycle (~1 minute), LED **amber** (R+G).  
  - **Double tap** (within ~400 ms) → long cycle (~3 minutes), LED **blue**.  
  - **Tap during an active cycle** → cancel cycle, force UVC OFF, return to idle.

- **Reed-switch safety**
  - Input is configured with pull-up:
    - **LOW → lid closed / magnet present (safe)**  
    - **HIGH → lid open (unsafe → force UVC OFF)**  
  - If lid opens during a cycle:
    - UVC branch is shut off immediately.  
    - State machine resets (no lingering taps or modes).  
    - LED returns to idle (off).

- **Auto-stop**
  - Each cycle tracks a target end-time.  
  - At expiry, UVC branch = OFF, mode cleared, LED back to idle.

### I/O mapping (expected)

**Inputs**

- `REED_SW_Pin` – reed switch (input with pull-up).  
- `B2_Pin` – user button (EXTI, falling-edge, active-low).

**Outputs**

- `Control_Pin` – UVC load-switch enable:  
  - LOW → UVC branch OFF  
  - HIGH → UVC branch ON
- RGB LED via **TIM2 PWM** channels (common-anode assumed):
  - `PA3` → TIM2 CH4 → RED  
  - `PA1` → TIM2 CH2 → GREEN  
  - `PA0` → TIM2 CH1 → BLUE

Some SPI/OLED-related pins may remain configured from CubeMX templates. They’re reserved for future debug UI and don’t affect UVC logic.

---

## Build & flash (STM32CubeIDE)

- **Toolchain:** STM32CubeIDE 
- **Target MCU:** STM32L031K6Tx

1. Clone the repository.
2. Open **STM32CubeIDE** → **File → Import… → Existing Projects into Workspace…**.
3. Select the firmware project under `/Software/`.
4. Build the **Debug** or **Release** configuration.
5. Connect **ST-LINK** (NUCLEO onboard or external).
6. Click **Run** or **Debug** to flash.

After flashing, for bench testing:

- Power the board from a **regulated 3.3 V rail** (or the NUCLEO’s default power path).  
- Connect the reed switch, button, RGB LED, and load-switch control line as per the I/O mapping.  
- Confirm that behavior matches the “High-level behavior” section above using a **safe stand-in load** (e.g. an OLED display or resistor) instead of a UVC LED.

---

## Validation status (UVC effectiveness)

A detailed **biodosimetry-based validation plan** is drafted (MS2 coliphage, collimated-beam-derived k(λ), RED/VF framework, EPA 186 mJ/cm² target), but:

> 🔬 **No biological tests have been executed yet.**  
> No log-reduction claims are made for this prototype.

Until validation is complete:

- All cycle times, UX modes, and design equations should be treated as **engineering placeholders**, not safety guarantees.  
- Any use of this design with real water is strictly **at your own risk** and **strongly discouraged**.

---
## Sponsorship (PCB fabrication)

<p align="center">
  <img
    src="https://camo.githubusercontent.com/f9c8ca4b3ebe6b7096d61655e481bd8495ea12bbd70d12b42d5d4f17fa019f94/68747470733a2f2f667265696768742e636172676f2e736974652f772f3830302f692f613933313639303230356332373136323437363231336238626363313731353835616164396438346436356364633132316361343235653831333131343132312f3078302e706e67"
    alt="PCBWay Sponsorship"
    width="720"
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

- **ECAD**
  - Integrate or document a dedicated UVC LED + heatsink board.  
  - Tighten power-integrity checks (inrush, brownout behavior, EMC, thermal).

- **MCAD**
  - Add outer bottle, finalized filter cartridge, and realistic sealing strategy (O-rings, glands/grooves, fasteners).  
  - Explore food-safe materials (Metal inner chamber, UVC-stable windows, and gaskets).

- **Firmware**
  - Add explicit **fault/error states** (Blink codes, fault latch, charging/solar status UI).  
  - Integrate charger status (PGOOD / CHG) into UX.

- **Testing & validation**
  - Execute **UVC biodosimetry tests** and publish RED / DVal results.  
  - Map measured performance back into Design Equations 1 & 2 to tune cycle times and power.

---

## License

MIT License — see `LICENSE`.
