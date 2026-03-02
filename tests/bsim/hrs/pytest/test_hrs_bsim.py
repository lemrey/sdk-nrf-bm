# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# Pytest that runs the HRS bsim two-peer test (compile + run script).
# One binary is launched twice with -testid=peripheral and -testid=central, then PHY.
# Requires BSIM_OUT_PATH and ZEPHYR_BASE. Exit code 0 = pass, non-zero = fail.

import os
import subprocess
from pathlib import Path

import pytest

# pytest is in tests/bsim/hrs/pytest/ -> nrf-bm root is 4 levels up
NRF_BM_ROOT = Path(__file__).resolve().parent.parent.parent.parent
COMPILE_SCRIPT = NRF_BM_ROOT / "bsim/hrs/compile_hrs_bsim.sh"
RUN_SCRIPT = NRF_BM_ROOT / "bsim/hrs/tests_scripts/hrs_service.sh"


@pytest.fixture(scope="module")
def env_required():
    bsim_out = os.environ.get("BSIM_OUT_PATH")
    zephyr_base = os.environ.get("ZEPHYR_BASE")
    if not bsim_out or not zephyr_base:
        pytest.skip(
            "BSIM_OUT_PATH and ZEPHYR_BASE must be set to run the bsim test "
            "(e.g. source zephyr/tests/bsim/sh_common.source)"
        )
    return {"BSIM_OUT_PATH": bsim_out, "ZEPHYR_BASE": zephyr_base}


def test_hrs_two_peers(env_required):
    """Build test binary, run peripheral + central under Babblesim; pass if run script exits 0."""
    env = os.environ.copy()
    env.update(env_required)

    assert COMPILE_SCRIPT.is_file(), f"Compile script not found: {COMPILE_SCRIPT}"
    assert RUN_SCRIPT.is_file(), f"Run script not found: {RUN_SCRIPT}"

    # Build
    r = subprocess.run(
        [str(COMPILE_SCRIPT)],
        env=env,
        cwd=NRF_BM_ROOT,
        capture_output=True,
        text=True,
        timeout=300,
    )
    assert r.returncode == 0, (
        f"Compile script failed (exit {r.returncode}):\n{r.stderr}\n{r.stdout}"
    )

    # Run peripheral + central + phy; script exits 0 on success
    r = subprocess.run(
        [str(RUN_SCRIPT)],
        env=env,
        cwd=os.path.join(env["BSIM_OUT_PATH"], "bin"),
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert r.returncode == 0, (
        f"Run script failed (exit {r.returncode}):\n{r.stderr}\n{r.stdout}"
    )
