package net.brokenfox.vulpirc;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.text.DateFormat;
import java.text.DateFormatSymbols;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;

/**
 * Created by ninji on 4/6/14.
 */
public class ServerData {
	public int id;

	public interface ServerListener {
		void handleConnStateChanged();
	}

	protected ArrayList<ServerListener> mListeners = new ArrayList<ServerListener>();

	public void registerListener(ServerListener l) {
		mListeners.add(l);
	}
	public void deregisterListener(ServerListener l) {
		mListeners.remove(l);
	}

	// TODO: split me into an IRCServerData class...!
	public int state;
	public String title;
	public String hostname, username, realname;
	public String nickname, altNick, password;
	public int port;
	public boolean useTls;
	public int statusWindowId;

	public final static int REQUESTED_ACTION_CONNECT = 1;
	public final static int REQUESTED_ACTION_DISCONNECT = 2;
	public int requestedAction = 0;

	public void processInitialSync(ByteBuffer p) {
		state = p.getInt();
		statusWindowId = p.getInt();
		readConfig(p);
	}

	public void readConfig(ByteBuffer p) {
		title = Util.readStringFromBuffer(p);
		hostname = Util.readStringFromBuffer(p);
		username = Util.readStringFromBuffer(p);
		realname = Util.readStringFromBuffer(p);
		nickname = Util.readStringFromBuffer(p);
		altNick = Util.readStringFromBuffer(p);
		password = Util.readStringFromBuffer(p);
		port = p.getInt();
		useTls = (p.get() != 0);
	}

	public void sendConfigToServer() {
		// This is awful and I feel awful for writing it.

		byte[] eTitle = Util.encodeString(title);
		byte[] eHostname = Util.encodeString(hostname);
		byte[] eUsername = Util.encodeString(username);
		byte[] eRealname = Util.encodeString(realname);
		byte[] eNickname = Util.encodeString(nickname);
		byte[] eAltNick = Util.encodeString(altNick);
		byte[] ePassword = Util.encodeString(password);

		int bufSize = 4 + (7 * 4) + 5 +
				eTitle.length +
				eHostname.length +
				eUsername.length +
				eRealname.length +
				eNickname.length +
				eAltNick.length +
				ePassword.length;

		ByteBuffer buf = ByteBuffer.allocate(bufSize);
		buf.order(ByteOrder.LITTLE_ENDIAN);

		buf.putInt(id);
		buf.putInt(eTitle.length); buf.put(eTitle);
		buf.putInt(eHostname.length); buf.put(eHostname);
		buf.putInt(eUsername.length); buf.put(eUsername);
		buf.putInt(eRealname.length); buf.put(eRealname);
		buf.putInt(eNickname.length); buf.put(eNickname);
		buf.putInt(eAltNick.length); buf.put(eAltNick);
		buf.putInt(ePassword.length); buf.put(ePassword);
		buf.putInt(port);
		buf.put((byte)(useTls ? 1 : 0));

		Connection.get().sendPacket(0x143, buf.array());
	}

	public void processServerPacket(int type, ByteBuffer p) {
		if (type == 0x142) {
			state = p.getInt();
			if (requestedAction == REQUESTED_ACTION_CONNECT) {
				if (state > 0)
					requestedAction = 0;
			} else if (requestedAction == REQUESTED_ACTION_DISCONNECT) {
				if (state == 0)
					requestedAction = 0;
			}
			Connection.get().notifyServersUpdated();
			for (ServerListener l : mListeners)
				l.handleConnStateChanged();
		} else if (type == 0x143) {
			readConfig(p);
			Connection.get().notifyServersUpdated();
		}
	}


	public String getDisplayTitle() {
		if (title.isEmpty())
			return hostname;
		else
			return title;
	}

	public String getDisplayState() {
		String prefix = "Unknown state";
		String suffix = "";

		switch (state) {
			case 0: prefix = "Disconnected"; break;
			case 1: prefix = "Looking up..."; break;
			case 2: prefix = "Connecting..."; break;
			case 3: prefix = "Waiting for TLS handshake..."; break;
			case 4: prefix = "Connected"; break;
		}

		switch (requestedAction) {
			case REQUESTED_ACTION_CONNECT:
				suffix = " (Connection requested)";
				break;
			case REQUESTED_ACTION_DISCONNECT:
				suffix = " (Disconnection requested)";
				break;
		}

		return prefix + suffix;
	}

	public boolean isConnSwitchEnabled() {
		return (requestedAction == 0);
	}
	public boolean isConnSwitchChecked() {
		if (requestedAction == REQUESTED_ACTION_CONNECT)
			return true;
		else if (requestedAction == REQUESTED_ACTION_DISCONNECT)
			return false;
		else
			return (state != 0);
	}

	public void connSwitchToggled(boolean b) {
		if (requestedAction != 0)
			return;

		if (state == 0) {
			if (b)
				requestedAction = REQUESTED_ACTION_CONNECT;
			else
				return; // already disconnected!
		} else {
			if (b)
				return; // already connecting/connected!
			else
				requestedAction = REQUESTED_ACTION_DISCONNECT;
		}


		ByteBuffer buf = ByteBuffer.allocate(5);
		buf.order(ByteOrder.LITTLE_ENDIAN);

		buf.putInt(id);
		buf.put((byte)requestedAction);

		Connection.get().sendPacket(0x142, buf.array());
	}
}
