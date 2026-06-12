#!/usr/bin/env python3
import argparse
import math
import re
from pathlib import Path


MEV_J = 1.602176634e-13
AMU_KG = 1.66053906660e-27
G0 = 9.80665


def extract(pattern, text, cast=float):
    match = re.search(pattern, text)
    if not match:
        raise ValueError(f"missing pattern: {pattern}")
    return cast(match.group(1))


def parse_summary(path):
    text = Path(path).read_text()
    return {
        "events": extract(r"The run consists of (\d+) GPS source", text, int),
        "fission_events": extract(r"Fission events\s+:\s+(\d+) /", text, int),
        "direct_fragments": extract(
            r"Total direct fission fragments\s+:\s+(\d+)", text, int
        ),
        "outer_escapes": extract(
            r"Outward fuel-coating escapes\s+:\s+(\d+)", text, int
        ),
        "outer_speed_km_s": extract(
            r"Outward coating escape speed\s+:\s+([0-9.]+) km/s", text
        ),
        "shape34_events": extract(
            r"Shape3 or Shape4 hit events\s+:\s+(\d+) /", text, int
        ),
        "fuel_bed_entries": extract(
            r"H2 fragment entries / exits\s+:\s+(\d+) /", text, int
        ),
        "fuel_bed_exits": extract(
            r"H2 fragment entries / exits\s+:\s+\d+ /\s+(\d+)", text, int
        ),
        "h2_edep_per_fission_mev": extract(
            r"H2 deposited fragment energy\s+:\s+[0-9.]+ MeV, "
            r"mean/event = [0-9.]+ MeV, mean/fission event = ([0-9.]+) MeV",
            text,
        ),
        "fuel_bed_exit_ke_mev": extract(
            r"H2 exit mean kinetic energy\s+:\s+([0-9.]+) MeV", text
        ),
        "fuel_bed_exit_speed_km_s": extract(
            r"H2 exit speed\s+:\s+([0-9.]+) km/s", text
        ),
    }


def pct(after, before):
    if before == 0:
        return 0.0
    return 100.0 * (after / before - 1.0)


def nozzle_velocity(args, total_temperature):
    pressure_term = 1.0 - args.pressure_ratio ** ((args.gamma - 1.0) / args.gamma)
    velocity2 = (
        args.nozzle_efficiency
        * 2.0
        * args.gamma
        / (args.gamma - 1.0)
        * args.h2_gas_constant
        * total_temperature
        * pressure_term
    )
    return math.sqrt(max(0.0, velocity2))


def fragment_thrust_upper_bound(summary, fission_rate):
    exit_fraction = summary["fuel_bed_exits"] / summary["direct_fragments"]
    fragment_mass_per_fission = args.fragment_mass_amu * AMU_KG
    mdot_fragments = fission_rate * fragment_mass_per_fission * exit_fraction
    thrust = mdot_fragments * summary["fuel_bed_exit_speed_km_s"] * 1000.0
    return thrust, mdot_fragments


def main():
    parser = argparse.ArgumentParser(
        description="Compute simplified H2 startup FFRE performance from logs."
    )
    parser.add_argument("baseline_log")
    parser.add_argument("h2_log")
    parser.add_argument("--fission-power-mw", type=float, default=1.0)
    parser.add_argument("--fission-energy-mev", type=float, default=200.0)
    parser.add_argument("--mdot-h2-g-s", type=float, nargs="+", default=[1.0, 5.0, 10.0])
    parser.add_argument("--heat-efficiency", type=float, default=0.7)
    parser.add_argument("--nozzle-efficiency", type=float, default=0.85)
    parser.add_argument("--cp-h2", type=float, default=14300.0)
    parser.add_argument("--tin-k", type=float, default=300.0)
    parser.add_argument("--gamma", type=float, default=1.4)
    parser.add_argument("--h2-gas-constant", type=float, default=4124.0)
    parser.add_argument("--pressure-ratio", type=float, default=0.01)
    parser.add_argument(
        "--fragment-mass-amu",
        type=float,
        default=236.0,
        help="Total fission-fragment mass represented per fission.",
    )
    args_ns = parser.parse_args()

    global args
    args = args_ns

    baseline = parse_summary(args.baseline_log)
    h2 = parse_summary(args.h2_log)

    fission_power = args.fission_power_mw * 1.0e6
    fission_rate = fission_power / (args.fission_energy_mev * MEV_J)
    p_dep = fission_rate * h2["h2_edep_per_fission_mev"] * MEV_J
    p_h2 = args.heat_efficiency * p_dep

    f_ff_baseline, mdot_ff_baseline = fragment_thrust_upper_bound(
        baseline, fission_rate
    )
    f_ff_h2, mdot_ff_h2 = fragment_thrust_upper_bound(h2, fission_rate)
    isp_baseline = f_ff_baseline / (mdot_ff_baseline * G0)

    print("# Shape1 H2 Startup Performance Estimate")
    print()
    print(f"baseline log: `{args.baseline_log}`")
    print(f"H2 log: `{args.h2_log}`")
    print()
    print("## Geant4 transport changes")
    print()
    print("| Quantity | No H2 | Shape1 H2 | Change |")
    print("| --- | ---: | ---: | ---: |")
    print(
        "| Fuel-bed exit speed (km/s) | "
        f"{baseline['fuel_bed_exit_speed_km_s']:.1f} | "
        f"{h2['fuel_bed_exit_speed_km_s']:.1f} | "
        f"{pct(h2['fuel_bed_exit_speed_km_s'], baseline['fuel_bed_exit_speed_km_s']):+.1f}% |"
    )
    print(
        "| Fuel-bed exit kinetic energy (MeV) | "
        f"{baseline['fuel_bed_exit_ke_mev']:.2f} | "
        f"{h2['fuel_bed_exit_ke_mev']:.2f} | "
        f"{pct(h2['fuel_bed_exit_ke_mev'], baseline['fuel_bed_exit_ke_mev']):+.1f}% |"
    )
    print(
        "| Fuel-bed fragment exits | "
        f"{baseline['fuel_bed_exits']} | {h2['fuel_bed_exits']} | "
        f"{pct(h2['fuel_bed_exits'], baseline['fuel_bed_exits']):+.1f}% |"
    )
    print(
        "| Shape3/Shape4 hit events | "
        f"{baseline['shape34_events']} | {h2['shape34_events']} | "
        f"{pct(h2['shape34_events'], baseline['shape34_events']):+.1f}% |"
    )
    print(
        "| H2 energy deposition per fission event (MeV) | "
        f"0.00 | {h2['h2_edep_per_fission_mev']:.2f} | n/a |"
    )
    print()
    print("## Simplified propulsion estimate")
    print()
    print(
        f"Assumptions: fission power = {args.fission_power_mw:g} MW, "
        f"heat efficiency = {args.heat_efficiency:g}, nozzle efficiency = "
        f"{args.nozzle_efficiency:g}, cp = {args.cp_h2:g} J/kg/K, "
        f"gamma = {args.gamma:g}, pressure ratio = {args.pressure_ratio:g}."
    )
    print()
    print(
        f"Deposited H2 power = {p_dep/1000.0:.1f} kW, usable H2 thermal power = "
        f"{p_h2/1000.0:.1f} kW."
    )
    print()
    print("| H2 mdot (g/s) | Delta T (K) | ve,H2 (m/s) | F before (N) | F after (N) | Delta F (N) | Isp before (s) | Isp after (s) |")
    print("| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for mdot_g_s in args.mdot_h2_g_s:
        mdot_h2 = mdot_g_s / 1000.0
        delta_t = p_h2 / (mdot_h2 * args.cp_h2)
        t0 = args.tin_k + delta_t
        ve = nozzle_velocity(args, t0)
        f_h2 = mdot_h2 * ve
        f_after = f_ff_h2 + f_h2
        isp_after = f_after / ((mdot_h2 + mdot_ff_h2) * G0)
        print(
            f"| {mdot_g_s:.3g} | {delta_t:.0f} | {ve:.0f} | "
            f"{f_ff_baseline:.3f} | {f_after:.3f} | "
            f"{f_after - f_ff_baseline:+.3f} | "
            f"{isp_baseline:.0f} | {isp_after:.0f} |"
        )

    print()
    print(
        "Note: fragment thrust here is a speed-based upper-bound estimate, not a "
        "self-consistent vector nozzle thrust. Use it for before/after trend "
        "comparison unless a dedicated outlet-axis momentum scorer is added."
    )


if __name__ == "__main__":
    main()
