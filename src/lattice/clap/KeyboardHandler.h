#pragma once

#include <atomic>

#if LATTICE_WINDOWS
#include <windows.h>
#include <commctrl.h>
#endif

namespace lattice {

#if LATTICE_WINDOWS

/// Windows-specific keyboard handler using window subclassing
/// By default, ALL keys pass through to the parent window/DAW
/// When consumeKeypresses is enabled, keys are captured by the webview
class WindowsKeyboardHandler {
public:
  /// Data associated with subclassed window
  struct SubclassData {
    std::atomic<bool>* consumeKeypresses;
    HWND parentWindow;
  };

  /// Window procedure for subclassed webview window
  static LRESULT CALLBACK SubclassProc(
      HWND hwnd,
      UINT msg,
      WPARAM wParam,
      LPARAM lParam,
      UINT_PTR /*uIdSubclass*/,
      DWORD_PTR dwRefData) {

    SubclassData* data = reinterpret_cast<SubclassData*>(dwRefData);

    // Intercept keyboard messages
    if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_CHAR) {
      // Check if webview should consume keypresses
      bool consumeKeys = data->consumeKeypresses->load(std::memory_order_relaxed);

      if (!consumeKeys && data->parentWindow) {
        // Pass through to parent window/DAW
        PostMessage(data->parentWindow, msg, wParam, lParam);
        // Return 0 to indicate we handled the message (by passing it through)
        return 0;
      }
      // If consumeKeys is true, fall through to default processing (webview handles it)
    }

    // Default processing for all other messages
    return DefSubclassProc(hwnd, msg, wParam, lParam);
  }

  /// Install keyboard passthrough on a webview window
  /// By default, keys pass through to parent. Set consumeKeypresses to true to capture them.
  static bool installKeyboardPassthrough(
      HWND webviewWindow,
      HWND parentWindow,
      std::atomic<bool>* consumeKeypresses) {

    if (!webviewWindow || !parentWindow || !consumeKeypresses)
      return false;

    // Allocate subclass data
    SubclassData* data = new SubclassData{ consumeKeypresses, parentWindow };

    // Install window subclass
    BOOL result = SetWindowSubclass(
        webviewWindow,
        SubclassProc,
        0, // subclass ID
        reinterpret_cast<DWORD_PTR>(data));

    return result == TRUE;
  }

  /// Remove keyboard passthrough from a webview window
  static bool removeKeyboardPassthrough(HWND webviewWindow) {
    if (!webviewWindow)
      return false;

    // Get subclass data to free it
    DWORD_PTR refData = 0;
    if (GetWindowSubclass(webviewWindow, SubclassProc, 0, &refData)) {
      SubclassData* data = reinterpret_cast<SubclassData*>(refData);
      delete data;
    }

    // Remove window subclass
    BOOL result = RemoveWindowSubclass(webviewWindow, SubclassProc, 0);
    return result == TRUE;
  }
};

#endif // LATTICE_WINDOWS

} // namespace lattice
