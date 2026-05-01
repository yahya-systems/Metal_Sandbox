#pragma once

/**
 * ENGINE VIRTUAL KEYCODES
 * ---------------------------------------------------------
 * Range 000-127: Standard ASCII (Direct Map)
 * Range 130-190: Island A (Numpad & F-Keys)
 * Range 200-213: Island B (Modifiers)
 * ---------------------------------------------------------
 */

// --- Letters (Direct ASCII Lowercase) ---
#define KEY_A 97
#define KEY_B 98
#define KEY_C 99
#define KEY_D 100
#define KEY_E 101
#define KEY_F 102
#define KEY_G 103
#define KEY_H 104
#define KEY_I 105
#define KEY_J 106
#define KEY_K 107
#define KEY_L 108
#define KEY_M 109
#define KEY_N 110
#define KEY_O 111
#define KEY_P 112
#define KEY_Q 113
#define KEY_R 114
#define KEY_S 115
#define KEY_T 116
#define KEY_U 117
#define KEY_V 118
#define KEY_W 119
#define KEY_X 120
#define KEY_Y 121
#define KEY_Z 122

// --- Numbers ---
#define KEY_0 48
#define KEY_1 49
#define KEY_2 50
#define KEY_3 51
#define KEY_4 52
#define KEY_5 53
#define KEY_6 54
#define KEY_7 55
#define KEY_8 56
#define KEY_9 57

// --- Island A: Numpad & Special (130 + sym - 65421) ---
#define KEY_KP_ENTER 130
#define KEY_KP_MULTIPLY 159 // 130 + (65450 - 65421)
#define KEY_KP_ADD 160
#define KEY_KP_SUBTRACT 162
#define KEY_KP_DECIMAL 163
#define KEY_KP_DIVIDE 164
#define KEY_KP_0 165 // 130 + (65456 - 65421)
#define KEY_KP_1 166
#define KEY_KP_2 167
#define KEY_KP_3 168
#define KEY_KP_4 169
#define KEY_KP_5 170
#define KEY_KP_6 171
#define KEY_KP_7 172
#define KEY_KP_8 173
#define KEY_KP_9 174

// --- Island A: Function Keys ---
#define KEY_F1 179 // 130 + (65470 - 65421)
#define KEY_F2 180
#define KEY_F3 181
#define KEY_F4 182
#define KEY_F5 183
#define KEY_F6 184
#define KEY_F7 185
#define KEY_F8 186
#define KEY_F9 187
#define KEY_F10 188
#define KEY_F11 189
#define KEY_F12 190

// --- Island B: Modifiers (200 + sym - 65505) ---
#define KEY_SHIFT_L 200
#define KEY_SHIFT_R 201
#define KEY_CONTROL_L 202
#define KEY_CONTROL_R 203
#define KEY_CAPS_LOCK 204
#define KEY_META_L 206
#define KEY_ALT_L 208 // 200 + (65513 - 65505)
#define KEY_ALT_R 209
#define KEY_SUPER_L 210 // Windows Key

// --- Common UI Keys (Low ASCII) ---
#define KEY_SPACE 32
#define KEY_RETURN 13
#define KEY_ESCAPE 27
#define KEY_BACKSPACE 8
#define KEY_TAB 9
