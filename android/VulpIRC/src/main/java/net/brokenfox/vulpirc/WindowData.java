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
	public int unreadCount = 0;

	public static class PendingMessage {
		public int id;
		public CharSequence text;
	}
	public ArrayList<PendingMessage> pendingMessages = new ArrayList<PendingMessage>();

	public static class Message {
		public boolean isHighlight;
		public CharSequence text;
	}
	public ArrayList<Message> messages = new ArrayList<Message>();

	private int mNextAckID = 1;
	private int allocateAckID() {
		int id = mNextAckID;
		mNextAckID++;
		if (mNextAckID > 127)
			mNextAckID = 1;
		return id;
	}


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

	public void addUnreadMessage(int newLevel) {
		boolean didChange = false;

		if (newLevel > unreadLevel) {
			unreadLevel = newLevel;
			didChange = true;
		}

		if (newLevel > 0) {
			unreadCount++;
			didChange = true;
		}

		if (didChange)
			Connection.get().notifyWindowsUpdated();
	}
	public void clearUnread() {
		unreadLevel = 0;
		unreadCount = 0;
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
		pushMessage(message, new Date(), 0, false);
	}
	public void pushMessage(String message, Date when, int ackID, boolean isHighlight) {
		boolean didChange = false;

		if (!message.isEmpty()) {
			Message m = new Message();
			m.isHighlight = isHighlight;
			m.text = RichText.process(timestampFormat.format(when) + message);
			messages.add(m);
			didChange = true;
		}

		if (ackID != 0) {
			for (PendingMessage p : pendingMessages) {
				if (p.id == ackID) {
					pendingMessages.remove(p);
					didChange = true;
					break;
				}
			}
		}

		if (didChange)
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
				boolean isHighlight = (p.get() != 0);
				//Log.i("VulpIRC", "msg " + j + ": " + msg);

				Message m = new Message();
				m.text = RichText.process(timestampFormat.format(new Date((long)time * 1000)) + msg);
				m.isHighlight = isHighlight;
				messages.add(m);
			}
		}
	}



	public void sendUserInput(CharSequence message) {
		// Add pending message to UI
		PendingMessage pend = new PendingMessage();
		pend.id = allocateAckID();
		pend.text = RichText.process(timestampFormat.format(new Date()) + message);
		pendingMessages.add(pend);

		for (WindowListener l : mListeners)
			l.handleMessagesChanged();

		// Send it to the server
		byte[] enc = Util.encodeString(message);

		ByteBuffer buf = ByteBuffer.allocate(9 + enc.length);
		buf.order(ByteOrder.LITTLE_ENDIAN);

		buf.putInt(id);
		buf.put((byte)pend.id);
		buf.putInt(enc.length);
		buf.put(enc);

		Connection.get().sendPacket(0x102, buf.array());
	}
}
