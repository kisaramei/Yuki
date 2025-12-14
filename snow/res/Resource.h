// resource.h
// Microsoft Visual C++ 生成的包含文件。
// 使用者 snow.rc

#pragma once

#define IDS_APP_TITLE    103
#define IDR_MAINFRAME    128
#define IDI_SNOW         107
#define IDI_SMALL        108
#define IDC_SNOW         109
#define IDC_MYICON       2

// --- 常用对话框 ---
#define IDD_ABOUTBOX     103

// --- 托盘菜单命令 ---
#define IDM_EXIT         105
#define IDM_ABOUT        104
// 注意：我把原来的 IDM_SETTINGS 改成了 IDM_TRAY_SETTING 以匹配代码
#define IDM_TRAY_SETTING 1003

// --- 露露叶新增：设置对话框 ID ---
#define IDD_SETTINGS     2000  // 设置对话框本身的 ID
#define IDC_SLIDER_COUNT 2001  // 雪量滑块
#define IDC_SLIDER_SPEED 2002  // 速度滑块
#define IDC_SLIDER_WIND  2003  // 风力滑块
#define IDC_LABEL_COUNT  2004  // 显示数值文本
#define IDC_LABEL_SPEED  2005
#define IDC_LABEL_WIND   2006
#define IDC_BTN_RESET    2007

#ifndef IDC_STATIC
#define IDC_STATIC -1
#endif

// 新对象的下一组默认值
// (保留这些是为了让 Visual Studio
// 的资源视图编辑器不报错，虽然手写代码其实不需要它们)
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NO_MFC              130
#define _APS_NEXT_RESOURCE_VALUE 129
#define _APS_NEXT_COMMAND_VALUE  32771
#define _APS_NEXT_CONTROL_VALUE  1000
#define _APS_NEXT_SYMED_VALUE    110
#endif
#endif
