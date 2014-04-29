package net.brokenfox.vulpirc;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.FragmentActivity;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.*;


public class ServerActivity extends FragmentActivity implements ServerData.ServerListener {
	public final static String EXTRA_SERVER_ID = "net.brokenfox.vulpirc.SERVER_ID";

	private Switch mConnSwitch = null;
	private MenuItem mSaveItem = null, mDeleteItem = null, mSwitchItem = null;

	private ServerData mServer = null;

	private EditText mTitle, mNickname, mAltNick;
	private EditText mUsername, mRealName, mHostname, mPort;
	private CheckBox mUseTls, mUsePassword;
	private EditText mPassword;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_server);

		// Get fields
		mTitle = (EditText)findViewById(R.id.networkName);
		mNickname = (EditText)findViewById(R.id.nickname);
		mAltNick = (EditText)findViewById(R.id.nickname2);
		mUsername = (EditText)findViewById(R.id.username);
		mRealName = (EditText)findViewById(R.id.realName);
		mHostname = (EditText)findViewById(R.id.address);
		mPort = (EditText)findViewById(R.id.portNumber);
		mUseTls = (CheckBox)findViewById(R.id.useSSL);
		mUsePassword = (CheckBox)findViewById(R.id.useServerPassword);
		mPassword = (EditText)findViewById(R.id.serverPassword);

		// Tab setup

		final TabHost tabs = (TabHost)findViewById(R.id.tabHost);
		tabs.setup();
		tabs.setOnTabChangedListener(new TabHost.OnTabChangeListener() {
			@Override
			public void onTabChanged(String s) {
				updateMenuItemVisibility(tabs.getCurrentTab() == 1);
			}
		});

		tabs.addTab(tabs.newTabSpec("statusTab").setIndicator("Status").setContent(R.id.statusTab));
		tabs.addTab(tabs.newTabSpec("configTab").setIndicator("Settings").setContent(R.id.settingsTab));


		// Set up lotsa things...
		int serverID = getIntent().getIntExtra(EXTRA_SERVER_ID, -1);
		mServer = Connection.get().findServerByID(serverID);
		if (mServer == null) {
			finish();
		} else {
			mServer.registerListener(this);

			mTitle.setText(mServer.title);
			mNickname.setText(mServer.nickname);
			mAltNick.setText(mServer.altNick);
			mUsername.setText(mServer.username);
			mRealName.setText(mServer.realname);
			mHostname.setText(mServer.hostname);
			mPort.setText(String.valueOf(mServer.port));
			mUseTls.setChecked(mServer.useTls);
			mUsePassword.setChecked(!mServer.password.isEmpty());
			mPassword.setEnabled(mUsePassword.isChecked());
			mPassword.setText(mServer.password);

			mUsePassword.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
				@Override
				public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
					mPassword.setEnabled(b);
				}
			});

			getActionBar().setTitle(mServer.getDisplayTitle());
			getActionBar().setSubtitle(mServer.getDisplayState());

			boolean showSettings = (mServer.state == 0);
			tabs.setCurrentTab(showSettings ? 1 : 0);


			WindowData wd = Connection.get().findWindowByID(mServer.statusWindowId);
			if (wd != null) {
				getSupportFragmentManager().beginTransaction().
						add(R.id.statusTab, wd.createFragment()).
						commit();
			}
		}
	}

	@Override
	protected void onDestroy() {
		if (mServer != null)
			mServer.deregisterListener(this);
		super.onDestroy();
	}


	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		getMenuInflater().inflate(R.menu.server, menu);

		mSaveItem = menu.findItem(R.id.menu_save);
		mDeleteItem = menu.findItem(R.id.menu_delete);
		mSwitchItem = menu.findItem(R.id.menu_toggle_connection);

		updateMenuItemVisibility(((TabHost)findViewById(R.id.tabHost)).getCurrentTab() == 1);

		mConnSwitch = (Switch)(mSwitchItem.getActionView());
		mConnSwitch.setEnabled(mServer.isConnSwitchEnabled());
		mConnSwitch.setChecked(mServer.isConnSwitchChecked());

		mConnSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
			@Override
			public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
				mServer.connSwitchToggled(b);
				Connection.get().notifyServersUpdated();
				handleConnStateChanged();
			}
		});

		return true;
	}

	@Override
	public void handleConnStateChanged() {
		getActionBar().setSubtitle(mServer.getDisplayState());
		if (mConnSwitch != null) {
			mConnSwitch.setEnabled(mServer.isConnSwitchEnabled());
			mConnSwitch.setChecked(mServer.isConnSwitchChecked());
		}
	}

	private void updateMenuItemVisibility(boolean isSettings) {
		if (mSaveItem != null) {
			mSaveItem.setVisible(isSettings);
			mDeleteItem.setVisible(isSettings);
			mSwitchItem.setVisible(!isSettings);
		}
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId()) {
			case android.R.id.home:
				finish();
				return true;
			case R.id.menu_save:
				saveStuff();
				return true;
			case R.id.menu_delete:
				//deleteServer();
				return true;
		}
		return super.onOptionsItemSelected(item);
	}




	private void saveStuff() {
		mNickname.setError(null);
		mAltNick.setError(null);
		mUsername.setError(null);
		mRealName.setError(null);
		mHostname.setError(null);
		mPort.setError(null);
		mPassword.setError(null);

		String title = mTitle.getText().toString();
		String nickname = mNickname.getText().toString();
		String altNick = mAltNick.getText().toString();
		String username = mUsername.getText().toString();
		String realName = mRealName.getText().toString();
		String hostname = mHostname.getText().toString();
		String portStr = mPort.getText().toString();
		int port = -1;
		if (!TextUtils.isEmpty(portStr)) {
			port = Integer.parseInt(portStr);
		}
		String password = mPassword.getText().toString();

		boolean usesPassword = mUsePassword.isChecked();
		boolean usesTls = mUseTls.isChecked();

		View errorView = null;

		if (usesPassword && TextUtils.isEmpty(password)) {
			mPassword.setError("Required");
			errorView = mPassword;
		}

		if (port < 1 || port > 65535) {
			mPort.setError("Invalid");
			errorView = mPort;
		}
		if (TextUtils.isEmpty(hostname)) {
			mHostname.setError("Required");
			errorView = mHostname;
		}

		if (TextUtils.isEmpty(username)) {
			mUsername.setError("Required");
			errorView = mUsername;
		}

		if (TextUtils.isEmpty(realName)) {
			mRealName.setError("Required");
			errorView = mRealName;
		}

		if (TextUtils.isEmpty(altNick)) {
			mAltNick.setError("Required");
			errorView = mAltNick;
		}

		if (TextUtils.isEmpty(nickname)) {
			mNickname.setError("Required");
			errorView = mNickname;
		}


		if (errorView != null) {
			errorView.requestFocus();
		} else {
			mServer.title = title;
			mServer.nickname = nickname;
			mServer.altNick = altNick;
			mServer.username = username;
			mServer.realname = realName;
			mServer.port = port;
			mServer.password = usesPassword ? password : "";
			mServer.useTls = usesTls;

			getActionBar().setTitle(mServer.getDisplayTitle());

			if (mServer.isConnSwitchChecked()) {
				Toast.makeText(this, "Saving.. Changes will take effect after you reconnect.", Toast.LENGTH_LONG).show();
			} else {
				Toast.makeText(this, "Saving...", Toast.LENGTH_SHORT).show();
			}

			mServer.sendConfigToServer();
		}
	}

}
