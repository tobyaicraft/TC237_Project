# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Embedded firmware for **Infineon AURIX TC237** (TriCore) microcontroller. Implements CAN communication with ADC data acquisition, PWM output, UART serial, and GPIO control via a 1ms cooperative scheduler.

- **MCU**: TC237L (TC23A family)
- **Toolchain**: TASKING TriCore C/C++ Compiler (primary), GCC (alternative)
- **IDE**: AURIX Studio (Eclipse CDT based)
- **Framework**: iLLD (Infineon Low-Level Driver) v1.20.0 for TC23A
- **Clock**: 20 MHz XTAL -> 200 MHz PLL

## Build

This project uses AURIX IDE managed build (Eclipse CDT). There is no standalone CLI build command — builds are triggered from the IDE. Build configurations:

- **TriCore Debug (TASKING)** — primary debug config, outputs `TriCore Debug (TASKING)/TC237_Project.elf`
- **TriCore Release (TASKING)**, **TriCore Debug (GCC)**, **TriCore Release (GCC)** — alternatives

Debug uses IConnect winIDEA debugger over USB (IFX_DAS), targeting TC233L, breakpoint at `core0_main()`.

## Architecture

```
core0_main()  →  Driver Init (Intc, Dio, Adc, Pwm, GtmTimer, Tim, Uart, Can)
                  →  Enable Interrupts
                  →  while(1) Scheduler_Run()

Scheduler_Run()  →  GTM TOM0 Ch1 ISR increments g_1ms_counter (1ms tick)
                 →  Modulo dispatch:
                     %1   → AppTask_1ms()    (UART echo)
                     %10  → AppTask_10ms()   (ADC scan, PWM update, TIM read, CAN TX)
                     %100 → AppTask_100ms()  (LED toggle)
```

**Non-preemptive cooperative scheduler** — all tasks must be short and return quickly. No RTOS.

### Source file conventions

| Prefix | Role | Examples |
|--------|------|---------|
| `Drv*` | Peripheral driver (one per hardware module) | DrvCan, DrvAdc, DrvUart, DrvPwm |
| `App*` | Application-level task logic | AppTask |
| `g_*` | Global variable | g_1ms_counter, g_pwmDuty |
| `s_*` | File-static variable | — |

### Key hardware configuration

| Peripheral | Pins | Config |
|-----------|------|--------|
| UART (ASCLIN0) | TX=P15.2, RX=P15.3 | 115200 8-N-1 |
| CAN (MultiCAN Node0) | TX=P20.8, RX=P20.7 | 500 kbps, MSG ID=0x100 |
| ADC (VADC Group0) | AN0=P40.0, AN1=P40.1, AN2=P40.2 | 12-bit, SW-triggered scan |
| PWM (GTM TOM0 Ch0) | P00.9 | ~100 Hz |
| LED0 | P13.0 | Active-low, toggle @100ms |

### ISR priorities

GTM Timer (scheduler tick) = 11, STM0 (fallback tick) = 10, GTM TIM = 5, UART RX = 4, UART TX = 3, UART ERR = 2.

## Libraries layout

- `Libraries/iLLD/TC23A/` — Infineon low-level drivers (VADC, Multican, Asclin, Gtm, Port, Scu, Stm, etc.)
- `Libraries/Infra/` — Platform abstraction, SFR register definitions, compiler abstractions
- `Libraries/Service/` — Standard interfaces (StdIf), system services (SysSe)

Do not modify library files. Application code lives in the project root (`Cpu0_Main.c`, `Scheduler.c`, `AppTask.c`, `Drv*.c/h`).

## Other resources

- `SubMCU/TC237_Project_CAN.zip` — secondary CAN receiver board firmware
- `Tool/adc_monitor_v2.py` — Python ADC serial monitor tool
- `docs/CAN_Project_Summary.html` — project summary (Korean)
- `Configurations/Ifx_Cfg.h` — system clock and project-level config defines
- Linker scripts: `Lcf_Tasking_Tricore_Tc.lsl` (TASKING), `Lcf_Gnuc_Tricore_Tc.lsl` (GCC)

## Language

Comments and documentation in the codebase are written in Korean. Follow this convention.
