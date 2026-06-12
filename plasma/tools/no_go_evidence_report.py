#!/usr/bin/env python3
"""Generate a no-go evidence dashboard from the DPFFR simulation output.

This script runs `exampleB1` with a macro, parses the global run summary,
exports a CSV evidence table, and renders a red PASS/FAIL SVG dashboard.
"""

from __future__ import annotations

import argparse
import csv
import html
import math
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


TIME_UNIT_TO_SECONDS = {
    "s": 1.0,
    "ms": 1.0e-3,
    "us": 1.0e-6,
    "ns": 1.0e-9,
    "ps": 1.0e-12,
}


@dataclass
class EvidenceRow:
    metric: str
    status: str
    observed: str
    constraint: str
    gap_x: float
    evidence: str


@dataclass
class ParsedSummary:
    thermal_feasibility: str
    target_reach_percent: float
    window_token: str
    window_seconds: float
    critical_status: str
    critical_ratio_percent: float
    thermal_design_mass_status: str
    thermal_design_mass_ratio_percent: float
    cooling_target_status: str
    cooling_target_overload_x: float
    cooling_stability_status: str
    cooling_stability_overload_x: float
    target_temp_k: float
    time_to_target_token: str
    time_to_target_seconds: float
    stability_temp_k: float
    time_to_stability_token: str
    time_to_stability_seconds: float
    normalized_dose_line: str
    normalized_temp_line: str
    normalized_power_line: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        default="build",
        help="Build directory containing exampleB1 executable",
    )
    parser.add_argument(
        "--macro",
        default="run_no_go.mac",
        help="Macro file passed to exampleB1",
    )
    parser.add_argument(
        "--log-out",
        default="build/evidence/no_go_run.log",
        help="Path to write full simulation log",
    )
    parser.add_argument(
        "--csv-out",
        default="build/evidence/no_go_evidence.csv",
        help="Path to write CSV evidence table",
    )
    parser.add_argument(
        "--svg-out",
        default="build/evidence/no_go_dashboard.svg",
        help="Path to write SVG dashboard",
    )
    return parser.parse_args()


def parse_time_token(token: str) -> float:
    match = re.fullmatch(r"\s*([0-9.+\-eE]+)\s*([a-zA-Z]+)\s*", token)
    if not match:
        raise ValueError(f"Unrecognized time token: {token!r}")
    value = float(match.group(1))
    unit = match.group(2)
    if unit not in TIME_UNIT_TO_SECONDS:
        raise ValueError(f"Unsupported time unit in token {token!r}")
    return value * TIME_UNIT_TO_SECONDS[unit]


def must_match(pattern: str, text: str, label: str) -> re.Match[str]:
    match = re.search(pattern, text, flags=re.MULTILINE)
    if not match:
        raise ValueError(f"Unable to parse {label} with pattern: {pattern}")
    return match


def extract_global_summary(full_log: str) -> str:
    block = re.search(
        r"--------------------End of Global Run-----------------------(.*?)"
        r"------------------------------------------------------------",
        full_log,
        flags=re.DOTALL,
    )
    if not block:
        raise ValueError("Could not find global run summary block in log output")
    return block.group(0)


def parse_summary(block: str) -> ParsedSummary:
    thermal_feasibility = must_match(
        r"Thermal feasibility screen \(normalized\):\s*([A-Z_]+)",
        block,
        "thermal feasibility",
    ).group(1)
    target_reach_percent = float(
        must_match(
            r"Thermal feasibility screen \(normalized\):.*target reach =\s*([0-9.+\-eE]+)%",
            block,
            "target reach percentage",
        ).group(1)
    )

    window_token = must_match(
        r"Power normalization window:\s*([0-9.+\-eE]+\s*[a-zA-Z]+)\s+at",
        block,
        "normalization window",
    ).group(1)
    window_seconds = parse_time_token(window_token)

    critical_match = must_match(
        r"\[1\].*?:\s*(FAIL|PASS).*ratio\s*=\s*([0-9.+\-eE]+)%",
        block,
        "critical-mass dashboard line",
    )
    critical_status = critical_match.group(1)
    critical_ratio_percent = float(critical_match.group(2))

    thermal_design_mass_match = must_match(
        r"\[2\].*?:\s*(FAIL|PASS).*modeled/reference\s*=\s*([0-9.+\-eE]+)%",
        block,
        "thermal-design mass closure line",
    )
    thermal_design_mass_status = thermal_design_mass_match.group(1)
    thermal_design_mass_ratio_percent = float(thermal_design_mass_match.group(2))

    cooling_target_match = must_match(
        r"\[3\].*?:\s*(FAIL|PASS).*required/available cooling power\s*=\s*([0-9.+\-eE]+)x",
        block,
        "target cooling dashboard line",
    )
    cooling_target_status = cooling_target_match.group(1)
    cooling_target_overload_x = float(cooling_target_match.group(2))

    cooling_stability_match = must_match(
        r"\[4\].*?:\s*(FAIL|PASS).*required/available cooling power\s*=\s*([0-9.+\-eE]+)x",
        block,
        "stability cooling dashboard line",
    )
    cooling_stability_status = cooling_stability_match.group(1)
    cooling_stability_overload_x = float(cooling_stability_match.group(2))

    overheat_match = must_match(
        r"\[5\].*to\s+([0-9.+\-eE]+)\s*K\s*in\s*([0-9.+\-eE]+\s*[a-zA-Z]+)\s*,\s*"
        r"to\s+([0-9.+\-eE]+)\s*K\s*in\s*([0-9.+\-eE]+\s*[a-zA-Z]+)",
        block,
        "overheat dashboard line",
    )
    target_temp_k = float(overheat_match.group(1))
    time_to_target_token = overheat_match.group(2)
    stability_temp_k = float(overheat_match.group(3))
    time_to_stability_token = overheat_match.group(4)
    time_to_target_seconds = parse_time_token(time_to_target_token)
    time_to_stability_seconds = parse_time_token(time_to_stability_token)

    normalized_dose_line = must_match(
        r"Reactor-normalized scoring dose in window:.*",
        block,
        "normalized dose line",
    ).group(0).strip()
    normalized_power_line = must_match(
        r"Reactor-normalized scoring power:.*",
        block,
        "normalized power line",
    ).group(0).strip()
    normalized_temp_line = must_match(
        r"Reactor-normalized temperature rise over window:.*",
        block,
        "normalized temperature line",
    ).group(0).strip()

    return ParsedSummary(
        thermal_feasibility=thermal_feasibility,
        target_reach_percent=target_reach_percent,
        window_token=window_token.strip(),
        window_seconds=window_seconds,
        critical_status=critical_status,
        critical_ratio_percent=critical_ratio_percent,
        thermal_design_mass_status=thermal_design_mass_status,
        thermal_design_mass_ratio_percent=thermal_design_mass_ratio_percent,
        cooling_target_status=cooling_target_status,
        cooling_target_overload_x=cooling_target_overload_x,
        cooling_stability_status=cooling_stability_status,
        cooling_stability_overload_x=cooling_stability_overload_x,
        target_temp_k=target_temp_k,
        time_to_target_token=time_to_target_token.strip(),
        time_to_target_seconds=time_to_target_seconds,
        stability_temp_k=stability_temp_k,
        time_to_stability_token=time_to_stability_token.strip(),
        time_to_stability_seconds=time_to_stability_seconds,
        normalized_dose_line=normalized_dose_line,
        normalized_temp_line=normalized_temp_line,
        normalized_power_line=normalized_power_line,
    )


def build_rows(summary: ParsedSummary) -> list[EvidenceRow]:
    heating_status = "PASS" if summary.target_reach_percent >= 100.0 else "FAIL"
    heating_gap_x = math.inf
    if summary.target_reach_percent > 0.0:
        heating_gap_x = 100.0 / summary.target_reach_percent

    critical_gap_x = math.inf
    if summary.critical_ratio_percent > 0.0:
        critical_gap_x = 100.0 / summary.critical_ratio_percent

    thermal_design_mass_gap_x = math.inf
    if summary.thermal_design_mass_ratio_percent > 0.0:
        thermal_design_mass_gap_x = 100.0 / summary.thermal_design_mass_ratio_percent

    overheat_status = "FAIL" if summary.time_to_target_seconds < summary.window_seconds else "PASS"
    overheat_gap_x = math.inf
    if summary.time_to_target_seconds > 0.0:
        overheat_gap_x = summary.window_seconds / summary.time_to_target_seconds

    return [
        EvidenceRow(
            metric="Heating Sufficiency @ Target Temperature",
            status=heating_status,
            observed=(
                f"{summary.target_reach_percent:.9g}% target reach"
                f" ({summary.thermal_feasibility})"
            ),
            constraint="Target reach >= 100% within normalization window",
            gap_x=heating_gap_x,
            evidence="From thermal feasibility line in global run summary",
        ),
        EvidenceRow(
            metric="Critical-Mass Proxy",
            status=summary.critical_status,
            observed=f"{summary.critical_ratio_percent:.9g}%",
            constraint=">= 100% of U-235 reference critical mass (11 kg proxy)",
            gap_x=critical_gap_x,
            evidence="From NO-GO line [1] in global run summary",
        ),
        EvidenceRow(
            metric="Thermal-Design Mass Closure",
            status=summary.thermal_design_mass_status,
            observed=f"{summary.thermal_design_mass_ratio_percent:.9g}%",
            constraint=">= 100% of literature thermal-design fuel mass (15 kg)",
            gap_x=thermal_design_mass_gap_x,
            evidence="From NO-GO line [2] in global run summary",
        ),
        EvidenceRow(
            metric="Cooling Margin @ Target Temperature",
            status=summary.cooling_target_status,
            observed=f"{summary.cooling_target_overload_x:.9g}x overload",
            constraint="Cooling overload factor <= 1.0x",
            gap_x=summary.cooling_target_overload_x,
            evidence="From NO-GO line [3] in global run summary",
        ),
        EvidenceRow(
            metric="Cooling Margin @ Stability Temperature",
            status=summary.cooling_stability_status,
            observed=f"{summary.cooling_stability_overload_x:.9g}x overload",
            constraint="Cooling overload factor <= 1.0x",
            gap_x=summary.cooling_stability_overload_x,
            evidence="From NO-GO line [4] in global run summary",
        ),
        EvidenceRow(
            metric=f"Overheat Time to {summary.target_temp_k:.0f} K",
            status=overheat_status,
            observed=(
                f"{summary.time_to_target_token} (to {summary.stability_temp_k:.0f} K:"
                f" {summary.time_to_stability_token})"
            ),
            constraint=f"Time to target >= normalization window ({summary.window_token})",
            gap_x=overheat_gap_x,
            evidence="From NO-GO line [5] in global run summary",
        ),
    ]


def write_csv(rows: Iterable[EvidenceRow], csv_path: Path) -> None:
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", newline="", encoding="utf-8") as fp:
        writer = csv.writer(fp)
        writer.writerow(["metric", "status", "observed", "constraint", "gap_x", "evidence"])
        for row in rows:
            writer.writerow(
                [row.metric, row.status, row.observed, row.constraint, f"{row.gap_x:.9g}", row.evidence]
            )


def status_colors(status: str) -> tuple[str, str, str]:
    if status == "FAIL":
        return "#7f1d1d", "#fee2e2", "#b91c1c"
    if status == "PASS":
        return "#14532d", "#dcfce7", "#16a34a"
    return "#334155", "#e2e8f0", "#64748b"


def safe_gap_label(gap_x: float) -> str:
    if math.isinf(gap_x):
        return "gap=inf"
    return f"gap={gap_x:.3g}x"


def zh_status(status: str) -> str:
    return {"FAIL": "失败", "PASS": "通过"}.get(status, status)


def zh_feasibility(value: str) -> str:
    return {
        "POTENTIALLY_FEASIBLE": "仅加热上可达",
        "MARGINAL": "临界边缘",
        "INSUFFICIENT_HEATING": "加热不足",
    }.get(value, value)


def zh_metric(metric: str) -> str:
    mapping = {
        "Heating Sufficiency @ Target Temperature": "目标温度加热充分性",
        "Critical-Mass Proxy": "临界质量代理",
        "Thermal-Design Mass Closure": "热设计质量闭合",
        "Cooling Margin @ Target Temperature": "目标温度冷却裕度",
        "Cooling Margin @ Stability Temperature": "稳定温度冷却裕度",
    }
    if metric.startswith("Overheat Time to "):
        return metric.replace("Overheat Time to ", "过热达到 ")
    return mapping.get(metric, metric)


def zh_constraint(constraint: str) -> str:
    mapping = {
        "Target reach >= 100% within normalization window": "归一化时间窗内目标温度达成率 >= 100%",
        ">= 100% of U-235 reference critical mass (11 kg proxy)": "至少达到 U-235 参考临界质量的 100%（11 kg 代理）",
        ">= 100% of literature thermal-design fuel mass (15 kg)": "至少达到文献热设计燃料质量的 100%（15 kg）",
        "Cooling overload factor <= 1.0x": "冷却超载倍数 <= 1.0x",
    }
    if constraint.startswith("Time to target >= normalization window"):
        return constraint.replace("Time to target >= normalization window", "达到目标温度时间 >= 归一化时间窗")
    return mapping.get(constraint, constraint)


def zh_observed(observed: str) -> str:
    observed = observed.replace("% target reach", "% 目标达成率")
    observed = observed.replace("(POTENTIALLY_FEASIBLE)", "（仅加热上可达）")
    observed = observed.replace("(MARGINAL)", "（临界边缘）")
    observed = observed.replace("(INSUFFICIENT_HEATING)", "（加热不足）")
    observed = observed.replace("x overload", "x 超载")
    observed = observed.replace("(to ", "（到 ")
    observed = observed.replace(" K: ", " K：")
    observed = observed.replace(")", "）")
    return observed


def zh_gap_label(gap_x: float) -> str:
    if math.isinf(gap_x):
        return "差距=无限大"
    return f"差距={gap_x:.3g}x"


def render_svg(rows: list[EvidenceRow], summary: ParsedSummary, svg_path: Path) -> None:
    width = 1400
    top = 130
    row_h = 150
    gap = 18
    height = top + len(rows) * (row_h + gap) + 80

    fail_count = sum(1 for r in rows if r.status == "FAIL")
    overall = "判废" if fail_count > 0 else "通过"
    overall_color = "#7f1d1d" if overall == "判废" else "#14532d"

    parts: list[str] = []
    parts.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">'
    )
    parts.append(f'<rect x="0" y="0" width="{width}" height="{height}" fill="#fff7f7"/>')
    parts.append(
        f'<text x="40" y="58" font-family="DejaVu Sans, Arial, sans-serif" font-size="40" '
        f'font-weight="700" fill="{overall_color}">DPFFR 不可行证据看板：{overall}</text>'
    )
    parts.append(
        f'<text x="40" y="92" font-family="DejaVu Sans, Arial, sans-serif" font-size="22" fill="#7f1d1d">'
        f'归一化热判据：{html.escape(zh_feasibility(summary.thermal_feasibility))} | 失败项：{fail_count}/{len(rows)}</text>'
    )
    parts.append(
        f'<text x="40" y="118" font-family="DejaVu Sans, Arial, sans-serif" font-size="17" fill="#475569">'
        f'归一化时间窗：{html.escape(summary.window_token)} | 目标温度：{summary.target_temp_k:.0f} K | 稳定温度：{summary.stability_temp_k:.0f} K</text>'
    )

    y = top
    for idx, row in enumerate(rows, start=1):
        fg, bg, badge = status_colors(row.status)
        parts.append(
            f'<rect x="28" y="{y}" width="{width - 56}" height="{row_h}" rx="14" '
            f'fill="{bg}" stroke="{fg}" stroke-width="2"/>'
        )
        parts.append(
            f'<rect x="48" y="{y + 18}" width="120" height="42" rx="10" fill="{badge}"/>'
        )
        parts.append(
            f'<text x="108" y="{y + 46}" text-anchor="middle" font-family="DejaVu Sans, Arial, sans-serif" '
            f'font-size="22" font-weight="700" fill="#ffffff">{html.escape(zh_status(row.status))}</text>'
        )
        parts.append(
            f'<text x="190" y="{y + 44}" font-family="DejaVu Sans, Arial, sans-serif" font-size="28" '
            f'font-weight="700" fill="{fg}">[{idx}] {html.escape(zh_metric(row.metric))}</text>'
        )
        parts.append(
            f'<text x="190" y="{y + 76}" font-family="DejaVu Sans, Arial, sans-serif" font-size="19" '
            f'fill="#0f172a">观测值：{html.escape(zh_observed(row.observed))}</text>'
        )
        parts.append(
            f'<text x="190" y="{y + 103}" font-family="DejaVu Sans, Arial, sans-serif" font-size="18" '
            f'fill="#334155">判据：{html.escape(zh_constraint(row.constraint))}</text>'
        )
        parts.append(
            f'<text x="190" y="{y + 129}" font-family="DejaVu Sans, Arial, sans-serif" font-size="18" '
            f'font-weight="700" fill="{fg}">{zh_gap_label(row.gap_x)}</text>'
        )

        # Simple right-hand severity bar, log-scaled and capped.
        if math.isinf(row.gap_x):
            severity = 1.0
        else:
            severity = min(1.0, max(0.0, math.log10(max(row.gap_x, 1.0) + 1.0) / 2.0))
        bar_x = width - 330
        bar_y = y + 103
        bar_w = 260
        bar_h = 20
        fill_w = int(bar_w * severity)
        parts.append(f'<rect x="{bar_x}" y="{bar_y}" width="{bar_w}" height="{bar_h}" rx="6" fill="#fee2e2"/>')
        parts.append(
            f'<rect x="{bar_x}" y="{bar_y}" width="{fill_w}" height="{bar_h}" rx="6" fill="#dc2626"/>'
        )
        parts.append(
            f'<text x="{bar_x + bar_w / 2:.1f}" y="{bar_y - 8}" text-anchor="middle" '
            f'font-family="DejaVu Sans, Arial, sans-serif" font-size="14" fill="#7f1d1d">'
            f'严重度（对数刻度）</text>'
        )
        y += row_h + gap

    parts.append("</svg>")
    svg_path.parent.mkdir(parents=True, exist_ok=True)
    svg_path.write_text("\n".join(parts), encoding="utf-8")


def run_simulation(build_dir: Path, macro: str) -> str:
    exe = build_dir / "exampleB1"
    if not exe.exists():
        raise FileNotFoundError(f"Executable not found: {exe}")
    completed = subprocess.run(
        [str(exe), macro],
        cwd=str(build_dir),
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    return completed.stdout


def main() -> None:
    args = parse_args()
    build_dir = Path(args.build_dir).resolve()
    log_path = Path(args.log_out).resolve()
    csv_path = Path(args.csv_out).resolve()
    svg_path = Path(args.svg_out).resolve()

    full_log = run_simulation(build_dir, args.macro)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(full_log, encoding="utf-8")

    global_block = extract_global_summary(full_log)
    summary = parse_summary(global_block)
    rows = build_rows(summary)
    write_csv(rows, csv_path)
    render_svg(rows, summary, svg_path)

    print(f"Log: {log_path}")
    print(f"CSV: {csv_path}")
    print(f"SVG: {svg_path}")


if __name__ == "__main__":
    main()
