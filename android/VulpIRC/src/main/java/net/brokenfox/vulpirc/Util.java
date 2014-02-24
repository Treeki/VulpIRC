package net.brokenfox.vulpirc;

import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.CharBuffer;
import java.nio.charset.*;
import java.util.zip.DataFormatException;
import java.util.zip.Inflater;

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


	public static byte[] decompress(byte[] input) {
		ByteBuffer p = ByteBuffer.wrap(input);
		p.order(ByteOrder.LITTLE_ENDIAN);

		int decompSize = p.getInt();
		int compSize = p.getInt();

		Inflater inf = new Inflater();
		inf.setInput(input, 8, compSize);
		byte[] output = new byte[decompSize];

		try {
			int result = inf.inflate(output);
			if (result != decompSize) {
				Log.e("VulpIRC", "Util.decompress did not succeed: expected " + decompSize + " bytes, decompressed " + result);
				inf.end();
				return new byte[0];
			}
		} catch (DataFormatException e) {
			Log.e("VulpIRC", "Util.decompress caught data format exception:");
			Log.e("VulpIRC", e.toString());
			inf.end();
			return new byte[0];
		}

		inf.end();
		return output;
	}

}
