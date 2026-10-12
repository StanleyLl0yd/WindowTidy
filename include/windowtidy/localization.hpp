#pragma once
#include <windows.h>
#include <array>
#include <cstddef>

namespace wt {
enum class Language { English, Russian };

constexpr Language LanguageForUiLang(LANGID language) noexcept {
    return PRIMARYLANGID(language) == LANG_RUSSIAN ? Language::Russian : Language::English;
}

enum class TextId {
    SettingsTitle,
    GlobalHotkeys,
    TidyLabel,
    TopmostLabel,
    NextLabel,
    PreviousLabel,
    MinimizeLabel,
    Behavior,
    SkipTopmost,
    OnlyCurrent,
    MoveModeLabel,
    RunAtStartup,
    HotkeyHelp,
    Save,
    Cancel,
    Preserve,
    Center,
    Maximize,
    TrayTidy,
    TrayUndo,
    TrayTopmost,
    TrayNext,
    TrayPrevious,
    TrayMinimize,
    TraySettings,
    TrayStartup,
    TrayDiagnostics,
    TrayExit,
    ActionTidy,
    ActionTopmost,
    ActionNext,
    ActionPrevious,
    ActionMinimize,
    WarnBadHotkey,
    WarnRegistration,
    WarnRegistrationDetail,
    WarnCannotSave,
    WarnCannotAutostart,
    WarnCannotStartup,
    DiagnosticTitle,
    DiagnosticIntro,
    DiagnosticActive,
    DiagnosticUnavailable,
    DiagnosticDisabled,
    DiagnosticAdvice,
};

struct Translation { const wchar_t* english; const wchar_t* russian; };
inline constexpr std::array<Translation, 45> kTranslations{{
    {L"Window Tidy Settings", L"Настройки Window Tidy"},
    {L"Global hotkeys", L"Глобальные горячие клавиши"},
    {L"Tidy / Undo:", L"Свернуть / вернуть:"},
    {L"Always on top:", L"Поверх всех окон:"},
    {L"Move to next monitor:", L"На следующий монитор:"},
    {L"Move to previous:", L"На предыдущий монитор:"},
    {L"Minimize active:", L"Свернуть активное:"},
    {L"Behavior", L"Поведение"},
    {L"Skip always-on-top windows in Tidy", L"Не сворачивать окна поверх всех"},
    {L"Tidy only on current monitor", L"Сворачивать только на текущем мониторе"},
    {L"Move mode:", L"Режим перемещения:"},
    {L"Start with Windows", L"Запускать вместе с Windows"},
    {L"Click X to disable a hotkey.", L"Нажмите X, чтобы отключить комбинацию."},
    {L"Save", L"Сохранить"},
    {L"Cancel", L"Отмена"},
    {L"Preserve relative position", L"Сохранить относительное положение"},
    {L"Center on destination", L"По центру целевого монитора"},
    {L"Move and maximize", L"Переместить и развернуть"},
    {L"Tidy / Undo", L"Свернуть / вернуть окна"},
    {L"Undo Tidy", L"Вернуть свёрнутые окна"},
    {L"Toggle always on top", L"Переключить поверх всех окон"},
    {L"Move to next monitor", L"Переместить на следующий монитор"},
    {L"Move to previous monitor", L"Переместить на предыдущий монитор"},
    {L"Minimize active window", L"Свернуть активное окно"},
    {L"Settings...", L"Настройки..."},
    {L"Start with Windows", L"Запускать вместе с Windows"},
    {L"Hotkey diagnostics...", L"Диагностика горячих клавиш..."},
    {L"Exit", L"Выход"},
    {L"Tidy", L"Свернуть / вернуть"},
    {L"Always on top", L"Поверх всех окон"},
    {L"Next monitor", L"Следующий монитор"},
    {L"Previous monitor", L"Предыдущий монитор"},
    {L"Minimize", L"Свернуть"},
    {L"Shortcuts must be unique. Each enabled shortcut must include Ctrl, Alt or Shift.", L"Комбинации должны быть уникальными и содержать Ctrl, Alt или Shift."},
    {L"Cannot register global shortcut: ", L"Не удалось зарегистрировать горячую клавишу: "},
    {L".\n\nAnother application or Windows may already use this combination. See Hotkey diagnostics in the tray menu.", L".\n\nКомбинация может быть занята Windows или другой программой. Подробности в меню трея: «Диагностика горячих клавиш»."},
    {L"Could not save all settings to the current user's registry.", L"Не удалось сохранить все настройки в реестре текущего пользователя."},
    {L"Could not update the Windows startup registry value.", L"Не удалось обновить автозапуск Windows в реестре."},
    {L"Could not update Windows startup.", L"Не удалось изменить автозапуск Windows."},
    {L"Hotkey diagnostics", L"Диагностика горячих клавиш"},
    {L"Global hotkey registration status:\n", L"Состояние регистрации глобальных горячих клавиш:\n"},
    {L"registered", L"зарегистрирована"},
    {L"unavailable", L"недоступна"},
    {L"disabled", L"отключена"},
    {L"\nIf a combination is unavailable, reassign it in Settings. A working hotkey does not guarantee that Windows will allow an action on every window.", L"\nЕсли комбинация недоступна, переназначьте её в настройках. Работающая горячая клавиша не гарантирует возможность управления любым окном."},
}};

constexpr const wchar_t* Text(TextId id, Language language) noexcept {
    const auto& item = kTranslations[static_cast<std::size_t>(id)];
    return language == Language::Russian ? item.russian : item.english;
}
} // namespace wt
