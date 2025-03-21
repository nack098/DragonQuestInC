#pragma once

#include <stdint.h>

typedef struct MENUITEM {

	char* Name;
	int16_t X;
	int16_t Y;
	void (*Action)(void);

} MENUITEM;

typedef struct MENU {

	char* Name;
	uint8_t SelectedItem;
	uint8_t ItemCount;
	MENUITEM** Items;

} MENU;

void MenuItem_TitleScreen_StartNew(void);
void MenuItem_TitleScreen_Options(void);
void MenuItem_TitleScreen_Exit(void);

void MenuItem_ExitYesNoScreen_Yes(void);
void MenuItem_ExitYesNoScreen_No(void);

void MenuItem_CharacterNaming_Add(void);
void MenuItem_CharacterNaming_Back(void);
void MenuItem_CharacterNaming_OK(void);

/// Title Screen

MENUITEM gMI_StartNewGame = {
	"Start New Game",
	(GAME_RES_WIDTH / 2) - ((14 * 6) / 2),
	100,
	MenuItem_TitleScreen_StartNew
};

MENUITEM gMI_Option = {
	"Options",
	(GAME_RES_WIDTH / 2) - ((7 * 6) / 2),
	120,
	MenuItem_TitleScreen_Options
};

MENUITEM gMI_Exit = {
	"Exit",
	(GAME_RES_WIDTH / 2) - ((4 * 6) / 2),
	140,
	MenuItem_TitleScreen_Exit
};

MENUITEM* gMI_TitleScreenItems[] = {
	&gMI_StartNewGame,
	&gMI_Option,
	&gMI_Exit
};

MENU gMenu_TitleScreen = {
	"Title Screen Menu",
	0,
	_countof(gMI_TitleScreenItems),
	gMI_TitleScreenItems
};

///

/// Exit Yes No Screen

MENUITEM gMI_Yes = {
	"Yes",
	(GAME_RES_WIDTH / 2) - ((3 * 6) / 2),
	100,
	MenuItem_ExitYesNoScreen_Yes
};

MENUITEM gMI_No = {
	"No",
	(GAME_RES_WIDTH / 2) - ((2 * 6) / 2),
	120,
	MenuItem_ExitYesNoScreen_No
};

MENUITEM* gMI_ExitYesNoScreenItems[] = {
	&gMI_Yes,
	&gMI_No
};

MENU gMenu_ExitYesNoScreen = {
	"Exit Yes No Screen",
	0,
	_countof(gMI_ExitYesNoScreenItems),
	gMI_ExitYesNoScreenItems
};

///

/// Character Naming Menu

MENUITEM gMI_CharacterNaming_A = { "A", 118, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_B = { "B", 130, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_C = { "C", 142, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_D = { "D", 154, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_E = { "E", 166, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_F = { "F", 178, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_G = { "G", 190, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_H = { "H", 202, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_I = { "I", 214, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_J = { "J", 226, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_K = { "K", 238, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_L = { "L", 250, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_M = { "M", 262, 134, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_N = { "N", 118, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_O = { "O", 130, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_P = { "P", 142, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_Q = { "Q", 154, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_R = { "R", 166, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_S = { "S", 178, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_T = { "T", 190, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_U = { "U", 202, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_V = { "V", 214, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_W = { "W", 226, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_X = { "X", 238, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_Y = { "Y", 250, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_Z = { "Z", 262, 144, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_a = { "a", 118, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_b = { "b", 130, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_c = { "c", 142, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_d = { "d", 154, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_e = { "e", 166, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_f = { "f", 178, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_g = { "g", 190, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_h = { "h", 202, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_i = { "i", 214, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_j = { "j", 226, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_k = { "k", 238, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_l = { "l", 250, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_m = { "m", 262, 154, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_n = { "n", 118, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_o = { "o", 130, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_p = { "p", 142, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_q = { "q", 154, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_r = { "r", 166, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_s = { "s", 178, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_t = { "t", 190, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_u = { "u", 202, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_v = { "v", 214, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_w = { "w", 226, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_x = { "x", 238, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_y = { "y", 250, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_z = { "z", 262, 164, MenuItem_CharacterNaming_Add };
MENUITEM gMI_CharacterNaming_Back = { "Back", 118, 174, MenuItem_CharacterNaming_Back };
MENUITEM gMI_CharacterNaming_OK = { "OK", 256, 174,  MenuItem_CharacterNaming_OK };
MENUITEM* gMI_CharacterNamingItems[] = {
	&gMI_CharacterNaming_A, &gMI_CharacterNaming_B, &gMI_CharacterNaming_C, &gMI_CharacterNaming_D, &gMI_CharacterNaming_E, &gMI_CharacterNaming_F,
	&gMI_CharacterNaming_G, &gMI_CharacterNaming_H, &gMI_CharacterNaming_I, &gMI_CharacterNaming_J, &gMI_CharacterNaming_K, &gMI_CharacterNaming_L,
	&gMI_CharacterNaming_M, &gMI_CharacterNaming_N, &gMI_CharacterNaming_O, &gMI_CharacterNaming_P, &gMI_CharacterNaming_Q, &gMI_CharacterNaming_R,
	&gMI_CharacterNaming_S, &gMI_CharacterNaming_T, &gMI_CharacterNaming_U, &gMI_CharacterNaming_V, &gMI_CharacterNaming_W, &gMI_CharacterNaming_X,
	&gMI_CharacterNaming_Y, &gMI_CharacterNaming_Z,
	&gMI_CharacterNaming_a, &gMI_CharacterNaming_b, &gMI_CharacterNaming_c, &gMI_CharacterNaming_d, &gMI_CharacterNaming_e, &gMI_CharacterNaming_f,
	&gMI_CharacterNaming_g, &gMI_CharacterNaming_h, &gMI_CharacterNaming_i, &gMI_CharacterNaming_j, &gMI_CharacterNaming_k, &gMI_CharacterNaming_l,
	&gMI_CharacterNaming_m, &gMI_CharacterNaming_n, &gMI_CharacterNaming_o, &gMI_CharacterNaming_p, &gMI_CharacterNaming_q, &gMI_CharacterNaming_r,
	&gMI_CharacterNaming_s, &gMI_CharacterNaming_t, &gMI_CharacterNaming_u, &gMI_CharacterNaming_v, &gMI_CharacterNaming_w, &gMI_CharacterNaming_x,
	&gMI_CharacterNaming_y, &gMI_CharacterNaming_z, &gMI_CharacterNaming_Back, &gMI_CharacterNaming_OK };

MENU gMenu_CharacterNaming = { "What's your name, hero?", 0, _countof(gMI_CharacterNamingItems), gMI_CharacterNamingItems };

///
