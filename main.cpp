#include <windows.h>
bool L, R, T;
HHOOK H;
bool S(DWORD k, bool u) {
 INPUT i{};
 i.type= INPUT_KEYBOARD;
 i.ki.wVk= (WORD)k;
 i.ki.dwFlags=
  (u ? KEYEVENTF_KEYUP : 0) | ((k == VK_RCONTROL || k == VK_RMENU) ? KEYEVENTF_EXTENDEDKEY : 0);
 return SendInput(1, &i, sizeof(i)) == 1;
}
LRESULT CALLBACK K(int c, WPARAM w, LPARAM l) {
 if(c < 0 || !l ||
    !(w == WM_KEYDOWN || w == WM_SYSKEYDOWN || w == WM_KEYUP || w == WM_SYSKEYUP))
  return CallNextHookEx(0, c, w, l);
 auto* d= (KBDLLHOOKSTRUCT*)l;
 if(d->flags & LLKHF_INJECTED) return CallNextHookEx(0, c, w, l);
 bool n= w == WM_KEYDOWN || w == WM_SYSKEYDOWN, u= w == WM_KEYUP || w == WM_SYSKEYUP;
 DWORD v= d->vkCode, e= v;
 bool cap= v == VK_CAPITAL;
 if(cap) {
  e= VK_RCONTROL;
  if(!S(e, u)) return CallNextHookEx(0, c, w, l);
 }
 if(n) {
  if(e == VK_LCONTROL) L= 1;
  if(e == VK_RCONTROL) R= 1;
 } else if(u) {
  if(e == VK_LCONTROL) L= 0;
  if(e == VK_RCONTROL) R= 0;
 }
 if(!(L && R))
  T= 0;
 else if(n && !T && S(VK_CAPITAL, 0) && S(VK_CAPITAL, 1))
  T= 1;
 return cap ? 1 : CallNextHookEx(0, c, w, l);
}
int main(int a, char** v) {
 for(int i= 1; i < a; i++)
  if(v[i] && !lstrcmpA(v[i], "--background")) {
   FreeConsole();
   H= SetWindowsHookExA(WH_KEYBOARD_LL, K, GetModuleHandleA(0), 0);
   if(!H) return 1;
   MSG m{};
   while(GetMessageA(&m, 0, 0, 0) > 0) {}
   UnhookWindowsHookEx(H);
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
