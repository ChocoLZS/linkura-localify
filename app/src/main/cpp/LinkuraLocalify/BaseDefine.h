#include "../platformDefine.hpp"

#ifndef GKMS_WINDOWS
	#define KEY_W  51
	#define KEY_S  47
	#define KEY_A  29
	#define KEY_D  32
	#define KEY_R  46
	#define KEY_Q  45
	#define KEY_E  33
	#define KEY_F  34
	#define KEY_I  37
	#define KEY_K  39
	#define KEY_J  38
	#define KEY_L  40
	#define KEY_V  50
	#define KEY_UP  19
	#define KEY_DOWN  20
	#define KEY_LEFT  21
	#define KEY_RIGHT  22
	#define KEY_CTRL  113
	#define KEY_SHIFT  59
	#define KEY_ALT  57
	#define KEY_SPACE  62
	#define KEY_ADD  70
	#define KEY_SUB  69

	#define WM_KEYDOWN 0
	#define WM_KEYUP 1
#else
	#define KEY_W  'W'
	#define KEY_S  'S'
	#define KEY_A  'A'
	#define KEY_D  'D'
	#define KEY_R  'R'
	#define KEY_Q  'Q'
	#define KEY_E  'E'
	#define KEY_F  'F'
	#define KEY_I  'I'
	#define KEY_K  'K'
	#define KEY_J  'J'
	#define KEY_L  'L'
	#define KEY_V  'V'
	#define KEY_UP  38
	#define KEY_DOWN  40
	#define KEY_LEFT  37
	#define KEY_RIGHT  39
	#define KEY_CTRL  17
	#define KEY_SHIFT  16
	#define KEY_ALT  18
	#define KEY_SPACE  32

	#define KEY_ADD  187
	#define KEY_SUB  189
#endif

#define BTN_A 96
#define BTN_B 97
#define BTN_X 99
#define BTN_Y 100
#define BTN_LB 102
#define BTN_RB 103
#define BTN_THUMBL 106
#define BTN_THUMBR 107
#define BTN_SELECT 109
#define BTN_START 108
#define BTN_SHARE 130
#define BTN_XBOX 110