#!/usr/bin/env python3
"""Mirror of CalcCards() in src/main.cpp, for layout sanity checks only.

This is NOT a Windows test: it verifies the C++ layout math in the
abstract, for the client sizes the device will actually produce once
Per-Monitor V2 DPI awareness is on (physical pixels, not virtualised
sizes). At 175% scaling the physical client size of a 1080p display
is still 1920x1080, so the rects at 100/150/175% are identical — that
is the whole point of the v1.0.4 DPI fix. Run: python3 tools/layout_check.py
"""
import sys

def calc(cw, ch, focus):
    gap = cw * 0.016
    base_w = (cw * 0.80 - gap * 3) / 4.0
    base_h = base_w * 1448.0 / 1086.0
    max_h = ch * 0.52
    if base_h > max_h:
        base_h = max_h
        base_w = base_h * 1086.0 / 1448.0
    row_w = base_w * 4 + gap * 3
    start_x = (cw - row_w) / 2.0
    top_y = ch * 0.32
    out = []
    for i in range(4):
        x, w, h, y = start_x + i * (base_w + gap), base_w, base_h, top_y
        if i == focus:
            gw, gh = w * 0.08, h * 0.08
            x -= gw / 2; y -= gh / 2; w += gw; h += gh
        out.append((x, y, x + w, y + h))
    return out

def check(cw, ch, label):
    for focus in range(4):
        cards = calc(cw, ch, focus)
        for i, (x0, y0, x1, y1) in enumerate(cards):
            assert 0 <= x0 < x1 <= cw, f"{label} focus={focus} card {i} x out of bounds: {cards[i]}"
            assert 0 <= y0 < y1 <= ch, f"{label} focus={focus} card {i} y out of bounds: {cards[i]}"
            # Card aspect ratio must stay 1086:1448 (no stretching).
            ratio = (x1 - x0) / (y1 - y0)
            assert abs(ratio - 1086 / 1448) < 0.001, f"{label} card {i} stretched: ratio {ratio}"
        # Focused growth must not overlap its neighbours (gap > growth).
        for a in range(4):
            for b in range(a + 1, 4):
                ax0, ay0, ax1, ay1 = cards[a]
                bx0, by0, bx1, by1 = cards[b]
                overlap = not (ax1 <= bx0 or bx1 <= ax0 or ay1 <= by0 or by1 <= ay0)
                assert not overlap, f"{label} focus={focus} cards {a},{b} overlap"
        # Title (12% height), status bar (82%), footer (88%) must not
        # collide with the card row.
        row_top = min(c[1] for c in cards)
        row_bottom = max(c[3] for c in cards)
        assert row_bottom < ch * 0.82, f"{label} card row hits status bar"
        assert row_top > ch * 0.12 + 60, f"{label} card row hits title"
    print(f"OK {label}: {cw}x{ch}, all focuses inside, aspect preserved, no overlap")

def main():
    # 1080p physical at any scaling (the v1.0.4 case), plus windowed.
    check(1920, 1080, "1080p (100/150/175% physical)")
    check(1280, 720, "windowed 1280x720")
    check(2560, 1440, "1440p physical")
    # The old broken case, for comparison: a virtualised 175% client
    # size would still lay out inside itself, but the OS then scaled
    # the whole window beyond the screen. Per-Monitor V2 removes that
    # scaling, so this size should never be produced fullscreen now.
    check(1097, 617, "virtualised 175% (legacy, must not occur fullscreen)")
    print("layout_check PASS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
