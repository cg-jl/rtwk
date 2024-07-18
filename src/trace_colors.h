#pragma once

namespace filters {
static constexpr bool surfaceHit = true;
static constexpr bool deepHit = true;
static constexpr bool treeHit = true;
static constexpr bool hit = surfaceHit && deepHit;
}

// Using Catppuccin Frappe theming
// https://catppuccin.com/palette
enum Ctp : int {
    Rosewater = 0xf2d5cf,
    Flamingo = 0xeebebe,
    Pink = 0xf4b8e4,
    Mauve = 0xca9ee6,
    Red = 0xe78284,
    Maroon = 0xea999c,
    Peach = 0xef9f76,
    Yellow = 0xe5c890,
    Green = 0xa6d189,
    Teal = 0x81c8be,
    Sky = 0x99d1db,
    Sapphire = 0x85c1dc,
    Blue = 0x8caaee,
    Lavender = 0xbabbf1,
    Text = 0xc6d0f5,
    Subtext_1 = 0xb5bfe2,
    Subtext_0 = 0xa5adce,
    Overlay_2 = 0x949cbb,
    Overlay_1 = 0x838ba7,
    Overlay_0 = 0x737994,
    Surface_2 = 0x626880,
    Surface_1 = 0x51576d,
    Surface_0 = 0x414559,
    Base = 0x303446,
    Mantle = 0x292c3c,
    Crust = 0x232634,
};

#define ZoneTextL(text) ZoneText(text, sizeof(text) - 1)
