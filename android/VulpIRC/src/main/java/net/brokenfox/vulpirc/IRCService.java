package net.brokenfox.vulpirc;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.support.v4.app.NotificationCompat;

/**
 * Created by ninji on 1/30/14.
 */
public class IRCService extends Service implements Connection.NotificationListener {
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
		Connection.get().registerNotificationListener(this);

		setupNotificationSystem();
	}

	@Override
	public void onDestroy() {
		Connection.get().deregisterNotificationListener(this);
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




	private NotificationCompat.Builder mNotifyBuilder = null;
	private NotificationManager mNM = null;

	private PendingIntent mLoginIntent = null;
	private PendingIntent mMainIntent = null;

	private int mNotifyID = 1;

	private void setupNotificationSystem() {
		mNM = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);

		Intent li = new Intent(this, LoginActivity.class);
		li.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		mLoginIntent = PendingIntent.getActivity(this, 0, li, 0);

		Intent mi = new Intent(this, MainActivity.class);
		mi.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		mMainIntent = PendingIntent.getActivity(this, 0, mi, 0);

		mNotifyBuilder = new NotificationCompat.Builder(this)
				.setContentTitle("VulpIRC")
				.setContentText("{placeholder}")
				.setContentIntent(mLoginIntent)
				.setSmallIcon(R.drawable.ic_launcher)
				.setOngoing(true)
				.setDefaults(0);
	}

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

		return mNotifyBuilder
				.setContentText(desc1 + desc2)
				.setContentIntent(active ? mMainIntent : mLoginIntent)
				.setNumber(Connection.get().totalMessagesUnread)
				.build();
	}


	// IRC FUNTIMES

	@Override
	public void handleRefreshOngoingNotify() {
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

	@Override
	public void handleNotifyOnHighlight(WindowData window, CharSequence text) {
		mNotifyID++;

		// Possible improvement: use setWhen() to show a server-side timestamp

		Intent intent = new Intent(this, MainActivity.class);
		intent.putExtra("WindowID", window.id);
		PendingIntent pi = PendingIntent.getActivity(this, mNotifyID, intent, 0);

		NotificationCompat.Builder builder = new NotificationCompat.Builder(this)
				.setContentTitle("Highlight in " + window.title)
				.setContentText(text)
				.setContentIntent(pi)
				.setTicker("[" + window.title + "] " + text)
				.setSmallIcon(R.drawable.ic_launcher)
				.setAutoCancel(true)
				.setDefaults(Notification.DEFAULT_LIGHTS | Notification.DEFAULT_VIBRATE);

		mNM.notify(mNotifyID, builder.build());
	}
}
