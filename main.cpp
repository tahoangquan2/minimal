#include <windows.h>

#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* kBackgroundArg = "--background";

std::unordered_set<DWORD> pressed_keys;
bool ctrl_chord_triggered = false;
HHOOK keyboard_hook = nullptr;
std::string last_error_message;

void set_last_error_message(const std::string& prefix) {
    DWORD error_code = GetLastError();
    char* message_buffer = nullptr;
    DWORD copied_length = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);

    std::string error_text;
    if (copied_length == 0 || message_buffer == nullptr) {
        error_text = "Windows error code " + std::to_string(error_code);
    } else {
        error_text.assign(message_buffer, static_cast<std::size_t>(copied_length));
        LocalFree(message_buffer);
        while (!error_text.empty() &&
               (error_text.back() == '\r' || error_text.back() == '\n' || error_text.back() == ' ')) {
            error_text.pop_back();
        }
    }

    last_error_message = prefix + ": " + error_text;
}

bool has_background_flag(int argc, char** argv) {
    for (int index = 1; index < argc; index += 1) {
        if (argv[index] == nullptr) {
            continue;
        }
        if (std::string(argv[index]) == kBackgroundArg) {
            return true;
        }
    }
    return false;
}

std::string build_background_command_line() {
    char module_path[MAX_PATH];
    DWORD copied_length = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
    if (copied_length == 0 || copied_length >= MAX_PATH) {
        set_last_error_message("Failed to resolve executable path");
        return "";
    }

    std::string executable_path(module_path, static_cast<std::size_t>(copied_length));
    return "\"" + executable_path + "\" " + kBackgroundArg;
}

bool launch_detached_background() {
    std::string command_line = build_background_command_line();
    if (command_line.empty()) {
        if (last_error_message.empty()) {
            last_error_message = "Failed to build background command line.";
        }
        return false;
    }

    STARTUPINFOA startup_info{};
    startup_info.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION process_info{};

    std::vector<char> command_line_buffer;
    command_line_buffer.reserve(command_line.size() + 1);
    for (char character : command_line) {
        command_line_buffer.push_back(character);
    }
    command_line_buffer.push_back('\0');

    BOOL launch_result =
        CreateProcessA(nullptr, command_line_buffer.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &startup_info,
                       &process_info);
    if (launch_result == FALSE) {
        set_last_error_message("Failed to launch detached background process");
        return false;
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return true;
}

bool is_key_message(WPARAM msg) {
    return msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP;
}

bool is_key_down(WPARAM msg) { return msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN; }

bool is_key_up(WPARAM msg) { return msg == WM_KEYUP || msg == WM_SYSKEYUP; }

bool send_key_event(DWORD vk, bool key_up) {
    INPUT input_event{};
    input_event.type = INPUT_KEYBOARD;
    input_event.ki.wVk = static_cast<WORD>(vk);
    if (vk == VK_RCONTROL || vk == VK_RMENU) {
        input_event.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    if (key_up) {
        input_event.ki.dwFlags |= KEYEVENTF_KEYUP;
    }
    return SendInput(1, &input_event, sizeof(INPUT)) == 1;
}

bool send_key_tap(DWORD vk) { return send_key_event(vk, false) && send_key_event(vk, true); }

LRESULT CALLBACK low_level_keyboard_proc(int code, WPARAM wParam, LPARAM lParam) {
    if (code < 0) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    if (!is_key_message(wParam) || lParam == 0) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const KBDLLHOOKSTRUCT* keyboard_data = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
    if ((keyboard_data->flags & LLKHF_INJECTED) != 0) {
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    const bool key_down = is_key_down(wParam);
    const bool key_up = is_key_up(wParam);

    DWORD source_vk = keyboard_data->vkCode;
    DWORD effective_vk = source_vk;
    bool remapped_caps = false;

    if (source_vk == VK_CAPITAL) {
        effective_vk = VK_RCONTROL;
        if (!send_key_event(effective_vk, key_up)) {
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }
        remapped_caps = true;
    }

    if (key_down) {
        pressed_keys.insert(effective_vk);
    } else if (key_up) {
        pressed_keys.erase(effective_vk);
    }

    const bool ctrl_chord_active =
        pressed_keys.find(VK_LCONTROL) != pressed_keys.end() &&
        pressed_keys.find(VK_RCONTROL) != pressed_keys.end();
    if (!ctrl_chord_active) {
        ctrl_chord_triggered = false;
    } else if (key_down && !ctrl_chord_triggered) {
        if (send_key_tap(VK_CAPITAL)) {
            ctrl_chord_triggered = true;
        }
    }

    if (remapped_caps) {
        return 1;
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

int run_background_hook_loop() {
    HMODULE module_handle = GetModuleHandleA(nullptr);
    keyboard_hook = SetWindowsHookExA(WH_KEYBOARD_LL, low_level_keyboard_proc, module_handle, 0);
    if (keyboard_hook == nullptr) {
        set_last_error_message("Failed to install keyboard hook");
        return 1;
    }

    MSG message{};
    BOOL get_message_result = TRUE;
    while (get_message_result > 0) {
        get_message_result = GetMessageA(&message, nullptr, 0, 0);
    }

    UnhookWindowsHookEx(keyboard_hook);
    keyboard_hook = nullptr;
    return get_message_result == -1 ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (!has_background_flag(argc, argv)) {
        if (launch_detached_background()) {
            return 0;
        }
        if (last_error_message.empty()) {
            std::fprintf(stderr, "Failed to start background keyboard manager.\n");
        } else {
            std::fprintf(stderr, "%s\n", last_error_message.c_str());
        }
        return 1;
    }

    FreeConsole();
    return run_background_hook_loop();
}
