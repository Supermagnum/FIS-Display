## Bill of Materials — Pico 2 W FIS Interceptor/Injector PCB (VW Passat B6 FIS)

Firmware target: Raspberry Pi Pico 2 W based inline 3LB (DATA/CLK/ENA) interceptor/injector for the VW Passat B6 (3C) FIS display, with USB CDC to Raspberry Pi (Navit) and Bluetooth SPP to Android head unit. No relays or analog switches; arbitration is done in firmware, with Pico sharing the bus via level shifting.

| Item | Value / Part | DigiKey P/N (example) | Qty | Package | Notes |
|------|--------------|-----------------------|-----|---------|-------|
| 1 | Raspberry Pi Pico 2 W module (RP2350, WiFi/BT) | `2648-SC0919-ND` (SC0919) | 1 | Module, THT header rows | Main MCU, USB, regulator, WiFi + Bluetooth (used for SPP). Mounts via 2×20 2.54 mm headers. **THT preferred** |
| 2 | BS170 N‑MOSFET (3LB level shift FETs) | `BS170-ND` | 6 | TO‑92 THT | Six channels of level shifting: three RX (GPIO0/1/2 from 5 V bus) and three TX (GPIO3/4/5 to 5 V bus) using NXP bidirectional MOSFET level shifter topology. **THT preferred** |
| 3 | 2.2 kΩ 1/4 W 5 % resistors (5 V pull‑ups for 3LB) | e.g. `CF14JT2K20CT-ND`; mfr **SFR25H0002201FR500** | 6 | Axial THT resistor | 2.2 kΩ from each level‑shifter 5 V side node (ENA/CLK/DATA, RX+TX) to +5 V. Non-inductive. Metal film or SMD thick/thin film; avoid wirewound. **THT preferred** |
| 4 | 2.2 kΩ 1/4 W 5 % resistors (3.3 V pull‑ups for 3LB) | e.g. `CF14JT2K20CT-ND`; mfr **SFR25H0002201FR500** | 6 | Axial THT resistor | 2.2 kΩ from each level‑shifter 3.3 V side node (ENA/CLK/DATA, RX+TX) to +3.3 V. Non-inductive. Metal film or SMD thick/thin film; avoid wirewound. **THT preferred** |
| 5 | 100 Ω 1/4 W 5 % resistors (series damping on 3LB outputs, optional) | e.g. `CF14JT100RCT-ND`; mfr **MFP-25BRD52-100R** | 1 | Axial THT resistor | Non-inductive. In series between Pico/level‑shifter outputs and cluster T32a pins to reduce ringing and EMI. Metal film or SMD thick/thin film; avoid wirewound. **THT preferred** |
| 6 | JST PH 3‑pin receptacle housing (cluster side) | `455-1705-ND` (PHR‑3) | 1 | JST PH 3‑pos housing | 3‑pin inline connector to instrument cluster side harness as specified. |
| 7 | JST PH 3‑pin receptacle housing (harness side) | `455-1705-ND` (PHR‑3) | 1 | JST PH 3‑pos housing | 3‑pin inline connector to radio/ECU harness side. |
| 8 | JST PH 3‑pin vertical header (cluster side PCB) | `455-1126-ND` (B3B-PH-K-S(LF)(SN)) | 1 | JST PH THT header | PCB‑mount header for T32a‑cluster side cable. **THT preferred** |
| 9 | JST PH 3‑pin vertical header (harness side PCB) | `455-1126-ND` (B3B-PH-K-S(LF)(SN)) | 1 | JST PH THT header | PCB‑mount header for harness‑side cable. **THT preferred** |
| 10 | JST PH crimp terminals | `455-2148TR-ND` (SPH-002T-P0.5S) | 6+ | Crimp contact | For both 3‑pin housings; fits 24–32 AWG as specified. Order strip quantity for spares. |
| 11 | 10 µF ≥16 V electrolytic capacitor (5 V bulk decoupling) | e.g. `1189-1100-ND` | 1 | Radial electrolytic THT | Bulk decoupling on 5 V rail near JST/VSYS entry to smooth transients. **THT preferred** |
| 12 | 100 nF 50 V ceramic capacitors (local decoupling) | e.g. `399-4267-ND` | 3–4 | Radial ceramic THT | Place near Pico VSYS, 3.3 V rail, and near level‑shifter 3LB connections. **THT preferred** |
| 13 | 3‑channel ESD/TVS array for 3LB lines | e.g. `SP0503BAJTCT-ND` | 1 | SOT‑23‑6 SMD | ESD / surge protection for ENA, CLK, DATA at cluster connector; place close to JST PH cluster connector. |
| 14 | 5 V TVS diode for USB/VSYS (optional) | e.g. `SMF5.0A-E3-08GICT-ND` | 1 | SOD‑123FL SMD | Protects VSYS/USB 5 V rail from spikes; optional, Raspberry Pi already regulates 5 V. |
| 15 | 3 mm green LED (power indicator) | e.g. `160-1163-ND`; mfr **C503B-GAN-CD0E0781** | 1 | 3 mm THT LED | Indicates board powered from Pi USB (VSYS). Connect with series resistor to 5 V. **THT preferred** |
| 16 | 1×20 2.54 mm female headers (Pico sockets, PCB side) | e.g. `S7039-ND` | 2 | 1×20 THT female header | Sockets for Pico 2 W module, so it is removable/serviceable. **THT preferred** |
| 17 | 1×20 2.54 mm male headers (Pico pins, if needed) | e.g. `S1012EC-20-ND` | 2 | 1×20 THT male header | Soldered to Pico 2 W if not pre‑pinned, to plug into PCB sockets. **THT preferred** |
| 18 | 1×3 2.54 mm male header (debug/test for 3LB, optional) | e.g. `S1011EC-03-ND` | 1 | 1×3 THT male header | Optional header for scope/logic‑analyzer access to ENA/CLK/DATA on PCB. **THT preferred** |
| 19 | Nylon M3 standoffs (board mounting) | e.g. `RPC2205-ND` | 4 | Nylon standoff | Mechanically mounts board inside dash/enclosure; choose length to suit. |
| 20 | M3 machine screws (board mounting) | e.g. `RPC1495-ND` | 4 | M3 screw | For securing PCB to standoffs or enclosure. |
| 21 | Silkscreen / labeling (no P/N) | – | – | – | Ensure PCB silkscreen clearly labels JST connectors (cluster vs harness), ENA/CLK/DATA signal directions, Pico orientation, and indicates Bluetooth device name (“FIS‑Bridge”) for installers. |

**Optional CAN bus (when CAN support is implemented):** B6 comfort/infotainment CAN runs at **100 kbit/s** (not 500 kbit/s). **MCP2561** — CAN transceiver; only 2 signal connections to the Pico: **TXD** (GPIO 11), **RXD** (GPIO 12). Logic side 3.3 V compatible, no level shifter to Pico. **120 ohm** non-inductive termination when your node is at a bus end: mfr **MFR50SFTE52-120R**. (Tapped in the middle: no termination; replacing OEM radio: may need 120 ohm; bench: both ends 120 ohm.) Measure CAN-H to CAN-L with power off: ~60 ohm = both terminations present, ~120 ohm = one. **4700 pF ceramic** cap: mfr **RCER71H472K0M1H03A**. Decoupling capacitors on MCP2561 supply pins. CAN is disabled by default in `firmware/fis_config_defaults.h`. See firmware README for full termination notes.

