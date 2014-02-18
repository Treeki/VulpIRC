package net.brokenfox.vulpirc;

import android.support.v4.app.Fragment;

import java.nio.ByteBuffer;
import java.util.HashMap;

/**
 * Created by ninji on 2/6/14.
 */
public class ChannelData extends WindowData {
	public static class UserData {
		public int id;
		public int modes;
		public char prefix;
	}
	public HashMap<String, UserData> users = new HashMap<String, UserData>();
	public String topic;

	public interface ChannelListener extends WindowListener {
		void handleUsersChanged();
	}

	private int mNextUserID = 1;


	@Override
	protected Fragment instantiateFragmentClass() {
		return new ChannelFragment();
	}

	@Override
	public void processInitialSync(ByteBuffer p) {
		super.processInitialSync(p);

		processAddUsers(p);
		processChangeTopic(p);
	}

	public void processChannelPacket(int type, ByteBuffer p) {
		switch (type) {
			case 0x120: processAddUsers(p); break;
			case 0x121: processRemoveUsers(p); break;
			case 0x122: processRenameUser(p); break;
			case 0x123: processChangeUserMode(p); break;
			case 0x124: processChangeTopic(p); break;
		}
	}

	private void processAddUsers(ByteBuffer p) {
		int count = p.getInt();

		for (int i = 0; i < count; i++) {
			String nick = Util.readStringFromBuffer(p);

			UserData d = new UserData();
			d.id = ++mNextUserID;
			d.modes = p.getInt();
			d.prefix = (char)p.get();

			users.put(nick, d);
		}

		if (count > 0)
			for (WindowListener l : mListeners)
				((ChannelListener)l).handleUsersChanged();
	}

	private void processRemoveUsers(ByteBuffer p) {
		int count = p.getInt();

		if (count == 0) {
			users.clear();
		} else {
			for (int i = 0; i < count; i++) {
				String nick = Util.readStringFromBuffer(p);
				users.remove(nick);
			}
		}

		for (WindowListener l : mListeners)
			((ChannelListener)l).handleUsersChanged();
	}

	private void processRenameUser(ByteBuffer p) {
		String fromNick = Util.readStringFromBuffer(p);
		String toNick = Util.readStringFromBuffer(p);

		if (users.containsKey(fromNick)) {
			users.put(toNick, users.get(fromNick));
			users.remove(fromNick);

			for (WindowListener l : mListeners)
				((ChannelListener)l).handleUsersChanged();
		}
	}

	private void processChangeUserMode(ByteBuffer p) {
		String nick = Util.readStringFromBuffer(p);
		int modes = p.getInt();
		char prefix = (char)p.get();

		if (users.containsKey(nick)) {
			UserData d = users.get(nick);
			d.modes = modes;
			d.prefix = prefix;

			for (WindowListener l : mListeners)
				((ChannelListener)l).handleUsersChanged();
		}
	}

	private void processChangeTopic(ByteBuffer p) {
		topic = Util.readStringFromBuffer(p);
	}
}
