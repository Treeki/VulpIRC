#include "richtext.h"

static int matchIRCColourString(const char *str, int pos, int *result) {
	// First, try to match a single digit.
	if (str[pos] >= '0' && str[pos] <= '9') {
		int first = str[pos] - '0';
		if (str[pos + 1] >= '0' && str[pos + 1] <= '9') {
			int second = str[pos + 1] - '0';

			int attempt = (first * 10) + second;
			if (attempt < 0 || attempt > 15) {
				// Invalid number, give up here
				return pos;
			} else {
				// Valid number!
				*result = attempt;
				return pos + 2;
			}
		} else {
			// Second character was not a digit, so
			// let's take the colour we *did* parse as is
			*result = first;
			return pos + 1;
		}
	} else {
		// First character was not a digit, so pass entirely.
		return pos;
	}
}

static int matchIRCColourPair(const char *str, int pos, int *whichFG, int *whichBG) {
	// str[pos] is the first character following the
	// \x03 colour code.

	*whichFG = -1;
	*whichBG = -1;

	pos = matchIRCColourString(str, pos, whichFG);
	if (*whichFG != -1) {
		if (str[pos] == ',') {
			pos = matchIRCColourString(str, pos + 1, whichBG);
		}
	}

	return pos;
}


void RichTextBuilder::appendIRC(const char *str) {
	// We're up for fun here!
	const int colLayer = COL_LEVEL_IRC;

	bool b = false, i = false, u = false;
	int activeFG = COL_DEFAULT_FG, activeBG = COL_DEFAULT_BG;

	int pos = 0;
	int len = strlen(str);

	while (pos < len) {
		char ch = str[pos];

		if (ch < 0 || ch == 7 || ch == 10 || ch >= 0x20) {
			// Pass this character through unfiltered.
			writeU8(ch);
			pos++;
			continue;
		}

		// IRC control code, but what?
		if (ch == 2) {
			b = !b;
			if (b)
				bold();
			else
				endBold();
		} else if (ch == 0x1D) {
			i = !i;
			if (i)
				italic();
			else
				endItalic();
		} else if (ch == 0x1F) {
			u = !u;
			if (u)
				underline();
			else
				endUnderline();
		} else if (ch == 0xF) {
			// Reset all formatting
			if (b) {
				endBold();
				b = false;
			}
			if (i) {
				endItalic();
				i = false;
			}
			if (u) {
				endUnderline();
				u = false;
			}
			if (activeFG != COL_DEFAULT_FG) {
				endForeground(colLayer);
				activeFG = COL_DEFAULT_FG;
			}
			if (activeBG != COL_DEFAULT_BG) {
				endBackground(colLayer);
				activeBG = COL_DEFAULT_BG;
			}
		} else if (ch == 0x16) {
			// Reverse
			int swap = activeBG;
			activeBG = activeFG;
			activeFG = swap;

			if (activeFG == COL_DEFAULT_FG)
				endForeground(colLayer);
			else
				foreground(colLayer, activeFG);

			if (activeBG == COL_DEFAULT_BG)
				endBackground(colLayer);
			else
				background(colLayer, activeBG);

		} else if (ch == 3) {
			// Colours!

			int whichFG = -1, whichBG = -1;

			pos = matchIRCColourPair(str, pos + 1, &whichFG, &whichBG);

			if (whichFG == -1) {
				if (activeFG != COL_DEFAULT_FG) {
					activeFG = COL_DEFAULT_FG;
					endForeground(colLayer);
				}
			} else {
				activeFG = whichFG;
				foreground(colLayer, whichFG);
			}

			if (whichBG == -1) {
				if (whichFG == -1 && activeBG != COL_DEFAULT_BG) {
					activeBG = COL_DEFAULT_BG;
					endBackground(colLayer);
				}
			} else {
				activeBG = whichBG;
				background(colLayer, whichBG);
			}

			continue;
		}
		pos++;
	}

	// Clean up any leftover state.
	if (b)
		endBold();
	if (i)
		endItalic();
	if (u)
		endUnderline();
	if (activeFG != COL_DEFAULT_FG)
		endForeground(colLayer);
	if (activeBG != COL_DEFAULT_BG)
		endBackground(colLayer);
}

const char *RichTextBuilder::c_str() {
	if (size() == 0 || data()[size() - 1] != 0)
		writeU8(0);

	return data();
}



inline static float hslValue(float n1, float n2, float hue) {
	if (hue > 6.0f)
		hue -= 6.0f;
	else if (hue < 0.0f)
		hue += 6.0f;

	if (hue < 1.0f)
		return n1 + (n2 - n1) * hue;
	else if (hue < 3.0f)
		return n2;
	else if (hue < 4.0f)
		return n1 + (n2 - n1) * (4.0f - hue);
	else
		return n1;
}

void calculateHSL(float h, float s, float l, uint8_t *r, uint8_t *g, uint8_t *b) {
	// make it RGB

	if (s == 0) {
		*r = *g = *b = l*255.0f;
	} else {
		float m1, m2;
		if (l <= 0.5f)
			m2 = l * (1.0f + s);
		else
			m2 = l + s - l * s;

		m1 = 2.0f * s - m2;

		*r = hslValue(m1, m2, h * 6.0f + 2.0) * 255.0f;
		*g = hslValue(m1, m2, h * 6.0f) * 255.0f;
		*b = hslValue(m1, m2, h * 6.0f - 2.0) * 255.0f;
	}
}



void RichTextBuilder::appendNick(const char *nick) {
	uint32_t hash = 0;
	const char *c = nick;
	while (*c)
		hash = (hash * 17) + *(c++);

	int hueBase = ((hash & 0xFF0000) >> 16);
	int satBase = ((hash & 0xFF00) >> 8);
	int lumBase = (hash & 0xFF);
	uint8_t r, g, b;

	calculateHSL(
		hueBase / 255.0f,
		(0.50f + (satBase / 765.0f)),
		(0.40f + (lumBase / 1020.0f)),
		&r, &g, &b);

	foreground(COL_LEVEL_NICK, r, g, b);
	append(nick);
	endForeground(COL_LEVEL_NICK);
}
