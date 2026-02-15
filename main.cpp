#include <windows.h>
bool T;
bool S(DWORD k, bool u) {
 INPUT i{};
 i.type= 1;
 i.ki.wVk= (WORD)k;
 i.ki.dwFlags= (u ? 2 : 0) | (k == VK_RCONTROL ? 1 : 0);
 return SendInput(1, &i, sizeof(i));
}
LRESULT CALLBACK K(int c, WPARAM w, LPARAM l) {
 if(c < 0 || !l ||
    !(w == WM_KEYDOWN || w == WM_SYSKEYDOWN || w == WM_KEYUP || w == WM_SYSKEYUP))
  return CallNextHookEx(0, c, w, l);
 auto* d= (KBDLLHOOKSTRUCT*)l;
 if(d->flags & LLKHF_INJECTED) return CallNextHookEx(0, c, w, l);
 bool n= w == WM_KEYDOWN || w == WM_SYSKEYDOWN, u= w == WM_KEYUP || w == WM_SYSKEYUP;
 DWORD v= d->vkCode;
 if(v == VK_CAPITAL && !S(VK_RCONTROL, u)) return CallNextHookEx(0, c, w, l);
 bool a= (GetAsyncKeyState(VK_LCONTROL) & 0x8000) && (GetAsyncKeyState(VK_RCONTROL) & 0x8000);
 if(!a)
  T= 0;
 else if(n && !T && S(VK_CAPITAL, 0) && S(VK_CAPITAL, 1))
  T= 1;
 return v == VK_CAPITAL ? 1 : CallNextHookEx(0, c, w, l);
}
int main(int a, char** v) {
 for(int i= 1; i < a; i++)
  if(v[i] && !lstrcmpA(v[i], "--background")) {
   FreeConsole();
   HHOOK h= SetWindowsHookExA(13, K, GetModuleHandleA(0), 0);
   if(!h) return 1;
   MSG m{};
   while(GetMessageA(&m, 0, 0, 0) > 0) {}
   UnhookWindowsHookEx(h);
   return 0;
  }
 char p[MAX_PATH], c[MAX_PATH + 20];
 DWORD n= GetModuleFileNameA(0, p, MAX_PATH);
 if(!n || n >= MAX_PATH) return 1;
 wsprintfA(c, "\"%s\" --background", p);
 STARTUPINFOA s{};
 PROCESS_INFORMATION pi{};
 s.cb= sizeof(s);
 if(!CreateProcessA(0, c, 0, 0, 0, CREATE_NO_WINDOW | DETACHED_PROCESS, 0, 0, &s, &pi)) return 1;
 CloseHandle(pi.hThread);
 CloseHandle(pi.hProcess);
 return 0;
}
