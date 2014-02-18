package net.brokenfox.vulpirc;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;

/**
 * Created by ninji on 1/30/14.
 */
public class IRCService extends Service implements Connection.LoginStateListener {
	private final IBinder mBinder = new LocalBinder();
	public class LocalBinder extends Binder {
		IRCService getService() {
			return IRCService.this;
		}
	}


	// Lifecycle junk.


	@Override
	public void onCreate() {
		super.onCreate();
		Connection.get().registerLoginStateListener(this);
		mNM = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);
	}

	@Override
	public void onDestroy() {
		Connection.get().deregisterLoginStateListener(this);
		disableForeground();
		super.onDestroy();
	}

	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {
		// If we're killed, don't automatically restart -- we only want to be
		// started if the main VulpIRC app requests it.

		return START_NOT_STICKY;
	}

	@Override
	public IBinder onBind(Intent intent) {
		return mBinder;
	}




	private NotificationManager mNM = null;

	private boolean mInForeground = false;
	private void enableForeground() {
		Notification n = generateNotification();

		if (mInForeground) {
			// Just refresh the notification
			mNM.notify(1, n);
		} else {
			mInForeground = true;

			startForeground(1, n);
		}
	}
	private void disableForeground() {
		if (!mInForeground)
			return;

		stopForeground(true);
		mInForeground = false;
	}


	private Notification generateNotification() {
		boolean active = Connection.get().getSessionActive();
		String desc1, desc2 = "";
		desc1 = active ? "Logged in, " : "Not logged in, ";
		switch (Connection.get().getSocketState()) {
			case DISCONNECTED:
				desc2 = "disconnected";
				break;
			case DISCONNECTING:
				desc2 = "disconnecting";
				break;
			case CONNECTING:
				desc2 = "connecting...";
				break;
			case CONNECTED:
				desc2 = "connected";
				break;
		}

		Intent i = new Intent(this, active ? MainActivity.class : LoginActivity.class);
		i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		//i.setFlags(i.getFlags() | 0x2000); // enables halo intent, maybe?
		PendingIntent pi = PendingIntent.getActivity(this, 0, i, 0);

		Notification n = new Notification.Builder(this)
				.setContentTitle("VulpIRC")
				.setContentText(desc1 + desc2)
				.setContentIntent(pi)
				.setSmallIcon(R.drawable.ic_launcher)
				.setOngoing(true)
				.setDefaults(0)
				.getNotification();

		return n;
	}


	// IRC FUNTIMES

	@Override
	public void handleLoginStateChanged() {
		if (Connection.get().getSessionActive()) {
			enableForeground();
		} else {
			if (Connection.get().getSocketState() == BaseConn.SocketState.DISCONNECTED) {
				disableForeground();
				stopSelf();
			} else
				enableForeground();
		}
	}
}
