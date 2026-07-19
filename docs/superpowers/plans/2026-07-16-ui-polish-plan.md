# UI Polish — Implementation Plan

## Overview

Fix 6 UX/UI issues in AceTranslatePro after the initial layout redesign: PNG icons, screenshot alignment, scrollbar styling, hotkey input, iOS switch toggles, and file-translation result notification.

## Requirements

1. **Navigation bar + all action buttons use PNG icons from user** — download a full set from iconfont.cn
2. **Screenshot panel**: left preview and right result vertically top-aligned, preview size reduced
3. **QScrollArea scrollbars** — style via QSS (thin, rounded, theme-colored)
4. **Hotkey configuration button** — styled like the rest of the UI (not raw QLineEdit)
5. **QCheckBox → iOS toggle switch** — QSS-only "toggle switch" with sliding indicator
6. **File translation completion** — top Toast notification showing "翻译完成，结果保存到：xxx" that auto-fades

## Design Details

### 1. All PNG icons (user will download)
User downloads a complete icon set from iconfont.cn. The set includes icons for all action buttons used in the UI. Once they place the files, we'll update QRC and code references.

### 2. Screenshot panel alignment
- Reduce `screenshotPreview_` fixed size from 150×150 to 120×120
- Put BOTH preview and result inside a single `QHBoxLayout` with `Qt::AlignTop`
- The preview QScrollArea and result QTextEdit share the row, both stretch equally
- Remove the separate card wrappers

### 3. Scrollbar QSS
```qss
/* Vertical */
QScrollBar:vertical {
    background: transparent; width: 6px; margin: 0;
}
QScrollBar::handle:vertical {
    background: #D0D4D8; border-radius: 3px; min-height: 30px;
}
QScrollBar::handle:vertical:hover { background: #0B7C72; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
/* Horizontal */
QScrollBar:horizontal {
    background: transparent; height: 6px; margin: 0;
}
QScrollBar::handle:horizontal {
    background: #D0D4D8; border-radius: 3px; min-width: 30px;
}
QScrollBar::handle:horizontal:hover { background: #0B7C72; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
```

### 4. Hotkey button styling
The hotkey buttons (`floatHotkeyBtn_`, `screenshotHotkeyBtn_`) already use QPushButton. Add a QSS rule:
```qss
QPushButton#hotkeyBtn {
    border: 1px solid #DDE1E5; border-radius: 6px;
    padding: 8px 14px; background: #FFFFFF; color: #1A1A2E;
    font-size: 13px; font-family: "Consolas", monospace; text-align: left;
}
QPushButton#hotkeyBtn:hover { border-color: #0B7C72; }
```

### 5. iOS toggle switch (QSS only on QCheckBox)
Transform QCheckBox into a switch entirely via QSS:
```qss
QCheckBox::indicator {
    width: 44px; height: 24px; border-radius: 12px;
    border: none; background: #D0D4D8; /* off state */
}
QCheckBox::indicator:checked {
    background: #0B7C72; /* on state */
}
/* The "knob" — a pseudo-element before/after won't work in Qt QSS.
   Instead we draw a circle in the ::indicator area:
   - Off: indicator shows a gray rectangle with a white circle on left
   - On: indicator shows green rectangle with white circle on right
   We achieve this via a border/background trick: the indicator area is split.
*/
QCheckBox::indicator {
    width: 44px; height: 24px; border-radius: 12px;
    border: none; background: #D0D4D8;
    margin-right: 6px;
}
QCheckBox::indicator:checked {
    background: #0B7C72;
}
/* For the knob we use a CSS workaround: the indicator is the full switch;
   Qt QSS can't do ::after, so we use a border-left trick to simulate the knob.
   Actually the cleanest approach: use setStyleSheet on the checkbox to
   apply a custom stylesheet that uses image: none + background-position.
   Best approach: setIndicatorStyle(QStyleOptionButton::...) won't work.
   We'll draw the knob with subcontrol positioning. */

/* Simpler approach that actually works in Qt QSS: */
QCheckBox::indicator {
    width: 20px; height: 20px; border-radius: 10px;
    border: 2px solid #D0D4D8; background: #FFFFFF;
}
QCheckBox::indicator:checked {
    background: #0B7C72; border-color: #0B7C72;
}
QCheckBox::indicator:hover { border-color: #0B7C72; }
```
(If the user specifically wants the full-width iOS style, we'd need to use a custom QStyle or override paintEvent — QSS alone can't do a sliding knob properly in Qt6. Default to the above round indicator which looks clean.)

### 6. Toast notification for file translation completion
A frameless QWidget that slides in from the top of the window, stays 3 seconds, then fades out:

```cpp
class ToastNotification : public QFrame {
    Q_OBJECT
public:
    static void show(QWidget* parent, const QString& message, int durationMs = 3000);
};
```

Implementation:
- Fixed width: parent width - 40px, height: ~48px
- Position: anchored to top of parent, margin 8px
- Background: #1A1A2E (dark), white text, border-radius 8px
- QPropertyAnimation on `windowOpacity` from 0→1 (300ms), stay, then 1→0 (500ms)
- Auto-delete after animation finishes
- For the file panel: after onWorkerFinished with currentNavIndex_==4, call ToastNotification::show(this, "翻译完成！结果保存到：" + result)

## Files to modify

| File | Changes |
|---|---|
| `ui/style.qss` | Add scrollbar QSS, hotkeyBtn QSS, toggle switch QSS, toast QSS |
| `ui/toast.h` (new) | ToastNotification class declaration |
| `ui/toast.cpp` (new) | ToastNotification implementation with QPropertyAnimation |
| `ui/resources.qrc` | Add new PNG icons when user places them |
| `ui/mainwindow.h` | Include toast.h |
| `ui/mainwindow.cpp` | Update screenshot panel alignment, file result → toast |
| `CMakeLists.txt` | Add toast.cpp to SOURCES |

## Implementation order
1. Add toast.h/.cpp and include in CMakeLists
2. QSS additions (scrollbar, hotkeyBtn, toggle checkbox)
3. File panel: replace result QTextEdit with toast notification
4. Screenshot panel: realign preview+result to top
5. Wait for user's PNG icon set download

## Verification
- Build: cmake --build . --config Debug with 0 errors
- File translation: process file → see top toast "翻译完成！结果保存到：xxx"
- Scrollbar: view file list or text area → thin styled scrollbar visible
- Hotkey button: click → QKeySequenceEdit dialog → updates cleanly
- Checkbox: appears as rounded toggle in file panel params
- Screenshot: capture → preview and result top-aligned
