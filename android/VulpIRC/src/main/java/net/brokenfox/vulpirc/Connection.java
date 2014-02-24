package net.brokenfox.vulpirc;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Date;

/**
 * Created by ninji on 1/30/14.
 */
public class Connection implements BaseConn.BaseConnListener {
	private static Connection mInstance = new Connection();
	public static Connection get() { return mInstance; }



	private BaseConn mBaseConn = new BaseConn();
	private Connection() {
		mBaseConn.setListener(this);

		statusWindow.id = -1;
		statusWindow.title = "Status";
	}

	// Listener junk
	public interface ConnectionListener {
		void handleWindowsUpdated();
	}

	private ArrayList<ConnectionListener> mListeners = new ArrayList<ConnectionListener>();
	public void registerListener(ConnectionListener l) {
		mListeners.add(l);
	}

	public void deregisterListener(ConnectionListener l) {
		mListeners.remove(l);
	}



	private ArrayList<LoginStateListener> mLoginStateListeners = new ArrayList<LoginStateListener>();
	public interface LoginStateListener {
		void handleLoginStateChanged();
	}
	public void registerLoginStateListener(LoginStateListener l) {
		mLoginStateListeners.add(l);
	}
	public void deregisterLoginStateListener(LoginStateListener l) {
		mLoginStateListeners.remove(l);
	}


	// Connection control
	public void connect(String hostname, int port, boolean useTls, String username, String password) {
		clearLoginError();
		mBaseConn.initiateConnection(hostname, port, useTls, username, password);
	}
	public void breakConn() {
		mBaseConn.requestDisconnection();
	}
	public void disconnect() {
		mBaseConn.requestEndSession();
	}

	public BaseConn.SocketState getSocketState() {
		return mBaseConn.getSocketState();
	}
	public boolean getSessionActive() {
		return mBaseConn.getSessionActive();
	}

	private String mLoginError = null;
	public String getLoginError() {
		return mLoginError;
	}
	public void clearLoginError() {
		mLoginError = null;
	}


	// BaseConn handlers
	@Override
	public void handleSessionStarted() {
		statusWindow.pushMessage("Session started!");

		for (LoginStateListener l : mLoginStateListeners)
			l.handleLoginStateChanged();

		windows.clear();
		for (ConnectionListener l : mListeners)
			l.handleWindowsUpdated();
	}

	@Override
	public void handleSessionEnded() {
		statusWindow.pushMessage("Session ended!");

		for (LoginStateListener l : mLoginStateListeners)
			l.handleLoginStateChanged();
	}

	@Override
	public void handleSocketStateChanged() {
		for (LoginStateListener l : mLoginStateListeners)
			l.handleLoginStateChanged();
	}

	@Override
	public void handleLoginError(String error) {
		mLoginError = error;
	}

	@Override
	public void handlePacketReceived(int type, byte[] data) {
		//statusWindow.pushMessage("Packet received! " + type + " " + data.length);

		if (type == 0x104)
			data = Util.decompress(data);

		ByteBuffer p = ByteBuffer.wrap(data);
		p.order(ByteOrder.LITTLE_ENDIAN);

		if (type == 1) {

			statusWindow.pushMessage(Util.readStringFromBuffer(p));

		} else if (type == 0x100 || type == 0x104) {
			// Add windows!
			int windowCount = p.getInt();
			if (windowCount <= 0)
				return;

			for (int i = 0; i < windowCount; i++) {
				int windowType = p.getInt();

				WindowData w;
				if (windowType == 2)
					w = new ChannelData();
				else
					w = new WindowData();

				w.processInitialSync(p);

				windows.add(w);
			}

			for (ConnectionListener l : mListeners)
				l.handleWindowsUpdated();

		} else if (type == 0x101) {
			// Remove windows
			int windowCount = p.getInt();
			if (windowCount <= 0)
				return;

			for (int i = 0; i < windowCount; i++) {
				int windowID = p.getInt();
				for (int j = 0; j < windows.size(); j++) {
					if (windows.get(j).id == windowID) {
						windows.remove(j);
						break;
					}
				}

				if ((mActiveWindow != null) && (mActiveWindow.id == windowID))
					mActiveWindow = null;
			}

			for (ConnectionListener l : mListeners)
				l.handleWindowsUpdated();

		} else if (type == 0x102) {
			// Add message to window
			int windowID = p.getInt();
			int timestamp = p.getInt();
			byte priority = p.get();
			byte ack = p.get();
			String message = Util.readStringFromBuffer(p);

			WindowData w = findWindowByID(windowID);
			if (w != null) {
				w.pushMessage(message, new Date((long)timestamp * 1000), ack);

				if (priority > w.unreadLevel && w != mActiveWindow)
					w.setUnreadLevel(priority);
			}

		} else if (type == 0x103) {
			// Rename window
			int windowID = p.getInt();
			String newTitle = Util.readStringFromBuffer(p);

			WindowData w = findWindowByID(windowID);
			if (w != null)
				w.setTitle(newTitle);

		} else if ((type >= 0x120) && (type < 0x124)) {
			// Channel packets
			int windowID = p.getInt();
			WindowData w = findWindowByID(windowID);

			if (w != null && w instanceof ChannelData) {
				((ChannelData)w).processChannelPacket(type, p);
			}
		}
	}

	public WindowData findWindowByID(int id) {
		if (id == -1)
			return statusWindow;

		for (WindowData w : windows)
			if (w.id == id)
				return w;

		return null;
	}

	@Override
	public void handleStatusMessage(String message) {
		statusWindow.pushMessage(message);
	}



	public void sendPacket(int type, byte[] data) {
		mBaseConn.sendPacket(type, data);
	}


	// Windows.
	public WindowData statusWindow = new WindowData();
	public ArrayList<WindowData> windows = new ArrayList<WindowData>();
	private WindowData mActiveWindow = null;

	public void notifyWindowsUpdated() {
		for (ConnectionListener l : mListeners)
			l.handleWindowsUpdated();
	}

	public WindowData getActiveWindow() { return mActiveWindow; }
	public void setActiveWindow(WindowData w) {
		mActiveWindow = w;
		w.setUnreadLevel(0);
	}
}
