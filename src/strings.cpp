#include "strings.h"

#include "keys.h"

const UiText kUi[kLangCount] = {
    {   // English
        L"English",
        L"KeyRebind - Key Settings",
        L"The game must be set to Key Layout A (the default).",
        L"Set Layout A in the game options before",
        L"changing any keys here.",
        L"Key",
        L"Mega Man Zero",
        L"Mega Man ZX",
        L"Movement",
        L"Actions",
        L"Sub-Display (ZX only)",
        L"System",
        L"Show bindable keys",
        L"Show this window again next time the game starts",
        L"Uncheck and the game starts straight away. You can always reopen "
        L"this window with KeyRebindConfig.exe.",
        L"Reset",
        L"Apply and Close",
        L"Press a key",
        L"right click to cancel",
        L"was %s",
        L"Key already in use",
        L"%s is already in use.",
        L"Pick another key, or change that action first.",
        L"Reset",
        L"Reset every key to its default.",
        L"Your changes will be lost.",
        L"Reset now?",
        L"OK",
        L"Cancel",
        L"Yes",
        L"No",
        L"Close",
        L"Bindable keys",
        L"Any of these %d keys can be used.",
        L"Arrows",
        L"Letters",
        L"Digits",
        L"Symbols",
        L"Special",
        L"Modifiers",
    },
    {   // Japanese
        L"日本語",
        L"KeyRebind - キー設定",
        L"ゲームのキー配置は Layout A（初期設定）である必要があります。",
        L"ゲームのオプションで Layout A に設定してから",
        L"キーを変更してください。",
        L"キー",
        L"ロックマンゼロ",
        L"ロックマンゼクス",
        L"移動",
        L"アクション",
        L"サブディスプレイ（ZX のみ）",
        L"システム",
        L"変更できるキー一覧",
        L"次回のゲーム起動時もこの画面を表示する",
        L"チェックを外すとすぐにゲームが始まります。"
        L"KeyRebindConfig.exe からいつでも開けます。",
        L"初期化",
        L"適用して閉じる",
        L"キーを押してください",
        L"右クリックで取り消し",
        L"初期 %s",
        L"キーの重複",
        L"%s は既に使われています。",
        L"別のキーを選ぶか、先にそちらを変更してください。",
        L"初期化",
        L"すべてのキーを初期設定に戻します。",
        L"変更した内容は失われます。",
        L"初期化しますか？",
        L"OK",
        L"キャンセル",
        L"はい",
        L"いいえ",
        L"閉じる",
        L"変更できるキー",
        L"次の %d 個のキーから選べます。",
        L"方向キー",
        L"文字",
        L"数字",
        L"記号",
        L"特殊",
        L"修飾キー",
    },
    {   // Korean
        L"한국어",
        L"KeyRebind - 키 설정",
        L"게임의 Key Layout이 Layout A(기본값)여야 합니다.",
        L"게임 설정에서 Layout A로 맞춘 뒤",
        L"키를 변경하세요.",
        L"키",
        L"록맨 ZERO",
        L"록맨 ZX",
        L"이동",
        L"액션",
        L"서브 디스플레이 (ZX 전용)",
        L"시스템",
        L"바인딩 가능한 키 보기",
        L"다음 게임 실행 시에도 이 창을 띄웁니다",
        L"해제하면 다음부터 바로 게임이 시작됩니다. "
        L"언제든 KeyRebindConfig.exe로 다시 열 수 있습니다.",
        L"초기화",
        L"적용 후 종료",
        L"키를 누르세요",
        L"우클릭하면 취소",
        L"기본 %s",
        L"키 중복",
        L"%s 키는 이미 사용 중입니다.",
        L"다른 키를 고르거나, 먼저 그 동작의 키를 바꾸세요.",
        L"초기화",
        L"모든 키를 기본값으로 되돌립니다.",
        L"지금까지 변경한 내용은 사라집니다.",
        L"정말 초기화하시겠습니까?",
        L"확인",
        L"취소",
        L"예",
        L"아니오",
        L"닫기",
        L"바인딩 가능한 키",
        L"아래 %d개 키 중에서 고를 수 있습니다.",
        L"방향키",
        L"문자",
        L"숫자",
        L"기호",
        L"특수",
        L"수정자",
    },
};

namespace {

// One row of the window: the stock key, then what it does in each game, in
// each language. A null means the game does not use that key.
struct Action {
    unsigned char dik;
    const wchar_t* text[kLangCount][2];   // [language][0 = Zero, 1 = ZX]
};

const Action kActions[] = {
    { 0xC8, {
        { L"Move / Menu (Up)", L"Move / Menu (Up)" },
        { L"移動 / メニュー（上）", L"移動 / メニュー（上）" },
        { L"위로 이동 / 메뉴 위", L"위로 이동 / 메뉴 위" } } },
    { 0xD0, {
        { L"Move / Menu (Down)", L"Move / Menu (Down)" },
        { L"移動 / メニュー（下）", L"移動 / メニュー（下）" },
        { L"아래로 이동 / 메뉴 아래", L"아래로 이동 / 메뉴 아래" } } },
    { 0xCB, {
        { L"Move / Menu (Left)", L"Move / Menu (Left)" },
        { L"移動 / メニュー（左）", L"移動 / メニュー（左）" },
        { L"왼쪽 이동 / 메뉴 왼쪽", L"왼쪽 이동 / 메뉴 왼쪽" } } },
    { 0xCD, {
        { L"Move / Menu (Right)", L"Move / Menu (Right)" },
        { L"移動 / メニュー（右）", L"移動 / メニュー（右）" },
        { L"오른쪽 이동 / 메뉴 오른쪽", L"오른쪽 이동 / 메뉴 오른쪽" } } },

    { 0x39, {
        { L"Attack / Confirm", L"Jump / Wall Jump / Confirm" },
        { L"攻撃 / 決定", L"ジャンプ / 壁蹴り / 決定" },
        { L"공격 / 메뉴 확인", L"점프 / 벽차기 / 메뉴 확인" } } },
    { 0x1E, {
        { L"Jump / Wall Jump / Back", L"Overdrive / Back" },
        { L"ジャンプ / 壁蹴り / キャンセル", L"オーバードライブ / キャンセル" },
        { L"점프 / 벽차기 / 메뉴 취소", L"오버드라이브 / 메뉴 취소" } } },
    { 0x20, {
        { L"Dash", L"Dash" },
        { L"ダッシュ", L"ダッシュ" },
        { L"대시", L"대시" } } },
    { 0x2E, {
        { L"Use Sub Weapon", L"Sub Weapon Attack" },
        { L"サブウェポン使用", L"サブウェポン攻撃" },
        { L"서브 웨폰 사용", L"서브 웨폰 공격" } } },
    { 0x2D, {
        { nullptr, L"Main Weapon Attack" },
        { nullptr, L"メインウェポン攻撃" },
        { nullptr, L"메인 웨폰 공격" } } },
    { 0x1F, {
        { nullptr, L"Megamerge" },
        { nullptr, L"メガマージ" },
        { nullptr, L"메가머지" } } },
    { 0x21, {
        { nullptr, L"Toggle Sub-Display" },
        { nullptr, L"サブディスプレイ 表示切替" },
        { nullptr, L"서브 디스플레이 켜기 / 끄기" } } },

    { 0x2C, {
        { nullptr, L"Touch Sub-Display" },
        { nullptr, L"サブディスプレイをタッチ" },
        { nullptr, L"커서로 서브 디스플레이 터치" } } },
    { 0x1C, {
        { nullptr, L"Touch Sub-Display" },
        { nullptr, L"サブディスプレイをタッチ" },
        { nullptr, L"커서로 서브 디스플레이 터치" } } },
    { 0x25, {
        { nullptr, L"Cursor Up" },
        { nullptr, L"カーソル（上）" },
        { nullptr, L"커서 위" } } },
    { 0x32, {
        { nullptr, L"Cursor Left" },
        { nullptr, L"カーソル（左）" },
        { nullptr, L"커서 왼쪽" } } },
    { 0x33, {
        { nullptr, L"Cursor Down" },
        { nullptr, L"カーソル（下）" },
        { nullptr, L"커서 아래" } } },
    { 0x34, {
        { nullptr, L"Cursor Right" },
        { nullptr, L"カーソル（右）" },
        { nullptr, L"커서 오른쪽" } } },

    { 0x0F, {
        { L"Open / Close Options", L"Open / Close Options" },
        { L"ゲーム内オプション 開閉", L"ゲーム内オプション 開閉" },
        { L"게임 내 옵션 열기 / 닫기", L"게임 내 옵션 열기 / 닫기" } } },
    { 0x01, {
        { L"Start / Sub-screen / Skip Movie", L"Start / Sub-screen / Skip Movie" },
        { L"ゲーム開始 / サブ画面 / ムービースキップ",
          L"ゲーム開始 / サブ画面 / ムービースキップ" },
        { L"게임 시작 / 서브 스크린 / 무비 스킵",
          L"게임 시작 / 서브 스크린 / 무비 스킵" } } },
};

// Legends that are not just the name of the key.
struct Legend {
    unsigned char dik;
    const wchar_t* text;
};

const Legend kLegends[] = {
    { 0xC8, L"↑" }, { 0xD0, L"↓" }, { 0xCB, L"←" }, { 0xCD, L"→" },
    { 0x02, L"1" }, { 0x03, L"2" }, { 0x04, L"3" }, { 0x05, L"4" }, { 0x06, L"5" },
    { 0x07, L"6" }, { 0x08, L"7" }, { 0x09, L"8" }, { 0x0A, L"9" }, { 0x0B, L"0" },
    { 0x0C, L"-" }, { 0x0D, L"=" }, { 0x1A, L"[" }, { 0x1B, L"]" },
    { 0x27, L";" }, { 0x28, L"'" }, { 0x29, L"`" }, { 0x2B, L"\\" },
    { 0x33, L"," }, { 0x34, L"." }, { 0x35, L"/" },
    { 0x01, L"Esc" }, { 0x0F, L"Tab" }, { 0x1C, L"Enter" }, { 0x39, L"Space" },
    { 0x3A, L"CapsLock" },
    { 0x1D, L"LCtrl" }, { 0x9D, L"RCtrl" },
    { 0x2A, L"LShift" }, { 0x36, L"RShift" },
    { 0x38, L"LAlt" }, { 0xB8, L"RAlt" },
};

}  // namespace

const wchar_t* ActionText(int lang, unsigned char gameKey, bool zx) {
    if (lang < 0 || lang >= kLangCount)
        lang = kLangEnglish;
    for (int i = 0; i < int(sizeof(kActions) / sizeof(kActions[0])); ++i) {
        if (kActions[i].dik == gameKey)
            return kActions[i].text[lang][zx ? 1 : 0];
    }
    return nullptr;
}

const wchar_t* KeyLabel(unsigned char dik) {
    for (int i = 0; i < int(sizeof(kLegends) / sizeof(kLegends[0])); ++i) {
        if (kLegends[i].dik == dik)
            return kLegends[i].text;
    }
    // Everything left is a letter, whose name is already its legend.
    static wchar_t buf[8];
    const char* name = NameFromKey(dik);
    if (!name)
        return L"?";
    int i = 0;
    while (name[i] && i < 7) {
        buf[i] = (wchar_t)name[i];
        ++i;
    }
    buf[i] = 0;
    return buf;
}
