package net.brokenfox.vulpirc;

import android.os.Handler;
import android.os.Message;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.LinkedBlockingQueue;

/**
 * Created by ninji on 1/29/14.
 */
public class BaseConn {
	// Listeners
	public interface BaseConnListener {
		void handleSessionStarted();
		void handleSessionEnded();
		void handleSocketStateChanged();
		void handlePacketReceived(int type, byte[] data);
		void handleLoginError(String error);
		void handleStatusMessage(String message);
	}

	private BaseConnListener mListener = null;
	public void setListener(BaseConnListener l) {
		mListener = l;
	}


	// Internal communications
	private final static int MSG_SESSION_STARTED = 100;
	private final static int MSG_SESSION_ENDED = 101;
	private final static int MSG_SOCKET_STATE_CHANGED = 102;
	private final static int MSG_PACKET_RECEIVED = 103;
	private final static int MSG_LOGIN_ERROR = 104;
	private final static int MSG_STATUS_MESSAGE = 105;
	private final static int MSG_TIMED_RECONNECT = 200;

	private final Handler.Callback mHandlerCallback = new Handler.Callback() {
		@Override
		public boolean handleMessage(Message message) {
			switch (message.what) {
				case MSG_SESSION_STARTED:
					if (mListener != null)
						mListener.handleSessionStarted();
					return true;
				case MSG_SESSION_ENDED:
					if (mListener != null)
						mListener.handleSessionEnded();
					return true;
				case MSG_SOCKET_STATE_CHANGED:
					if (mListener != null)
						mListener.handleSocketStateChanged();
					return true;
				case MSG_PACKET_RECEIVED:
					if (mListener != null)
						mListener.handlePacketReceived(message.arg1, (byte[]) message.obj);
					return true;
				case MSG_LOGIN_ERROR:
					if (mListener != null)
						mListener.handleLoginError((String)message.obj);
					return true;
				case MSG_STATUS_MESSAGE:
					if (mListener != null)
						mListener.handleStatusMessage((String)message.obj);
					return true;
				case MSG_TIMED_RECONNECT:
					initiateConnection();
					return true;
			}
			return false;
		}
	};
	private final Handler mHandler = new Handler(mHandlerCallback);




	// Packet system
	public final static int PROTOCOL_VERSION = 3;
	private final static int SESSION_KEY_SIZE = 16;

	private static class Packet {
		public int id;
		public int type;
		public byte[] data;
	}

	private final Object mPacketLock = new Object();

	// Anything that touches the following fields must lock on
	// mPacketLock first..!
	private final ArrayList<Packet> mPacketCache =
			new ArrayList<Packet>();

	private byte[] mSessionKey = null;
	private int mNextPacketID = 1;
	private int mLastReceivedFromServer = 0;
	private boolean mSessionActive = false;
	private boolean mSessionWillTerminate = false;

	public boolean getSessionActive() { return mSessionActive; }

	// NOTE: This executes on the Socket thread, so locking
	// is absolutely necessary!
	private void processRawPacket(int packetType, int packetSize, int msgID, int lastReceivedByServer, byte[] buffer, int bufferPos) {
		Log.i("VulpIRC", "Packet received: " + packetType + ", " + packetSize);

		if ((packetType & 0x8000) == 0) {
			// For in-band packets, handle the caching junk
			synchronized (mPacketLock) {
				clearCachedPackets(lastReceivedByServer);

				if (msgID > mLastReceivedFromServer) {
					// This is a new packet
					mLastReceivedFromServer = msgID;
				} else {
					// We've already seen this packet, so ignore it
					return;
				}
			}

			// Fire off the packet to the Handler for further
			// processing by a subclass
			mHandler.sendMessage(mHandler.obtainMessage(
					MSG_PACKET_RECEIVED,
					packetType, 0,
					Arrays.copyOfRange(buffer, bufferPos, bufferPos + packetSize)));
		} else {
			// Out-of-band packets are handled here.

			synchronized (mPacketLock) {
				switch (packetType) {
					case 0x8001:
						// Successful login
						Log.i("VulpIRC", "*** Successful login. ***");
						mHandler.sendMessage(
								mHandler.obtainMessage(MSG_STATUS_MESSAGE,
										"Logged in successfully."));

						mSessionKey = Arrays.copyOfRange(buffer, bufferPos, bufferPos + packetSize);
						mLastReceivedFromServer = 0;
						mNextPacketID = 1;
						mPacketCache.clear();

						if (mSessionActive)
							mHandler.sendEmptyMessage(MSG_SESSION_ENDED);
						mSessionActive = true;
						mSessionWillTerminate = false;
						mHandler.sendEmptyMessage(MSG_SESSION_STARTED);

						break;

					case 0x8002:
						// Login failure. Output this to the UI somewhere?
						Log.e("VulpIRC", "*** Login failed! ***");

						ByteBuffer b = ByteBuffer.wrap(buffer);
						b.order(ByteOrder.LITTLE_ENDIAN);
						b.position(bufferPos);
						int code = b.getInt();

						mHandler.sendMessage(
								mHandler.obtainMessage(MSG_LOGIN_ERROR,
										"Login failure: " + code));

						if (mSessionActive)
							mHandler.sendEmptyMessage(MSG_SESSION_ENDED);
						mSessionActive = false;

						requestEndSession();

						break;

					case 0x8003:
						// Session resumed.
						Log.i("VulpIRC", "*** Session resumed. ***");
						mHandler.sendMessage(
								mHandler.obtainMessage(MSG_STATUS_MESSAGE,
										"Session resumed."));

						lastReceivedByServer = buffer[bufferPos] |
								(buffer[bufferPos + 1] << 8) |
								(buffer[bufferPos + 2] << 16) |
								(buffer[bufferPos + 3] << 24);

						clearCachedPackets(lastReceivedByServer);
						for (Packet p : mPacketCache)
							sendPacketOverWire(p);
						break;
				}
			}
		}
	}


	// This function must only be called while mPacketLock is held!
	private void clearCachedPackets(int maxID) {
		while (!mPacketCache.isEmpty()) {
			if (mPacketCache.get(0).id > maxID)
				break;
			else
				mPacketCache.remove(0);
		}
	}


	private void sendPacketOverWire(Packet p) {
		int headerSize = ((p.type & 0x8000) == 0) ? 16 : 8;
		ByteBuffer b = ByteBuffer.allocate(headerSize + p.data.length);

		b.order(ByteOrder.LITTLE_ENDIAN);
		b.putShort((short)p.type);
		b.putShort((short)0);
		b.putInt(p.data.length);

		if ((p.type & 0x8000) == 0) {
			b.putInt(p.id);
			b.putInt(mLastReceivedFromServer);
		}

		b.put(p.data);

		mWriteQueue.offer(b.array());
	}


	public void sendPacket(int type, byte[] data) {
		Packet p = new Packet();
		p.type = type;

		if ((type & 0x8000) == 0) {
			p.id = mNextPacketID;
			++mNextPacketID;
		}

		p.data = data;

		synchronized (mPacketLock) {
			if ((type & 0x8000) == 0)
				mPacketCache.add(p);

			synchronized (mSocketStateLock) {
				if (mSocketState == SocketState.CONNECTED && mSessionActive)
					sendPacketOverWire(p);
			}
		}
	}


	// Socket junk
	private Socket mSocket = null;
	private LinkedBlockingQueue<byte[]> mWriteQueue =
			new LinkedBlockingQueue<byte[]>();


	public enum SocketState {
		DISCONNECTED, CONNECTING, CONNECTED, DISCONNECTING
	}
	// mSocketState can be accessed from multiple threads
	// Always synchronise on the lock before doing anything with it!
	private final Object mSocketStateLock = new Object();
	private SocketState mSocketState = SocketState.DISCONNECTED;
	public SocketState getSocketState() { return mSocketState; }

	private String mHostname, mUsername, mPassword;
	private int mPort;
	private boolean mUseTls;


	private class WriterThread implements Runnable {
		private OutputStream mOutputStream = null;
		public WriterThread(OutputStream os) {
			mOutputStream = os;
		}

		@Override
		public void run() {
			// Should I be using a BufferedOutputStream?
			// Not sure.

			while (true) {
				try {
					byte[] data = mWriteQueue.take();
					mOutputStream.write(data);
					mOutputStream.flush();
				} catch (IOException e) {
					// oops
					Log.e("VulpIRC", "WriterThread write failure:");
					Log.e("VulpIRC", e.toString());
					// Should disconnect here? Maybe?
					break;
				} catch (InterruptedException e) {
					// nothing wrong here
					break;
				}
			}
		}
	}
	private Thread mSocketThread = null;

	private class SocketThread implements Runnable {
		@Override
		public void run() {
			synchronized (mSocketStateLock) {
				mSocketState = SocketState.CONNECTING;
				mHandler.sendEmptyMessage(MSG_SOCKET_STATE_CHANGED);
			}

			Log.i("VulpIRC", "SocketThread running");

			// No SSL just yet, let's simplify things for now

			mSocket = new Socket();
			try {
				mHandler.sendMessage(
						mHandler.obtainMessage(MSG_STATUS_MESSAGE,
								"Connecting..."));

				mSocket.connect(new InetSocketAddress(mHostname, mPort));
			} catch (IOException e) {
				mHandler.sendMessage(
						mHandler.obtainMessage(MSG_STATUS_MESSAGE,
								"Connection failure (1): " + e.toString()));

				mHandler.sendMessage(mHandler.obtainMessage(MSG_LOGIN_ERROR, e.toString()));
				cleanUpConnection();
				return;
			}

			// We're connected, yay
			Log.i("VulpIRC", "Connected!");

			InputStream inputStream;
			OutputStream outputStream;

			try {
				inputStream = mSocket.getInputStream();
			} catch (IOException e) {
				mHandler.sendMessage(
						mHandler.obtainMessage(MSG_STATUS_MESSAGE,
								"Connection failure (2): " + e.toString()));

				mHandler.sendMessage(mHandler.obtainMessage(MSG_LOGIN_ERROR, e.toString()));
				cleanUpConnection();
				return;
			}

			try {
				outputStream = mSocket.getOutputStream();
			} catch (IOException e) {
				mHandler.sendMessage(
						mHandler.obtainMessage(MSG_STATUS_MESSAGE,
								"Connection failure (3): " + e.toString()));

				mHandler.sendMessage(mHandler.obtainMessage(MSG_LOGIN_ERROR, e.toString()));
				cleanUpConnection();
				return;
			}

			// Start up our writer
			mWriteQueue.clear();

			Thread writerThread = new Thread(new WriterThread(outputStream));
			writerThread.start();

			// Send the initial login packet
			byte[] encPW = Util.encodeString(mPassword);

			ByteBuffer loginData = ByteBuffer.allocate(12 + encPW.length + SESSION_KEY_SIZE);
			loginData.order(ByteOrder.LITTLE_ENDIAN);
			loginData.putInt(PROTOCOL_VERSION);
			loginData.putInt(mLastReceivedFromServer);
			loginData.putInt(encPW.length);
			loginData.put(encPW);

			if (mSessionKey == null) {
				for (int i = 0; i < SESSION_KEY_SIZE; i++)
					loginData.put((byte)0);
			} else {
				loginData.put(mSessionKey);
			}

			Packet loginPack = new Packet();
			loginPack.type = 0x8001;
			loginPack.data = loginData.array();
			sendPacketOverWire(loginPack);

			// Let's go !
			synchronized (mSocketStateLock) {
				mSocketState = SocketState.CONNECTED;
				mHandler.sendEmptyMessage(MSG_SOCKET_STATE_CHANGED);
			}

			Log.i("VulpIRC", "Beginning socket read");

			byte[] readBuffer = new byte[16384];
			int readBufSize = 0;

			while (true) {
				// Do we have enough space to (try and) read 8k?
				// If not, make the buffer bigger
				if ((readBufSize + 8192) > readBuffer.length)
					readBuffer = Arrays.copyOf(readBuffer, readBuffer.length + 16384);

				// OK, now we should have enough!
				try {
					int amount = inputStream.read(readBuffer, readBufSize, 8192);
					readBufSize += amount;

					if (amount == -1)
						break;
				} catch (IOException e) {
					mHandler.sendMessage(
							mHandler.obtainMessage(MSG_STATUS_MESSAGE,
									"Connection lost (4): " + e.toString()));

					break;
				}

				// Try and parse as many packets as we can!
				int pos = 0;

				while (true) {
					// Do we have enough to parse a packet header?
					if ((readBufSize - pos) < 8)
						break;

					int headerSize = 8;
					int packetType = (0xFF & (int)readBuffer[pos]) |
							((0xFF & readBuffer[pos + 1]) << 8);
					int packetSize = (0xFF & (int)readBuffer[pos + 4]) |
							((0xFF & (int)readBuffer[pos + 5]) << 8) |
							((0xFF & (int)readBuffer[pos + 6]) << 16) |
							((0xFF & (int)readBuffer[pos + 7]) << 24);

					int msgID = 0, lastReceivedByServer = 0;

					// In-band packets have extra stuff
					if ((packetType & 0x8000) == 0) {
						if ((readBufSize - pos) < 16)
							break;

						headerSize = 16;

						msgID = (0xFF & (int)readBuffer[pos + 8]) |
								((0xFF & (int)readBuffer[pos + 9]) << 8) |
								((0xFF & (int)readBuffer[pos + 10]) << 16) |
								((0xFF & (int)readBuffer[pos + 11]) << 24);
						lastReceivedByServer = (0xFF & (int)readBuffer[pos + 12]) |
								((0xFF & (int)readBuffer[pos + 13]) << 8) |
								((0xFF & (int)readBuffer[pos + 14]) << 16) |
								((0xFF & (int)readBuffer[pos + 15]) << 24);
					}

					// Negative packet sizes aren't right
					if (packetSize < 0)
						break;

					// Enough data?
					if ((readBufSize - pos) < (packetSize + headerSize))
						break;

					// OK, this should mean we can parse things now!
					processRawPacket(
							packetType, packetSize,
							msgID, lastReceivedByServer,
							readBuffer, pos + headerSize);

					pos += headerSize + packetSize;
				}

				if (pos > 0) {
					if (pos >= readBufSize) {
						// We've read everything, no copying needed, just wipe it all
						readBufSize = 0;
					} else {
						// Move the remainder to the beginning of the buffer
						System.arraycopy(readBuffer, pos, readBuffer, 0, readBufSize - pos);
						readBufSize -= pos;
					}
				}
			}

			synchronized (mSocketStateLock) {
				mSocketState = SocketState.DISCONNECTING;
				mHandler.sendEmptyMessage(MSG_SOCKET_STATE_CHANGED);
			}

			// Clean up everything
			writerThread.interrupt();
			cleanUpConnection();
		}

		private void cleanUpConnection() {
			if (mSocket != null) {
				try {
					mSocket.close();
				} catch (IOException e) {
					// don't care
				}
				mSocket = null;
			}

			synchronized (mSocketStateLock) {
				synchronized (mPacketLock) {
					if (mSessionWillTerminate) {
						mHandler.sendMessage(
								mHandler.obtainMessage(MSG_STATUS_MESSAGE,
										"Session completed."));

						mSessionWillTerminate = false;
						mSessionKey = null;
						if (mSessionActive)
							mHandler.sendEmptyMessage(MSG_SESSION_ENDED);
						mSessionActive = false;
						mHandler.removeMessages(MSG_TIMED_RECONNECT);
					} else if (mSessionActive) {
						mHandler.sendMessage(
								mHandler.obtainMessage(MSG_STATUS_MESSAGE,
										"Connection closed. Reconnecting in 15 seconds."));
						mHandler.sendEmptyMessageDelayed(MSG_TIMED_RECONNECT, 15 * 1000);
					}
				}

				mSocketState = SocketState.DISCONNECTED;
				mSocketThread = null;
				mHandler.sendEmptyMessage(MSG_SOCKET_STATE_CHANGED);
			}

			Log.i("VulpIRC", "Connection closed.");
		}
	}


	public boolean initiateConnection(String hostname, int port, boolean useTls, String username, String password) {
		synchronized (mSocketStateLock) {
			if (mSocketState != SocketState.DISCONNECTED)
				return false;

			mHostname = hostname;
			mPort = port;
			mUseTls = useTls;
			mUsername = username;
			mPassword = password;

			return initiateConnection();
		}
	}

	public boolean initiateConnection() {
		synchronized (mSocketStateLock) {
			if (mSocketState != SocketState.DISCONNECTED)
				return false;

			mSocketState = SocketState.CONNECTING;
			mHandler.sendEmptyMessage(MSG_SOCKET_STATE_CHANGED);

			mSocketThread = new Thread(new SocketThread());
			mSocketThread.start();

			return true;
		}
	}

	public boolean requestDisconnection() {
		synchronized (mSocketStateLock) {
			if (mSocketState != SocketState.DISCONNECTED) {
				try {
					if (mSocket != null)
						mSocket.close();
				} catch (IOException e) {
					Log.w("VulpIRC", "requestDisconnection could not close socket:");
					Log.w("VulpIRC", e.toString());
				}

				mSocketThread.interrupt();
				return true;
			}
			return false;
		}
	}	
	public boolean requestEndSession() {
		// Should send an OOB logout packet to the server too, if possible.

		synchronized (mSocketStateLock) {
			mHandler.removeMessages(MSG_TIMED_RECONNECT);

			synchronized (mPacketLock) {
				if (mSocketState == SocketState.DISCONNECTED) {
					if (mSessionActive) {
						mSessionActive = false;
						mHandler.sendEmptyMessage(MSG_SESSION_ENDED);
					}
					return true;
				} else {
					mSessionWillTerminate = true;
					return requestDisconnection();
				}
			}
		}
	}
}
