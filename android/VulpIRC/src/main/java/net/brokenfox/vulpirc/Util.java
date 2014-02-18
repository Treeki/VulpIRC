package net.brokenfox.vulpirc;

import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.*;

/**
 * Created by ninji on 2/3/14.
 */
public class Util {

	private static CharsetDecoder mUTF8Decoder = null;
	private static CharsetEncoder mUTF8Encoder = null;

	public static synchronized String readStringFromBuffer(ByteBuffer p) {
		if (mUTF8Decoder == null) {
			mUTF8Decoder = Charset.forName("UTF-8").newDecoder();
			mUTF8Decoder.onMalformedInput(CodingErrorAction.REPLACE);
		}

		int size = p.getInt();
		if (size <= 0)
			return "";

		int beginPos = p.position();
		int endPos = beginPos + size;
		int saveLimit = p.limit();
		p.limit(endPos);

		String result = "";

		try {
			result = mUTF8Decoder.decode(p).toString();
		} catch (CharacterCodingException e) {
			Log.e("VulpIRC", "Utils.readStringFromBuffer caught decode exception:");
			Log.e("VulpIRC", e.toString());
		}

		p.limit(saveLimit);
		p.position(endPos);

		return result;
	}

	public static synchronized byte[] encodeString(CharSequence s) {
		if (mUTF8Encoder == null)
			mUTF8Encoder = Charset.forName("UTF-8").newEncoder();

		try {
			return mUTF8Encoder.encode(CharBuffer.wrap(s)).array();
		} catch (CharacterCodingException e) {
			Log.e("VulpIRC", "Utils.encodeString caught encode exception:");
			Log.e("VulpIRC", e.toString());

			return new byte[0];
		}
	}

}
