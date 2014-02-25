package net.brokenfox.vulpirc;

import android.graphics.Typeface;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.BackgroundColorSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.text.style.UnderlineSpan;
import android.text.util.Linkify;

import java.util.HashMap;

/**
 * Created by ninji on 2/14/14.
 */
public class RichText {
	private final static int[] presetColors = new int[] {
		// IRC colours
		0xFFFFFF, 0x000000, 0x000080, 0x008000,
		0xFF0000, 0x800000, 0x800080, 0x808000,
		0xFFFF00, 0x00FF00, 0x008080, 0x00FFFF,
		0x0000FF, 0xFF00FF, 0x808080, 0xC0C0C0,
				// Default FG, BG
		0xFFFFFF, 0x000000,
				// Preset message colours
		0x6600CC, 0x009900, 0x660000, 0x660000,
		0x660000, 0x336699,
	};

	private final static StyleSpan boldSpan = new StyleSpan(Typeface.BOLD);
	private final static StyleSpan italicSpan = new StyleSpan(Typeface.ITALIC);
	private final static UnderlineSpan underlineSpan = new UnderlineSpan();
	private static ForegroundColorSpan[] presetForegroundSpans = null;
	private static BackgroundColorSpan[] presetBackgroundSpans = null;
	private static HashMap<Integer, ForegroundColorSpan> cachedForegroundSpans = new HashMap<Integer, ForegroundColorSpan>();
	private static HashMap<Integer, BackgroundColorSpan> cachedBackgroundSpans = new HashMap<Integer, BackgroundColorSpan>();

	private static void setupPresets() {
		presetForegroundSpans = new ForegroundColorSpan[presetColors.length];
		presetBackgroundSpans = new BackgroundColorSpan[presetColors.length];

		for (int i = 0; i < presetColors.length; i++) {
			presetForegroundSpans[i] = new ForegroundColorSpan(0xFF000000 | presetColors[i]);
			presetBackgroundSpans[i] = new BackgroundColorSpan(0xFF000000 | presetColors[i]);
		}
	}

	// This is kinda kludgey but... I don't want to make two extra allocations
	// every time I process a string.
	// So here.
	private static ForegroundColorSpan[] currentFgSpans = new ForegroundColorSpan[4];
	private static BackgroundColorSpan[] currentBgSpans = new BackgroundColorSpan[4];

	public static Spanned process(String source) {
		if (presetForegroundSpans == null)
			setupPresets();

		SpannableStringBuilder s = new SpannableStringBuilder();

		int boldStart = -1;
		int italicStart = -1;
		int underlineStart = -1;
		int fgStart = -1, bgStart = -1;
		ForegroundColorSpan fgSpan = null;
		BackgroundColorSpan bgSpan = null;
		int rawTextStart = -1;

		for (int i = 0; i < 4; i++) {
			currentFgSpans[i] = null;
			currentBgSpans[i] = null;
		}

		int in = 0, out = 0;

		for (in = 0; in < source.length(); in++) {
			char c = source.charAt(in);
			if (c >= 0 && c <= 0x1F) {
				// Control code!
				if (rawTextStart != -1) {
					s.append(source, rawTextStart, in);
					rawTextStart = -1;
				}

				// Process the code...
				if (c == 1 && boldStart == -1) {
					boldStart = out;
				} else if (c == 2 && boldStart > -1) {
					if (boldStart != out)
						s.setSpan(boldSpan, boldStart, out, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
					boldStart = -1;

				} else if (c == 3 && italicStart == -1) {
					italicStart = out;
				} else if (c == 4 && italicStart > -1) {
					if (italicStart != out)
						s.setSpan(italicSpan, italicStart, out, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
					italicStart = -1;

				} else if (c == 5 && underlineStart == -1) {
					underlineStart = out;
				} else if (c == 6 && underlineStart > -1) {
					if (underlineStart != out)
						s.setSpan(underlineSpan, underlineStart, out, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
					underlineStart = -1;

				} else if (c >= 0x10 && c <= 0x1F) {
					boolean isBG = ((c & 4) == 4);
					boolean isEnd = ((c & 8) == 8);
					int layer = (c & 3);

					// Read what comes afterwards, to decide what we're doing
					Object chosenSpan = null;

					if (!isEnd && ((in + 1) < source.length())) {
						char first = source.charAt(in + 1);

						if ((first & 1) == 1) {
							// Preset colour
							if (isBG)
								chosenSpan = presetBackgroundSpans[first >> 1];
							else
								chosenSpan = presetForegroundSpans[first >> 1];
							in++; // Skip the extra

						} else if ((in + 3) < source.length()) {
							// RGB colour
							int r = first;
							int g = source.charAt(in + 2);
							int b = source.charAt(in + 3);
							int col = 0xFF000000 | (r << 17) | (g << 9) | (b << 1);

							if (isBG) {
								if (cachedBackgroundSpans.containsKey(col)) {
									chosenSpan = cachedBackgroundSpans.get(col);
								} else {
									chosenSpan = new BackgroundColorSpan(col);
									cachedBackgroundSpans.put(col, (BackgroundColorSpan)chosenSpan);
								}
							} else {
								if (cachedForegroundSpans.containsKey(col)) {
									chosenSpan = cachedForegroundSpans.get(col);
								} else {
									chosenSpan = new ForegroundColorSpan(col);
									cachedForegroundSpans.put(col, (ForegroundColorSpan)chosenSpan);
								}
							}

							in += 3; // Skip the extra
						}
					}

					// OK, are we actually changing?
					Object oldSpan, newSpan = null;

					if (isBG)
						oldSpan = bgSpan;
					else
						oldSpan = fgSpan;

					// Modify the array, and figure out our new active span
					if (isBG) {
						if (isEnd)
							currentBgSpans[layer] = null;
						else
							currentBgSpans[layer] = (BackgroundColorSpan)chosenSpan;

						for (int i = 0; i < 4; i++)
							if (currentBgSpans[i] != null)
								newSpan = currentBgSpans[i];

					} else {
						if (isEnd)
							currentFgSpans[layer] = null;
						else
							currentFgSpans[layer] = (ForegroundColorSpan)chosenSpan;

						for (int i = 0; i < 4; i++)
							if (currentFgSpans[i] != null)
								newSpan = currentFgSpans[i];
					}

					// If we changed spans....
					if (oldSpan != newSpan) {
						// End the existing span, no matter what...
						if (isBG) {
							if (bgStart > -1) {
								if (bgStart != out)
									s.setSpan(bgSpan, bgStart, out, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
								bgStart = -1;
							}
						} else {
							if (fgStart > -1) {
								if (fgStart != out)
									s.setSpan(fgSpan, fgStart, out, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
								fgStart = -1;
							}
						}

						// .. and if we have another span, add it!
						if (newSpan != null) {
							// Preset colour
							if (isBG) {
								bgSpan = (BackgroundColorSpan)newSpan;
								bgStart = out;
							} else {
								fgSpan = (ForegroundColorSpan)newSpan;
								fgStart = out;
							}
						}
					}
				}

			} else {
				// Regular character.
				if (rawTextStart == -1) {
					rawTextStart = in;
				}
				out++;
			}
		}

		// Anything left?
		if (rawTextStart != -1) {
			s.append(source, rawTextStart, source.length());
		}

		// Any un-applied spans?
		if (boldStart > -1 && boldStart != out)
			s.setSpan(boldSpan, boldStart, out, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
		if (italicStart > -1 && italicStart != out)
			s.setSpan(italicSpan, italicStart, out, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
		if (underlineStart > -1 && underlineStart != out)
			s.setSpan(underlineSpan, underlineStart, out, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
		if (bgStart > -1 && bgStart != out)
			s.setSpan(bgSpan, bgStart, out, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
		if (fgStart > -1 && fgStart != out)
			s.setSpan(fgSpan, fgStart, out, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);

		Linkify.addLinks(s, Linkify.WEB_URLS);

		return s;
	}
}
