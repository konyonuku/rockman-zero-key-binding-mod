#pragma once

// Everything the binding window puts on screen, in the three languages the mod
// ships with. English is the default; the player picks another from the box in
// the top right and the choice is remembered in keybind.toml.

enum Lang {
    kLangEnglish = 0,
    kLangJapanese = 1,
    kLangKorean = 2,
    kLangCount = 3,
};

struct UiText {
    const wchar_t* langName;          // shown in the language box
    const wchar_t* windowTitle;
    const wchar_t* warn1;
    const wchar_t* warn2;
    const wchar_t* warn3;
    const wchar_t* colKey;
    const wchar_t* gameZero;
    const wchar_t* gameZx;
    const wchar_t* groupMove;
    const wchar_t* groupAction;
    const wchar_t* groupSubDisplay;
    const wchar_t* groupSystem;
    const wchar_t* keyListButton;
    const wchar_t* showNextTime;
    const wchar_t* showNextTimeHint;
    const wchar_t* reset;
    const wchar_t* applyAndClose;
    const wchar_t* pressAKey;
    const wchar_t* rightClickCancels;
    const wchar_t* defaultIs;         // takes the default key name
    const wchar_t* clashTitle;
    const wchar_t* clashHead;         // takes the key name
    const wchar_t* clashHint;
    const wchar_t* resetTitle;
    const wchar_t* resetHead;
    const wchar_t* resetBody;
    const wchar_t* resetAsk;
    const wchar_t* ok;
    const wchar_t* cancel;
    const wchar_t* yes;
    const wchar_t* no;
    const wchar_t* close;
    const wchar_t* keyListTitle;
    const wchar_t* keyListIntro;      // takes the number of keys
    const wchar_t* catArrows;
    const wchar_t* catLetters;
    const wchar_t* catDigits;
    const wchar_t* catSymbols;
    const wchar_t* catSpecial;
    const wchar_t* catModifiers;
};

extern const UiText kUi[kLangCount];

// What one of the game keys does, per game. Null when that game does not use
// the key, which the window draws as a dash.
const wchar_t* ActionText(int lang, unsigned char gameKey, bool zx);

// The legend printed on a key button: L"A", L"SPACE", L"↑" and so on.
// Latin in every language, the way a keyboard is labelled.
const wchar_t* KeyLabel(unsigned char dik);
