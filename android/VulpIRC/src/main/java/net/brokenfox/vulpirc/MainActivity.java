package net.brokenfox.vulpirc;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.support.v4.app.*;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.support.v4.view.ViewPager;
import android.support.v4.widget.DrawerLayout;
import android.util.Log;
import android.view.*;
import android.widget.*;

public class MainActivity extends FragmentActivity implements Connection.ConnectionListener, Connection.LoginStateListener {

	private LinearLayout mContainer;
	private LinearLayout mDrawerMainContent;
	private ListView mWindowList;
	private ViewPager mWindowPager;
	private WindowListAdapter mWindowListAdapter;
	private WindowPagerAdapter mWindowPagerAdapter;

	private boolean mUsingDrawerMode;
	private DrawerLayout mDrawerLayout;
	private ActionBarDrawerToggle mDrawerToggle;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

	    mUsingDrawerMode = true; // default state as per XML.

	    mContainer = (LinearLayout)findViewById(R.id.container);
	    mDrawerMainContent = (LinearLayout)findViewById(R.id.drawerMainContent);

	    mDrawerLayout = (DrawerLayout)findViewById(R.id.drawerLayout);
	    mDrawerToggle = new ActionBarDrawerToggle(this, mDrawerLayout,
			    R.drawable.ic_drawer,
			    R.string.drawer_open, R.string.drawer_close) {
		    @Override
		    public void onDrawerOpened(View drawerView) {
			    super.onDrawerOpened(drawerView);
			    getActionBar().setTitle("VulpIRC");
			    invalidateOptionsMenu();
		    }

		    @Override
		    public void onDrawerClosed(View drawerView) {
			    super.onDrawerClosed(drawerView);

			    fixActionBarTitle();
			    invalidateOptionsMenu();
		    }
	    };
	    mDrawerLayout.setDrawerListener(mDrawerToggle);
	    mDrawerLayout.setDrawerShadow(R.drawable.drawer_shadow, Gravity.LEFT);

	    getActionBar().setHomeButtonEnabled(true);
	    getActionBar().setDisplayHomeAsUpEnabled(true);

	    mWindowList = (ListView)findViewById(R.id.windowList);
	    mWindowList.setChoiceMode(AbsListView.CHOICE_MODE_SINGLE);
	    mWindowList.setOnItemClickListener(new WindowListItemClickListener());
	    mWindowList.setBackgroundColor(0x60000000);
	    mWindowListAdapter = new WindowListAdapter();

	    mWindowPager = (ViewPager)findViewById(R.id.windowPager);
	    mWindowPager.setOnPageChangeListener(new WindowPagerPageChangeListener());
	    mWindowPagerAdapter = new WindowPagerAdapter(getSupportFragmentManager());

	    setupDrawerLayout(Connection.get().uiUsingDrawer);

	    doBindService();
    }

	@Override
	protected void onPostCreate(Bundle savedInstanceState) {
		super.onPostCreate(savedInstanceState);
		mDrawerToggle.syncState();
	}

	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
		mDrawerToggle.onConfigurationChanged(newConfig);
	}

	@Override
	protected void onDestroy() {
		if (mService != null) {
			Connection.get().deregisterListener(this);
			Connection.get().deregisterLoginStateListener(this);
		}
		doUnbindService();
		super.onDestroy();
	}



	private void setupDrawerLayout(boolean usingDrawerMode) {
		if (usingDrawerMode == mUsingDrawerMode)
			return;
		mUsingDrawerMode = usingDrawerMode;

		float density = getResources().getDisplayMetrics().density;

		DrawerLayout.LayoutParams listParams = new DrawerLayout.LayoutParams(
				(int)(density * 160 + 0.5), ViewGroup.LayoutParams.MATCH_PARENT);
		listParams.gravity = Gravity.LEFT;

		if (mUsingDrawerMode) {
			mDrawerMainContent.removeView(mWindowList);
			mWindowList.setLayoutParams(listParams);
			mDrawerLayout.addView(mWindowList);
		} else {
			mDrawerLayout.removeView(mWindowList);
			mDrawerMainContent.addView(mWindowList, 0, listParams);
			mWindowList.setVisibility(View.VISIBLE);
		}

		mDrawerToggle.setDrawerIndicatorEnabled(mUsingDrawerMode);
		getActionBar().setHomeButtonEnabled(mUsingDrawerMode);
		getActionBar().setDisplayHomeAsUpEnabled(mUsingDrawerMode);
	}



	@Override
	public void handlePublicWindowsUpdated() {
		mWindowPagerAdapter.notifyDataSetChanged();
		mWindowListAdapter.notifyDataSetChanged();
	}
	@Override
	public void handleServerWindowsUpdated() {
		// nothing here
	}
	@Override
	public void handleServersUpdated() {
		// nothing here
	}

	@Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main, menu);
		menu.findItem(R.id.action_pinWindowList).setChecked(!mUsingDrawerMode);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (mDrawerToggle.onOptionsItemSelected(item))
	        return true;

        switch (item.getItemId()) {
            case R.id.action_settings:
                return true;

	        case R.id.action_pinWindowList:
		        setupDrawerLayout(item.isChecked());
		        item.setChecked(!item.isChecked());
		        Connection.get().uiUsingDrawer = mUsingDrawerMode;
		        return true;

	        case R.id.actionLogOut:
		        Connection.get().disconnect();
		        return true;
        }
        return super.onOptionsItemSelected(item);
    }


	private void fixActionBarTitle() {
		if (mUsingDrawerMode && mDrawerLayout.isDrawerOpen(mWindowList))
			return;

		WindowData w = Connection.get().getActiveWindow();
		if (w == null)
			getActionBar().setTitle("VulpIRC");
		else
			getActionBar().setTitle(w.title);
	}


	@Override
	public void handleLoginStateChanged() {
		BaseConn.SocketState s = Connection.get().getSocketState();
		switch (s) {
			case CONNECTED:
				getActionBar().setSubtitle("Connected!");
				break;
			case CONNECTING:
				getActionBar().setSubtitle("Connecting...");
				break;
			case DISCONNECTED:
				getActionBar().setSubtitle("Disconnected.");
				break;
			case DISCONNECTING:
				getActionBar().setSubtitle("Disconnecting...");
				break;
		}
	}

	private class WindowListItemClickListener implements AdapterView.OnItemClickListener {
		@Override
		public void onItemClick(AdapterView<?> adapterView, View view, int position, long id) {
			mWindowPager.setCurrentItem(position);
			mWindowList.setItemChecked(position, true);
		}
	}

	private class WindowPagerPageChangeListener extends ViewPager.SimpleOnPageChangeListener {
		@Override
		public void onPageSelected(int position) {
			mWindowList.setItemChecked(position, true);
			if (Connection.get().getActiveWindow() != null)
				Log.i("VulpIRC", "Boop1 : " + Connection.get().getActiveWindow().title);

			if (position == 0)
				Connection.get().setActiveWindow(Connection.get().statusWindow);
			else
				Connection.get().setActiveWindow(Connection.get().publicWindows.get(position - 1));

			Log.i("VulpIRC", "Boop2 : " + Connection.get().getActiveWindow().title);
			fixActionBarTitle();
		}
	}


	private class WindowListItemView extends LinearLayout {
		private ImageView mStatusView;
		private TextView mTitleView;
		private TextView mUnreadView;

		public WindowListItemView(Context context) {
			super(context);

			setOrientation(HORIZONTAL);

			mStatusView = new ImageView(context);
			mStatusView.setImageDrawable(getResources().getDrawable(R.drawable.window_status));
			mTitleView = new TextView(context);
			mTitleView.setGravity(0x800003|0x10); // start|center_vertical
			mUnreadView = new TextView(context);
			mUnreadView.setGravity(0x800003|0x10); // start|center_vertical

			float density = getResources().getDisplayMetrics().density;
			setLayoutParams(new AbsListView.LayoutParams(LayoutParams.MATCH_PARENT, (int) (36 * density + 0.5f)));

			int statusSize = (int)(8 * density + 0.5f);
			LayoutParams statuslp = new LayoutParams(statusSize, LayoutParams.MATCH_PARENT);

			int titlePadding = (int)(16 * density + 0.5f);
			mTitleView.setPadding(titlePadding, 0, 0, 0);

			LayoutParams titleParams = new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.MATCH_PARENT);
			titleParams.weight = 1;

			mUnreadView.setPadding(titlePadding, 0, titlePadding, 0);

			addView(mStatusView, statuslp);
			addView(mTitleView, titleParams);
			addView(mUnreadView, new LayoutParams(LayoutParams.WRAP_CONTENT, (int) (24 * density + 0.5f)));

			// I really don't like this.
			TypedArray a = getTheme().obtainStyledAttributes(new int[] { android.R.attr.activatedBackgroundIndicator });
			//setBackground(getResources().getDrawable(android.R.attr.activatedBackgroundIndicator));
			setBackgroundDrawable(a.getDrawable(0));
			a.recycle();
		}

		public void setPlaceholder() {
			mTitleView.setText("Placeholder");
		}
		public void setWindow(WindowData wd) {
			mTitleView.setText(wd.title);
			mStatusView.setImageLevel(wd.unreadLevel);
			mUnreadView.setText(String.valueOf(wd.unreadCount));
			mUnreadView.setVisibility((wd.unreadCount == 0) ? INVISIBLE : VISIBLE);
		}
	}

	private class WindowListAdapter extends BaseAdapter implements ListAdapter {
		@Override
		public int getCount() {
			return 1 + Connection.get().publicWindows.size();
		}

		@Override
		public Object getItem(int i) {
			if (i == 0)
				return "boop";
			else
				return Connection.get().publicWindows.get(i - 1);
		}

		@Override
		public long getItemId(int i) {
			if (i == 0)
				return -1;
			else
				return Connection.get().publicWindows.get(i - 1).id;
		}

		@Override
		public View getView(int i, View view, ViewGroup viewGroup) {
			WindowListItemView iv;

			if (view != null && view instanceof WindowListItemView) {
				iv = (WindowListItemView)view;
			} else {
				iv = new WindowListItemView(viewGroup.getContext());
			}

			if (i == 0)
				iv.setWindow(Connection.get().statusWindow);
			else
				iv.setWindow(Connection.get().publicWindows.get(i - 1));
			return iv;
		}
	}

	private class WindowPagerAdapter extends FragmentPagerAdapter {
		WindowPagerAdapter(FragmentManager fm) {
			super(fm);
		}

		@Override
		public Fragment getItem(int i) {
			WindowData window = null;
			if (i == 0)
				window = Connection.get().statusWindow;
			else
				window = Connection.get().publicWindows.get(i - 1);

			return window.createFragment();
		}

		@Override
		public long getItemId(int position) {
			if (position == 0)
				return -1;
			else
				return Connection.get().publicWindows.get(position - 1).id;
		}

		@Override
		public CharSequence getPageTitle(int position) {
			if (position == 0)
				return "Status";
			else
				return Connection.get().publicWindows.get(position - 1).title;
		}

		@Override
		public int getCount() {
			return 1 + Connection.get().publicWindows.size();
		}
	}




	// SERVICE JUNK
	private IRCService mService = null;

	private final ServiceConnection mServiceConnection = new ServiceConnection() {
		@Override
		public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
			Log.i("VulpIRC", "[MainActivity connected to IRCService]");

			mService = ((IRCService.LocalBinder)iBinder).getService();
			Connection.get().registerListener(MainActivity.this);
			Connection.get().registerLoginStateListener(MainActivity.this);

			mWindowList.setAdapter(mWindowListAdapter);
			mWindowPager.setAdapter(mWindowPagerAdapter);

			handleLoginStateChanged();
		}

		@Override
		public void onServiceDisconnected(ComponentName componentName) {
			Log.i("VulpIRC", "[MainActivity disconnected from IRCService]");

			mService = null;
		}
	};

	private void doBindService() {
		Log.i("VulpIRC", "[MainActivity binding to IRCService...]");

		Intent i = new Intent(this, IRCService.class);
		bindService(i, mServiceConnection, BIND_AUTO_CREATE);
	}

	private void doUnbindService() {
		Log.i("VulpIRC", "[MainActivity unbinding from IRCService...]");

		unbindService(mServiceConnection);
	}

}
