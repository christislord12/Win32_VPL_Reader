#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>
#include <sstream>

// Link essential Windows libraries
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")

#define ID_COMBO_FILE 101
#define ID_COMBO_BOOK 102
#define ID_TEXT_AREA  103

HINSTANCE hInst;
HWND hWndComboFile, hWndComboBook, hWndEdit;
// We store data as wstring (UTF-16) for easy display
std::map<std::wstring, std::vector<std::wstring>> currentBookMap;
std::string bibleDirPath = "./bibles";

// Helper to convert UTF-8 (from file) to UTF-16 (for Windows UI)
std::wstring Utf8ToUtf16(const std::string& utf8Str) {
    if (utf8Str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8Str[0], (int)utf8Str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8Str[0], (int)utf8Str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void LoadVplFiles(HWND hCombo);
void ParseVplFile(std::wstring fileName);
void DisplayBook(std::wstring bookId);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    if (strlen(lpCmdLine) > 0) bibleDirPath = lpCmdLine;

    InitCommonControls();

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"BibleReaderClass";

    if (!RegisterClassExW(&wc)) return 0;

    HWND hWnd = CreateWindowExW(0, L"BibleReaderClass", L"Bible Reader",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CreateWindowExW(0, L"STATIC", L"Version:", WS_CHILD | WS_VISIBLE, 10, 15, 60, 20, hWnd, NULL, hInst, NULL);
        hWndComboFile = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 70, 10, 180, 200, hWnd, (HMENU)ID_COMBO_FILE, hInst, NULL);
        
        CreateWindowExW(0, L"STATIC", L"Book:", WS_CHILD | WS_VISIBLE, 260, 15, 50, 20, hWnd, NULL, hInst, NULL);
        hWndComboBook = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 310, 10, 100, 200, hWnd, (HMENU)ID_COMBO_BOOK, hInst, NULL);

        hWndEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 10, 50, 760, 500, hWnd, (HMENU)ID_TEXT_AREA, hInst, NULL);

        // Standard fonts like Times New Roman support Greek characters
        HFONT hFont = CreateFontW(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_ROMAN, L"Times New Roman");
        SendMessage(hWndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        LoadVplFiles(hWndComboFile);
        break;
    }
    case WM_SIZE: {
        MoveWindow(hWndEdit, 10, 50, LOWORD(lParam) - 25, HIWORD(lParam) - 70, TRUE);
        break;
    }
    case WM_COMMAND: {
        if (HIWORD(wParam) == CBN_SELCHANGE) {
            if (LOWORD(wParam) == ID_COMBO_FILE) {
                wchar_t fileName[256];
                GetWindowTextW(hWndComboFile, fileName, 256);
                ParseVplFile(fileName);
            }
            else if (LOWORD(wParam) == ID_COMBO_BOOK) {
                wchar_t bookId[10];
                GetWindowTextW(hWndComboBook, bookId, 10);
                DisplayBook(bookId);
            }
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

void LoadVplFiles(HWND hCombo) {
    if (!std::filesystem::exists(bibleDirPath)) return;
    for (const auto& entry : std::filesystem::directory_iterator(bibleDirPath)) {
        if (entry.path().extension() == ".txt") {
            std::wstring fname = entry.path().filename().wstring();
            SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)fname.c_str());
        }
    }
    if (SendMessage(hCombo, CB_GETCOUNT, 0, 0) > 0) {
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
        wchar_t fileName[256];
        GetWindowTextW(hCombo, fileName, 256);
        ParseVplFile(fileName);
    }
}

void ParseVplFile(std::wstring fileName) {
    currentBookMap.clear();
    SendMessage(hWndComboBook, CB_RESETCONTENT, 0, 0);

    // Build path (converting wstring fileName to string for filesystem if needed)
    std::filesystem::path p = bibleDirPath;
    p /= fileName;

    std::ifstream file(p, std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        if (line.length() < 4) continue;

        // Convert UTF-8 line from file to UTF-16 wstring
        std::wstring wLine = Utf8ToUtf16(line);
        std::wstring wBookId = wLine.substr(0, 3);
        wBookId.erase(wBookId.find_last_not_of(L" \n\r\t") + 1);

        if (currentBookMap.find(wBookId) == currentBookMap.end()) {
            currentBookMap[wBookId] = std::vector<std::wstring>();
            SendMessageW(hWndComboBook, CB_ADDSTRING, 0, (LPARAM)wBookId.c_str());
        }
        currentBookMap[wBookId].push_back(wLine);
    }

    if (SendMessage(hWndComboBook, CB_GETCOUNT, 0, 0) > 0) {
        SendMessage(hWndComboBook, CB_SETCURSEL, 0, 0);
        wchar_t bookId[10];
        GetWindowTextW(hWndComboBook, bookId, 10);
        DisplayBook(bookId);
    }
}

void DisplayBook(std::wstring bookId) {
    if (currentBookMap.count(bookId) == 0) return;
    std::wstring fullText = L"";
    for (const auto& verse : currentBookMap[bookId]) {
        fullText += verse + L"\r\n";
    }
    SetWindowTextW(hWndEdit, fullText.c_str());
}
