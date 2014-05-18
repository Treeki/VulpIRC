package net.brokenfox.vulpirc;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.text.TextUtils;
import android.util.Log;
import android.view.Menu;
import android.view.View;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

public class LoginActivity extends Activity implements Connection.NotificationListener {
	private View mContainerLoginForm;
	private View mContainerLoginStatus;
	private TextView mLoginStatus;
	private EditText mInputUsername, mInputPassword;
	private EditText mInputHostname, mInputPort;
	private CheckBox mCheckUseTls;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_login);

	    mContainerLoginForm = findViewById(R.id.containerLoginForm);
	    mContainerLoginStatus = findViewById(R.id.containerLoginStatus);

	    mLoginStatus = (TextView)findViewById(R.id.loginStatus);

	    mInputUsername = (EditText)findViewById(R.id.inputUsername);
	    mInputPassword = (EditText)findViewById(R.id.inputPassword);
	    mInputHostname = (EditText)findViewById(R.id.inputHostname);
	    mInputPort = (EditText)findViewById(R.id.inputPort);

	    mCheckUseTls = (CheckBox)findViewById(R.id.checkUseTls);

	    findViewById(R.id.buttonConnect).setOnClickListener(new View.OnClickListener() {
		    @Override
		    public void onClick(View view) {
			    tryAndConnect();
		    }
	    });

	    findViewById(R.id.buttonCancel).setOnClickListener(new View.OnClickListener() {
		    @Override
		    public void onClick(View view) {
			    tryAndCancel();
		    }
	    });

	    doBindService();
    }

	@Override
	protected void onDestroy() {
		doUnbindService();
		super.onDestroy();
	}

	@Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        getMenuInflater().inflate(R.menu.login, menu);
        return true;
    }



	// Junk.

	private boolean mIsConnecting = false;

	private void tryAndConnect() {
		if (mIsConnecting)
			return;

		mInputUsername.setError(null);
		mInputPassword.setError(null);
		mInputHostname.setError(null);
		mInputPort.setError(null);

		String username = mInputUsername.getText().toString();
		String password = mInputPassword.getText().toString();
		String hostname = mInputHostname.getText().toString();
		String portStr = mInputPort.getText().toString();
		int port = -1;
		if (!TextUtils.isEmpty(portStr)) {
			port = Integer.parseInt(portStr);
		}

		boolean usesTls = mCheckUseTls.isChecked();

		View errorView = null;

		//if (TextUtils.isEmpty(password)) {
		//	mInputPassword.setError("Required");
		//	errorView = mInputPassword;
		//}
		if (TextUtils.isEmpty(username)) {
			mInputUsername.setError("Required");
			errorView = mInputUsername;
		}

		if (port < 1 || port > 65535) {
			mInputPort.setError("Invalid");
			errorView = mInputPort;
		}
		if (TextUtils.isEmpty(hostname)) {
			mInputHostname.setError("Required");
			errorView = mInputHostname;
		}


		if (errorView != null) {
			errorView.requestFocus();
		} else {
			mIsConnecting = true;

			mLoginStatus.setText("Starting up...");

			showProgress(true);

			// start service so it'll remain around...
			Intent i = new Intent(this, IRCService.class);
			startService(i);

			Connection.get().connect(hostname, port, usesTls, username, password);
		}
	}

	private void tryAndCancel() {
		Connection.get().disconnect();
	}



	public void handleRefreshOngoingNotify() {
		switch (Connection.get().getSocketState()) {
			case DISCONNECTED:
				mLoginStatus.setText("Disconnected.");
				String details = Connection.get().getLoginError();
				if (details != null) {
					Toast.makeText(this, details, Toast.LENGTH_LONG).show();
					Connection.get().clearLoginError();
				}
				showProgress(false);
				mIsConnecting = false;
				break;
			case DISCONNECTING:
				mLoginStatus.setText("Disconnecting...");
				break;
			case CONNECTING:
				mLoginStatus.setText("Connecting...");
				break;
			case CONNECTED:
				if (Connection.get().getSessionActive()) {
					mLoginStatus.setText("Connected!");

					completeLoginScreen();
				} else {
					mLoginStatus.setText("Logging in...");
				}
				break;
		}
	}

	@Override
	public void handleNotifyOnHighlight(WindowData window, CharSequence text) {
		// Nothing here!
	}

	private void showProgress(final boolean show) {
		// Straight from the LoginActivity template!
		// .... Almost.

		int shortAnimTime = getResources().getInteger(android.R.integer.config_shortAnimTime);

		mContainerLoginStatus.setVisibility(View.VISIBLE);
		mContainerLoginStatus.animate()
				.setDuration(shortAnimTime)
				.alpha(show ? 1 : 0)
				.setListener(new AnimatorListenerAdapter() {
					@Override
					public void onAnimationEnd(Animator animation) {
						mContainerLoginStatus.setVisibility(show ? View.VISIBLE : View.INVISIBLE);
					}
				});

		mContainerLoginForm.setVisibility(View.VISIBLE);
		mContainerLoginForm.animate()
				.setDuration(shortAnimTime)
				.alpha(show ? 0 : 1)
				.setListener(new AnimatorListenerAdapter() {
					@Override
					public void onAnimationEnd(Animator animation) {
						mContainerLoginForm.setVisibility(show ? View.INVISIBLE : View.VISIBLE);
					}
				});
	}


	private void completeLoginScreen() {
		if (!isFinishing()) {
			Intent i = new Intent(LoginActivity.this, MainActivity.class);
			//i.setFlags(i.getFlags() | 0x2000); // enables halo intent, maybe?
			startActivity(i);
			finish();
		}
	}


	// SERVICE JUNK
	// Not happy about having this both here and in MainActivity.
	// Moof.
	private IRCService mService = null;

	private final ServiceConnection mServiceConnection = new ServiceConnection() {
		@Override
		public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
			Log.i("VulpIRC", "[LoginActivity connected to IRCService]");

			mService = ((IRCService.LocalBinder)iBinder).getService();
			Connection.get().registerNotificationListener(LoginActivity.this);

			if (Connection.get().getSessionActive()) {
				// We have a session, just jump right into the client
				completeLoginScreen();
			} else {
				handleRefreshOngoingNotify();
				if (Connection.get().getSocketState() == BaseConn.SocketState.DISCONNECTED) {
					mContainerLoginForm.setVisibility(View.VISIBLE);
				} else {
					mContainerLoginStatus.setVisibility(View.VISIBLE);
				}
			}
		}

		@Override
		public void onServiceDisconnected(ComponentName componentName) {
			Log.i("VulpIRC", "[LoginActivity disconnected from IRCService]");

			Connection.get().deregisterNotificationListener(LoginActivity.this);
			mService = null;
		}
	};

	private void doBindService() {
		Log.i("VulpIRC", "[LoginActivity binding to IRCService...]");

		Intent i = new Intent(this, IRCService.class);
		bindService(i, mServiceConnection, BIND_AUTO_CREATE);
	}

	private void doUnbindService() {
		Log.i("VulpIRC", "[LoginActivity unbinding from IRCService...]");

		unbindService(mServiceConnection);
	}}
