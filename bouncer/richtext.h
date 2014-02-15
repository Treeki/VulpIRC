#include "buffer.h"

enum ColourPresets {
	COL_IRC_WHITE = 0,
	COL_IRC_BLACK = 1,
	COL_IRC_BLUE = 2,
	COL_IRC_GREEN = 3,
	COL_IRC_RED = 4,
	COL_IRC_BROWN = 5,
	COL_IRC_PURPLE = 6,
	COL_IRC_ORANGE = 7,
	COL_IRC_YELLOW = 8,
	COL_IRC_LIME = 9,
	COL_IRC_TEAL = 10,
	COL_IRC_CYAN = 11,
	COL_IRC_LIGHT_BLUE = 12,
	COL_IRC_PINK = 13,
	COL_IRC_GREY = 14,
	COL_IRC_LIGHT_GREY = 15,

	COL_DEFAULT_FG = 16,
	COL_DEFAULT_BG = 17,

	COL_ACTION = 18,
	COL_JOIN = 19,
	COL_PART = 20,
	COL_QUIT = 21,
	COL_KICK = 22,
	COL_CHANNEL_NOTICE = 23,
};

enum ColourLevels {
	COL_LEVEL_BASE = 0,
	COL_LEVEL_IRC = 1,
	COL_LEVEL_NICK = 2,
};

class RichTextBuilder : public Buffer {
public:
	void bold() { writeU8(1); }
	void endBold() { writeU8(2); }

	void italic() { writeU8(3); }
	void endItalic() { writeU8(4); }

	void underline() { writeU8(5); }
	void endUnderline() { writeU8(6); }

	void colour(bool background, int layer, int r, int g, int b) {
		writeU8(0x10 + (background ? 4 : 0) + layer);

		r >>= 1;
		g >>= 1;
		b >>= 1;

		writeU8((r==0)?2:(r&254));
		writeU8((g==0)?1:g);
		writeU8((b==0)?1:b);
	}
	void colour(bool background, int layer, int col) {
		writeU8(0x10 + (background ? 4 : 0) + layer);
		writeU8((col << 1) | 1);
	}

	void endColour(bool background, int layer) {
		writeU8(0x18 + (background ? 4 : 0) + layer);
	}

	void foreground(int layer, int r, int g, int b) {
		colour(false, layer, r, g, b);
	}
	void foreground(int layer, int col) {
		colour(false, layer, col);
	}
	void endForeground(int layer) {
		endColour(false, layer);
	}

	void background(int layer, int r, int g, int b) {
		colour(true, layer, r, g, b);
	}
	void background(int layer, int col) {
		colour(true, layer, col);
	}
	void endBackground(int layer) {
		endColour(true, layer);
	}

	void append(const char *str) {
		Buffer::append(str, strlen(str));
	}

	void appendIRC(const char *str);
	void appendNick(const char *nick);

	const char *c_str();
};

