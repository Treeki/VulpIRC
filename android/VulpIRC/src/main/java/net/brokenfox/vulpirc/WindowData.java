package net.brokenfox.vulpirc;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.text.format.Time;
import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.text.*;
import java.util.ArrayList;
import java.util.Date;

/**
 * Created by ninji on 2/3/14.
 */
public class WindowData {
	public int id;
	public String title;
	public int unreadLevel = 0;

	public ArrayList<CharSequence> messages = new ArrayList<CharSequence>();

	protected Fragment instantiateFragmentClass() {
		return new WindowFragment();
	}
	public Fragment createFragment() {
		Bundle args = new Bundle();
		args.putInt("winID", id);

		Fragment wf = instantiateFragmentClass();
		wf.setArguments(args);
		return wf;
	}

	public void setUnreadLevel(int newLevel) {
		unreadLevel = newLevel;
		Connection.get().notifyWindowsUpdated();
	}

	public void setTitle(String newTitle) {
		title = newTitle;
		Connection.get().notifyWindowsUpdated();
	}


	public interface WindowListener {
		void handleMessagesChanged();
	}

	protected ArrayList<WindowListener> mListeners = new ArrayList<WindowListener>();

	public void registerListener(WindowListener l) {
		mListeners.add(l);
	}
	public void deregisterListener(WindowListener l) {
		mListeners.remove(l);
	}



	// This is a kludge.
	private final DateFormat timestampFormat = new SimpleDateFormat("\u0001[\u0002\u0010\u001DHH:mm:ss\u0018\u0001]\u0002 ", new DateFormatSymbols());

	public void pushMessage(String message) {
		pushMessage(message, new Date(), 0);
	}
	public void pushMessage(String message, Date when, int ackID) {
		messages.add(RichText.process(timestampFormat.format(when) + message));
		for (WindowListener l : mListeners)
			l.handleMessagesChanged();
	}


	public void processInitialSync(ByteBuffer p) {
		id = p.getInt();
		title = Util.readStringFromBuffer(p);

		int messageCount = p.getInt();
		Log.i("VulpIRC", "id=" + id + ", title=[" + title + "]");

		if (messageCount > 0) {
			messages.ensureCapacity(messageCount);
			for (int j = 0; j < messageCount; j++) {
				int time = p.getInt();
				String msg = Util.readStringFromBuffer(p);
				//Log.i("VulpIRC", "msg " + j + ": " + msg);
				messages.add(RichText.process(timestampFormat.format(new Date((long)time * 1000)) + msg));
			}
		}
	}



	public void sendUserInput(CharSequence message) {
		byte[] enc = Util.encodeString(message);

		ByteBuffer buf = ByteBuffer.allocate(9 + enc.length);
		buf.order(ByteOrder.LITTLE_ENDIAN);

		buf.putInt(id);
		buf.put((byte)0);
		buf.putInt(enc.length);
		buf.put(enc);

		Connection.get().sendPacket(0x102, buf.array());
	}
}
