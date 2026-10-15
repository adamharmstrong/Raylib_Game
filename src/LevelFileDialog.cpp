#include "LevelFileDialog.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#endif

std::string PickCustomLevelFile() {
#if defined(_WIN32)
    wchar_t path[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFile = path;
    dialog.nMaxFile = 32768;
    dialog.lpstrFilter = L"Power Pulley Panic levels (*.level)\0*.level\0\0";
    dialog.lpstrTitle = L"Load Custom Level";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) return std::filesystem::path(path).string();
#endif
    return {};
}
