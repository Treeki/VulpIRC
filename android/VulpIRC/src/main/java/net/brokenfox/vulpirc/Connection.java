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
		void handleServerWindowsUpdated();
		void handlePublicWindowsUpdated();
		void handleServersUpdated();
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
		serverWindows.clear();
		publicWindows.clear();
		servers.clear();
		for (ConnectionListener l : mListeners) {
			l.handleServerWindowsUpdated();
			l.handlePublicWindowsUpdated();
			l.handleServersUpdated();
		}
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

		if (type == 0x81 || type == 0x104)
			data = Util.decompress(data);

		ByteBuffer p = ByteBuffer.wrap(data);
		p.order(ByteOrder.LITTLE_ENDIAN);

		if (type == 1) {

			statusWindow.pushMessage(Util.readStringFromBuffer(p));

		} else if (type == 0x80 || type == 0x81 || type == 0x100 || type == 0x104) {
			_packet_addWindows(p);
			if (type <= 0x81)
				_packet_addServers(p);

		} else if (type == 0x101) {
			_packet_removeWindows(p);
		} else if (type == 0x102) {
			_packet_addMessageToWindow(p);
		} else if (type == 0x103) {
			_packet_renameWindow(p);
		} else if ((type >= 0x120) && (type < 0x124)) {
			// Channel packets
			int windowID = p.getInt();
			WindowData w = findWindowByID(windowID);

			if (w != null && w instanceof ChannelData) {
				((ChannelData) w).processChannelPacket(type, p);
			}

		} else if (type == 0x140) {
			_packet_addServers(p);
		} else if (type == 0x141) {
			_packet_removeServers(p);
		} else if ((type >= 0x142) && (type <= 0x143)) {
			// Server packets
			int serverID = p.getInt();
			ServerData s = findServerByID(serverID);

			if (s != null) {
				s.processServerPacket(type, p);
			}
		}
	}

	private void _packet_addWindows(ByteBuffer p) {
		int windowCount = p.getInt();
		if (windowCount <= 0)
			return;

		boolean touchedServer = false, touchedPublic = false;

		for (int i = 0; i < windowCount; i++) {
			int windowType = p.getInt();

			WindowData w;
			if (windowType == 2)
				w = new ChannelData();
			else
				w = new WindowData();

			w.processInitialSync(p);

			windows.add(w);
			if (windowType == 1) {
				serverWindows.add(w);
				touchedServer = true;
			} else {
				publicWindows.add(w);
				touchedPublic = true;
			}
		}

		for (ConnectionListener l : mListeners) {
			if (touchedServer)
				l.handleServerWindowsUpdated();
			if (touchedPublic)
				l.handlePublicWindowsUpdated();
		}
	}

	private void _packet_removeWindows(ByteBuffer p) {
		int windowCount = p.getInt();
		if (windowCount <= 0)
			return;

		for (int i = 0; i < windowCount; i++) {
			int windowID = p.getInt();
			Object window = null;

			for (int j = 0; j < windows.size(); j++) {
				if (windows.get(j).id == windowID) {
					window = windows.get(j);
					break;
				}
			}

			if (window != null) {
				if (window == mActiveWindow)
					mActiveWindow = null;

				windows.remove(window);
				serverWindows.remove(window);
				publicWindows.remove(window);
			}
		}

		for (ConnectionListener l : mListeners) {
			l.handleServerWindowsUpdated();
			l.handlePublicWindowsUpdated();
		}
	}

	private void _packet_addMessageToWindow(ByteBuffer p) {
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
	}

	private void _packet_renameWindow(ByteBuffer p) {
		int windowID = p.getInt();
		String newTitle = Util.readStringFromBuffer(p);

		WindowData w = findWindowByID(windowID);
		if (w != null)
			w.setTitle(newTitle);
	}

	private void _packet_addServers(ByteBuffer p) {
		int serverCount = p.getInt();
		if (serverCount <= 0)
			return;

		for (int i = 0; i < serverCount; i++) {
			int id = p.getInt();
			String serverType = Util.readStringFromBuffer(p);
			int dataSize = p.getInt();
			int endPos = p.position() + dataSize;

			ServerData s;

			// For now, assume that every server is an IRC Server...
			// ...this will change eventually.

			s = new ServerData();
			s.id = id;

			s.processInitialSync(p);
			p.position(endPos);

			servers.add(s);
		}

		notifyServersUpdated();
	}

	private void _packet_removeServers(ByteBuffer p) {
		int serverCount = p.getInt();
		if (serverCount <= 0)
			return;

		for (int i = 0; i < serverCount; i++) {
			int serverID = p.getInt();
			Object server = null;

			for (int j = 0; j < servers.size(); j++) {
				if (servers.get(j).id == serverID) {
					server = servers.get(j);
					break;
				}
			}

			if (server != null) {
				servers.remove(server);
			}
		}

		notifyServersUpdated();
	}

	public WindowData findWindowByID(int id) {
		if (id == -1)
			return statusWindow;

		for (WindowData w : windows)
			if (w.id == id)
				return w;

		return null;
	}

	public ServerData findServerByID(int id) {
		for (ServerData s : servers)
			if (s.id == id)
				return s;

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
	public StatusData statusWindow = new StatusData();
	public ArrayList<WindowData> windows = new ArrayList<WindowData>();

	public ArrayList<WindowData> serverWindows = new ArrayList<WindowData>();
	public ArrayList<WindowData> publicWindows = new ArrayList<WindowData>();

	private WindowData mActiveWindow = null;

	public void notifyWindowsUpdated() {
		for (ConnectionListener l : mListeners) {
			l.handleServerWindowsUpdated();
			l.handlePublicWindowsUpdated();
		}
	}

	public WindowData getActiveWindow() { return mActiveWindow; }
	public void setActiveWindow(WindowData w) {
		mActiveWindow = w;
		w.setUnreadLevel(0);
	}

	// Servers, and crap
	public ArrayList<ServerData> servers = new ArrayList<ServerData>();

	public void notifyServersUpdated() {
		for (ConnectionListener l : mListeners) {
			l.handleServersUpdated();
		}
	}
}
