"""Per-toggle checks for the enhancements (LAUNCHER_PLAN.md §7.9, docs/TESTING.md).

    python -I tools/tests/enhancement_cases.py [--build port/build] [--assets DIR] [case ...|all]
Each case makes fresh portable copies, runs a deterministic seeded game (fixed step, scripted taps),
and keeps frame dumps + statistics in <out>/<case>/ for inspection; the summary line per run shows
the key stats. Cases: default flash shake logos god level overlay snapshot seed statsfix pounce ja
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

A = None
CASES = {}


def run(name, enh, taps=None, frames=3600, every=60, env=None, args=(), game=None):
    d = os.path.join(A.out, name)
    c.make_portable(d, A.build, A.assets, {"game": dict(game or {}), "enhancements": enh})
    e = c.seeded_env(frames, every, taps if taps is not None else c.taps_default())
    e.update(env or {})
    rc = c.run_game(d, e, args, timeout=900)
    s = c.stats(d)
    n = len(os.listdir(os.path.join(d, "frames")))
    print("[%s] rc=%s frames=%d games=%.0f kills=%.0f purple=%.0f yellow=%.0f red=%.0f" % (
        name, rc, n, s.get("gamesPlayed", 0), s.get("kills", 0), s.get("purpleKills", 0),
        s.get("yellowKills", 0), s.get("redKills", 0)))
    return d, s


def case(fn):
    CASES[fn.__name__] = fn
    return fn


def compare(a, b, label):
    diff, n = c.framediff(a, b)
    print("  %s: %d/%d frames differ" % (label, len(diff), n))


@case
def default():
    run("default", {})


@case
def flash():
    a, _ = run("flash_off", {}, every=5, frames=1500)
    b, _ = run("flash_on", {"reduce_flashing": True}, every=5, frames=1500)
    compare(a, b, "reduce_flashing")


@case
def shake():
    a, _ = run("shake_full", {}, every=10, frames=1500)
    b, _ = run("shake_zero", {"screen_shake": 0.0}, every=10, frames=1500)
    compare(a, b, "screen_shake")


@case
def logos():
    a, _ = run("logos_off", {}, every=30, frames=300, taps="")
    b, _ = run("logos_on", {"skip_logos": True}, every=30, frames=300, taps="")
    compare(a, b, "skip_logos")


@case
def god():
    run("god", {"god_mode": True}, frames=3600)


@case
def level():
    run("level3", {"start_enemy_level": 3, "show_overlay": True}, frames=900, every=60)


@case
def overlay():
    run("zones_precision", {"touch_zones": True, "show_overlay": True}, frames=600, every=30)
    run("zones_sling", {"touch_zones": True}, frames=600, every=30, game={"control_scheme": "slingshot"})


@case
def snapshot():
    run("snapshot_on", {"show_death_snapshot": True}, frames=1500, every=30)


@case
def seed():
    a, _ = run("seed_a", {"fixed_seed": 42}, frames=1200, env={"ZIGGURAT_SEED": ""})
    b, _ = run("seed_b", {"fixed_seed": 42}, frames=1200, env={"ZIGGURAT_SEED": ""})
    d, _ = run("seed_c", {"fixed_seed": 1234}, frames=1200, env={"ZIGGURAT_SEED": ""})
    compare(a, b, "same seed")
    compare(a, d, "different seed")


@case
def statsfix():
    # long god-mode run (test hook keeps stats recording) to reach purple / toppling kills
    taps = c.taps_long(600)
    hooks = {"ZIGGURAT_GODMODE": "1", "ZIGGURAT_KILL_AT": "560"}
    run("stats_bugs", {}, taps=taps, frames=19000, every=1000, env=hooks)
    run("stats_fixed", {"fix_purple_double_count": True, "fix_yellow_as_red": True}, taps=taps, frames=19000,
        every=1000, env=hooks)


@case
def pounce():
    taps = c.taps_long(300)
    a, _ = run("pounce_orig", {}, taps=taps, frames=9000, every=300, env={"ZIGGURAT_GODMODE": "1"})
    b, _ = run("pounce_fix", {"fix_red_pounce": True}, taps=taps, frames=9000, every=300, env={"ZIGGURAT_GODMODE": "1"})
    compare(a, b, "fix_red_pounce")


@case
def ja():
    # die, tap the options gear on the game-over screen, then hold on the Options screen
    taps = ";".join(c.taps_default().split(";")[:42] + ["80000:440:295:150"])
    run("ja_off", {}, taps=taps, frames=2700, every=150, env={"ZIGGURAT_LANG": "ja"})
    run("ja_on", {"show_ja_overlay": True}, taps=taps, frames=2700, every=150, env={"ZIGGURAT_LANG": "ja"})


def main():
    global A
    A = c.parse_args("enhancement_cases", lambda ap: ap.add_argument("cases", nargs="*"))
    names = A.cases or ["default"]
    for n in (list(CASES) if names == ["all"] else names):
        if n not in CASES:
            raise SystemExit("unknown case %s (have: %s)" % (n, " ".join(CASES)))
        CASES[n]()


if __name__ == "__main__":
    main()
