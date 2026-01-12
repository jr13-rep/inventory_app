#define UNICODE
#define _UNICODE
#include <windows.h>

#include <commctrl.h>
#include <sqlite3.h>

#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr wchar_t kWindowClassName[] = L"InventoryDatabaseWindow";
constexpr wchar_t kWindowTitle[] = L"Inventory Database";

enum ControlId {
    kNameEdit = 1001,
    kPartEdit,
    kNsnEdit,
    kSerialEdit,
    kQuantityEdit,
    kSaveButton,
    kUpdateButton,
    kDeleteButton,
    kSearchButton,
    kClearButton,
    kResultsView,
    kStatusLabel,
};

struct AppState {
    sqlite3* db = nullptr;
    int selected_id = -1;
    HWND name_edit = nullptr;
    HWND part_edit = nullptr;
    HWND nsn_edit = nullptr;
    HWND serial_edit = nullptr;
    HWND quantity_edit = nullptr;
    HWND results_view = nullptr;
    HWND status_label = nullptr;
};

AppState g_state;

std::wstring GetText(HWND handle) {
    int length = GetWindowTextLengthW(handle);
    std::wstring text(static_cast<size_t>(length), L'\0');
    if (length > 0) {
        GetWindowTextW(handle, text.data(), length + 1);
    }
    return text;
}

void SetText(HWND handle, const std::wstring& text) {
    SetWindowTextW(handle, text.c_str());
}

std::string ToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring FromUtf8(const unsigned char* value) {
    if (!value) {
        return {};
    }
    int size =
        MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(value), -1, nullptr, 0);
    std::wstring result(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, reinterpret_cast<const char*>(value), -1, result.data(), size);
    return result;
}

bool ParseInt(const std::wstring& value, int& out) {
    if (value.empty()) {
        return false;
    }
    std::wistringstream stream(value);
    stream >> out;
    return !stream.fail() && stream.eof();
}

void SetStatus(const std::wstring& message) {
    SetText(g_state.status_label, message);
}

std::wstring GetDatabasePath() {
    wchar_t buffer[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::wstring path(buffer);
    size_t position = path.find_last_of(L"\\/");
    if (position != std::wstring::npos) {
        path.erase(position + 1);
    }
    path += L"inventory.db";
    return path;
}

bool ExecuteSql(const std::string& sql) {
    char* error_message = nullptr;
    int result = sqlite3_exec(g_state.db, sql.c_str(), nullptr, nullptr, &error_message);
    if (result != SQLITE_OK) {
        if (error_message) {
            sqlite3_free(error_message);
        }
        return false;
    }
    return true;
}

bool InitDatabase() {
    std::wstring db_path = GetDatabasePath();
    if (sqlite3_open(ToUtf8(db_path).c_str(), &g_state.db) != SQLITE_OK) {
        return false;
    }
    return ExecuteSql(
        "CREATE TABLE IF NOT EXISTS items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "part_number TEXT NOT NULL,"
        "nsn TEXT NOT NULL,"
        "serial_number TEXT NOT NULL,"
        "quantity INTEGER NOT NULL DEFAULT 0,"
        "created_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ")");
}

void ClearInputs() {
    SetText(g_state.name_edit, L"");
    SetText(g_state.part_edit, L"");
    SetText(g_state.nsn_edit, L"");
    SetText(g_state.serial_edit, L"");
    SetText(g_state.quantity_edit, L"");
    g_state.selected_id = -1;
    SetStatus(L"Ready");
}

void ConfigureListViewColumns(HWND list_view) {
    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    const std::vector<std::wstring> headers = {
        L"ID", L"Name", L"Part Number", L"NSN", L"Serial Number", L"Quantity", L"Created"};
    const std::vector<int> widths = {0, 180, 160, 140, 160, 90, 160};

    for (size_t i = 0; i < headers.size(); ++i) {
        column.pszText = const_cast<wchar_t*>(headers[i].c_str());
        column.cx = widths[i];
        column.iSubItem = static_cast<int>(i);
        ListView_InsertColumn(list_view, static_cast<int>(i), &column);
    }
}

void RefreshResults() {
    std::wstring name = GetText(g_state.name_edit);
    std::wstring part = GetText(g_state.part_edit);
    std::wstring nsn = GetText(g_state.nsn_edit);
    std::wstring serial = GetText(g_state.serial_edit);
    std::wstring quantity_text = GetText(g_state.quantity_edit);

    std::vector<std::string> conditions;
    std::vector<std::string> parameters;
    int quantity_value = 0;

    if (!name.empty()) {
        conditions.push_back("name LIKE ?");
        parameters.push_back("%" + ToUtf8(name) + "%");
    }
    if (!part.empty()) {
        conditions.push_back("part_number LIKE ?");
        parameters.push_back("%" + ToUtf8(part) + "%");
    }
    if (!nsn.empty()) {
        conditions.push_back("nsn LIKE ?");
        parameters.push_back("%" + ToUtf8(nsn) + "%");
    }
    if (!serial.empty()) {
        conditions.push_back("serial_number LIKE ?");
        parameters.push_back("%" + ToUtf8(serial) + "%");
    }
    if (!quantity_text.empty()) {
        if (!ParseInt(quantity_text, quantity_value)) {
            SetStatus(L"Quantity must be a whole number.");
            return;
        }
        conditions.push_back("quantity = ?");
    }

    std::string query =
        "SELECT id, name, part_number, nsn, serial_number, quantity, created_at FROM items";
    if (!conditions.empty()) {
        query += " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) {
                query += " AND ";
            }
            query += conditions[i];
        }
    }
    query += " ORDER BY created_at DESC";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_state.db, query.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        SetStatus(L"Search failed.");
        return;
    }

    int index = 1;
    for (const auto& value : parameters) {
        sqlite3_bind_text(statement, index++, value.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (!quantity_text.empty()) {
        sqlite3_bind_int(statement, index++, quantity_value);
    }

    ListView_DeleteAllItems(g_state.results_view);
    int row_count = 0;
    while (sqlite3_step(statement) == SQLITE_ROW) {
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        std::wstring id_text = std::to_wstring(sqlite3_column_int(statement, 0));
        item.iItem = row_count;
        item.pszText = id_text.data();
        int row_index = ListView_InsertItem(g_state.results_view, &item);

        std::vector<std::wstring> values = {
            id_text,
            FromUtf8(sqlite3_column_text(statement, 1)),
            FromUtf8(sqlite3_column_text(statement, 2)),
            FromUtf8(sqlite3_column_text(statement, 3)),
            FromUtf8(sqlite3_column_text(statement, 4)),
            std::to_wstring(sqlite3_column_int(statement, 5)),
            FromUtf8(sqlite3_column_text(statement, 6)),
        };

        for (size_t column = 1; column < values.size(); ++column) {
            ListView_SetItemText(
                g_state.results_view, row_index, static_cast<int>(column),
                const_cast<wchar_t*>(values[column].c_str()));
        }
        ++row_count;
    }
    sqlite3_finalize(statement);
    SetStatus(std::to_wstring(row_count) + L" record(s) found.");
}

void SaveRecord() {
    std::wstring name = GetText(g_state.name_edit);
    std::wstring part = GetText(g_state.part_edit);
    std::wstring nsn = GetText(g_state.nsn_edit);
    std::wstring serial = GetText(g_state.serial_edit);
    std::wstring quantity_text = GetText(g_state.quantity_edit);

    if (name.empty() || part.empty() || nsn.empty() || serial.empty()) {
        SetStatus(L"Please fill out all fields before saving.");
        return;
    }
    int quantity_value = 0;
    if (!ParseInt(quantity_text, quantity_value)) {
        SetStatus(L"Quantity must be a whole number.");
        return;
    }

    const char* sql =
        "INSERT INTO items (name, part_number, nsn, serial_number, quantity)"
        " VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_state.db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        SetStatus(L"Save failed.");
        return;
    }
    sqlite3_bind_text(statement, 1, ToUtf8(name).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, ToUtf8(part).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, ToUtf8(nsn).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, ToUtf8(serial).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 5, quantity_value);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        SetStatus(L"Save failed.");
        return;
    }
    sqlite3_finalize(statement);
    SetStatus(L"Record saved.");
    ClearInputs();
    RefreshResults();
}

void UpdateRecord() {
    if (g_state.selected_id < 0) {
        SetStatus(L"Select a record to update.");
        return;
    }

    std::wstring name = GetText(g_state.name_edit);
    std::wstring part = GetText(g_state.part_edit);
    std::wstring nsn = GetText(g_state.nsn_edit);
    std::wstring serial = GetText(g_state.serial_edit);
    std::wstring quantity_text = GetText(g_state.quantity_edit);

    if (name.empty() || part.empty() || nsn.empty() || serial.empty()) {
        SetStatus(L"Please fill out all fields before updating.");
        return;
    }
    int quantity_value = 0;
    if (!ParseInt(quantity_text, quantity_value)) {
        SetStatus(L"Quantity must be a whole number.");
        return;
    }

    const char* sql =
        "UPDATE items SET name = ?, part_number = ?, nsn = ?, serial_number = ?, quantity = ? "
        "WHERE id = ?";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_state.db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        SetStatus(L"Update failed.");
        return;
    }
    sqlite3_bind_text(statement, 1, ToUtf8(name).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, ToUtf8(part).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, ToUtf8(nsn).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, ToUtf8(serial).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 5, quantity_value);
    sqlite3_bind_int(statement, 6, g_state.selected_id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        SetStatus(L"Update failed.");
        return;
    }
    sqlite3_finalize(statement);
    SetStatus(L"Record updated.");
    ClearInputs();
    RefreshResults();
}

void DeleteRecord(HWND window) {
    if (g_state.selected_id < 0) {
        SetStatus(L"Select a record to delete.");
        return;
    }
    if (MessageBoxW(window, L"Delete the selected record?", L"Delete Record",
                    MB_ICONWARNING | MB_YESNO) != IDYES) {
        SetStatus(L"Delete cancelled.");
        return;
    }

    const char* sql = "DELETE FROM items WHERE id = ?";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(g_state.db, sql, -1, &statement, nullptr) != SQLITE_OK) {
        SetStatus(L"Delete failed.");
        return;
    }
    sqlite3_bind_int(statement, 1, g_state.selected_id);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        SetStatus(L"Delete failed.");
        return;
    }
    sqlite3_finalize(statement);
    SetStatus(L"Record deleted.");
    ClearInputs();
    RefreshResults();
}

void OnListViewSelect() {
    int selected = ListView_GetNextItem(g_state.results_view, -1, LVNI_SELECTED);
    if (selected < 0) {
        return;
    }

    wchar_t buffer[256] = {};
    ListView_GetItemText(g_state.results_view, selected, 0, buffer, 256);
    g_state.selected_id = _wtoi(buffer);

    ListView_GetItemText(g_state.results_view, selected, 1, buffer, 256);
    SetText(g_state.name_edit, buffer);
    ListView_GetItemText(g_state.results_view, selected, 2, buffer, 256);
    SetText(g_state.part_edit, buffer);
    ListView_GetItemText(g_state.results_view, selected, 3, buffer, 256);
    SetText(g_state.nsn_edit, buffer);
    ListView_GetItemText(g_state.results_view, selected, 4, buffer, 256);
    SetText(g_state.serial_edit, buffer);
    ListView_GetItemText(g_state.results_view, selected, 5, buffer, 256);
    SetText(g_state.quantity_edit, buffer);
}

void LayoutControls(HWND window, int width, int height) {
    const int margin = 16;
    const int label_width = 110;
    const int edit_height = 24;
    const int row_gap = 12;
    const int column_gap = 24;
    const int column_width = (width - margin * 2 - column_gap) / 2;
    const int edit_width = column_width - label_width - 10;

    int left_x = margin;
    int right_x = margin + column_width + column_gap;
    int y = margin;

    auto place_field = [&](HWND label, HWND edit, int x, int y_pos) {
        MoveWindow(label, x, y_pos, label_width, edit_height, TRUE);
        MoveWindow(edit, x + label_width + 8, y_pos, edit_width, edit_height, TRUE);
    };

    HWND name_label = GetDlgItem(window, kNameEdit - 100);
    HWND part_label = GetDlgItem(window, kPartEdit - 100);
    HWND nsn_label = GetDlgItem(window, kNsnEdit - 100);
    HWND serial_label = GetDlgItem(window, kSerialEdit - 100);
    HWND quantity_label = GetDlgItem(window, kQuantityEdit - 100);

    place_field(name_label, g_state.name_edit, left_x, y);
    place_field(serial_label, g_state.serial_edit, right_x, y);
    y += edit_height + row_gap;
    place_field(part_label, g_state.part_edit, left_x, y);
    place_field(quantity_label, g_state.quantity_edit, right_x, y);
    y += edit_height + row_gap;
    place_field(nsn_label, g_state.nsn_edit, left_x, y);

    int button_y = y + edit_height + row_gap;
    const int button_width = 110;
    const int button_height = 28;
    const int button_gap = 10;

    int button_x = margin;
    const int buttons[] = {kSaveButton, kUpdateButton, kDeleteButton, kSearchButton, kClearButton};
    for (int id : buttons) {
        HWND button = GetDlgItem(window, id);
        MoveWindow(button, button_x, button_y, button_width, button_height, TRUE);
        button_x += button_width + button_gap;
    }

    int list_y = button_y + button_height + row_gap;
    int status_height = 22;
    int list_height = height - list_y - status_height - margin;
    MoveWindow(g_state.results_view, margin, list_y, width - margin * 2, list_height, TRUE);
    MoveWindow(g_state.status_label, margin, height - status_height - margin,
               width - margin * 2, status_height, TRUE);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            CreateWindowW(L"STATIC", L"Name:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                          reinterpret_cast<HMENU>(kNameEdit - 100), nullptr, nullptr);
            CreateWindowW(L"STATIC", L"Part Number:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                          reinterpret_cast<HMENU>(kPartEdit - 100), nullptr, nullptr);
            CreateWindowW(L"STATIC", L"NSN:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                          reinterpret_cast<HMENU>(kNsnEdit - 100), nullptr, nullptr);
            CreateWindowW(L"STATIC", L"Serial Number:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                          reinterpret_cast<HMENU>(kSerialEdit - 100), nullptr, nullptr);
            CreateWindowW(L"STATIC", L"Quantity:", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window,
                          reinterpret_cast<HMENU>(kQuantityEdit - 100), nullptr, nullptr);

            g_state.name_edit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                              0, 0, 0, 0, window,
                                              reinterpret_cast<HMENU>(kNameEdit), nullptr,
                                              nullptr);
            g_state.part_edit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                              0, 0, 0, 0, window,
                                              reinterpret_cast<HMENU>(kPartEdit), nullptr,
                                              nullptr);
            g_state.nsn_edit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                             0, 0, 0, 0, window,
                                             reinterpret_cast<HMENU>(kNsnEdit), nullptr, nullptr);
            g_state.serial_edit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                                0, 0, 0, 0, window,
                                                reinterpret_cast<HMENU>(kSerialEdit), nullptr,
                                                nullptr);
            g_state.quantity_edit = CreateWindowW(
                L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(kQuantityEdit), nullptr, nullptr);

            CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                          window, reinterpret_cast<HMENU>(kSaveButton), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"Update", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0,
                          0, window, reinterpret_cast<HMENU>(kUpdateButton), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0,
                          0, window, reinterpret_cast<HMENU>(kDeleteButton), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"Search", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0,
                          0, window, reinterpret_cast<HMENU>(kSearchButton), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                          window, reinterpret_cast<HMENU>(kClearButton), nullptr, nullptr);

            g_state.results_view = CreateWindowW(
                WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 0, 0, 0,
                0, window, reinterpret_cast<HMENU>(kResultsView), nullptr, nullptr);
            ListView_SetExtendedListViewStyle(
                g_state.results_view, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            ConfigureListViewColumns(g_state.results_view);

            g_state.status_label = CreateWindowW(L"STATIC", L"Ready",
                                                 WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0,
                                                 window, reinterpret_cast<HMENU>(kStatusLabel),
                                                 nullptr, nullptr);

            RefreshResults();
            return 0;
        }
        case WM_SIZE: {
            int width = LOWORD(lparam);
            int height = HIWORD(lparam);
            LayoutControls(window, width, height);
            return 0;
        }
        case WM_COMMAND: {
            switch (LOWORD(wparam)) {
                case kSaveButton:
                    SaveRecord();
                    return 0;
                case kUpdateButton:
                    UpdateRecord();
                    return 0;
                case kDeleteButton:
                    DeleteRecord(window);
                    return 0;
                case kSearchButton:
                    RefreshResults();
                    return 0;
                case kClearButton:
                    ClearInputs();
                    RefreshResults();
                    return 0;
                default:
                    return 0;
            }
        }
        case WM_NOTIFY: {
            auto* header = reinterpret_cast<NMHDR*>(lparam);
            if (header->idFrom == kResultsView && header->code == LVN_ITEMCHANGED) {
                OnListViewSelect();
            }
            return 0;
        }
        case WM_DESTROY: {
            if (g_state.db) {
                sqlite3_close(g_state.db);
                g_state.db = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        }
        default:
            return DefWindowProcW(window, message, wparam, lparam);
    }
}
}  // namespace

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&controls);

    if (!InitDatabase()) {
        MessageBoxW(nullptr, L"Failed to initialize the database.", L"Error", MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClassName;
    RegisterClassExW(&wc);

    HWND window = CreateWindowExW(
        0, kWindowClassName, kWindowTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        1200, 720, nullptr, nullptr, instance, nullptr);
    if (!window) {
        MessageBoxW(nullptr, L"Failed to create the main window.", L"Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(window, SW_MAXIMIZE);
    UpdateWindow(window);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}