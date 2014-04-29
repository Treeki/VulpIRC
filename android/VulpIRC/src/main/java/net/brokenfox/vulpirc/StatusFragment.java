package net.brokenfox.vulpirc;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.*;

/**
 * Created by ninji on 4/6/14.
 */
public class StatusFragment extends WindowFragment implements Connection.ConnectionListener {
	private ServersAdapter mServersAdapter;
	private ListView mServersList;
	private LayoutInflater mLayoutInflater;

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
		Context ctx = container.getContext();

		// This is really kludgey, but... whatever ><;;
		LinearLayout lv = (LinearLayout)super.onCreateView(inflater, container, savedInstanceState);

		LinearLayout.LayoutParams mlParams = new LinearLayout.LayoutParams(
				ViewGroup.LayoutParams.MATCH_PARENT,
				ViewGroup.LayoutParams.MATCH_PARENT);
		mlParams.weight = 1;

		mServersList = new ListView(ctx);
		lv.addView(mServersList, mlParams);

		mLayoutInflater = (LayoutInflater)getActivity().getSystemService(Context.LAYOUT_INFLATER_SERVICE);

		return lv;
	}

	@Override
	public void onDestroyView() {
		mLayoutInflater = null;
		super.onDestroyView();
	}

	@Override
	public void onActivityCreated(Bundle savedInstanceState) {
		super.onActivityCreated(savedInstanceState);

		mServersAdapter = new ServersAdapter();
		mServersList.setAdapter(mServersAdapter);

		mServersList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
			@Override
			public void onItemClick(AdapterView<?> adapterView, View view, int i, long l) {
				ServerData s = Connection.get().servers.get(i);
				Intent intent = new Intent(getActivity(), ServerActivity.class);
				intent.putExtra(ServerActivity.EXTRA_SERVER_ID, s.id);
				startActivity(intent);
			}
		});

		Connection.get().registerListener(this);
	}

	@Override
	public void onDestroy() {
		Connection.get().deregisterListener(this);
		super.onDestroy();
	}

	private static class ViewHolder {
		public TextView title, status;
		public Switch connSwitch;

		public ViewHolder(View base) {
			title = (TextView)base.findViewById(R.id.serverName);
			status = (TextView)base.findViewById(R.id.serverStatus);
			connSwitch = (Switch)base.findViewById(R.id.toggleConnection);
		}
	}

	private class ServersAdapter extends BaseAdapter implements ListAdapter, Switch.OnCheckedChangeListener {
		@Override
		public int getCount() {
			return Connection.get().servers.size();
		}

		@Override
		public Object getItem(int i) {
			return Connection.get().servers.get(i);
		}

		@Override
		public long getItemId(int i) {
			return i;
		}

		@Override
		public View getView(int i, View view, ViewGroup viewGroup) {
			Object _tag = null;
			ViewHolder holder = null;

			if (view != null)
				_tag = view.getTag();

			if (_tag != null && _tag instanceof ViewHolder) {
				holder = (ViewHolder)_tag;
			} else {
				view = mLayoutInflater.inflate(R.layout.server_list_entry, viewGroup, false);
				holder = new ViewHolder(view);
				view.setTag(holder);

				holder.connSwitch.setOnCheckedChangeListener(this);
			}

			ServerData server = Connection.get().servers.get(i);
			holder.title.setText(server.getDisplayTitle());
			holder.status.setText(server.getDisplayState());
			holder.connSwitch.setTag(server.id);
			holder.connSwitch.setChecked(server.isConnSwitchChecked());
			holder.connSwitch.setEnabled(server.isConnSwitchEnabled());

			return view;
		}

		@Override
		public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
			int serverID = (Integer)compoundButton.getTag();
			ServerData server = Connection.get().findServerByID(serverID);

			if (server != null) {
				server.connSwitchToggled(b);
				mServersAdapter.notifyDataSetChanged();
			}
		}
	}

	@Override
	public void handleServerWindowsUpdated() {
		// Nothing here
	}

	@Override
	public void handlePublicWindowsUpdated() {
		// Nothing here
	}

	@Override
	public void handleServersUpdated() {
		// nothing here
		mServersAdapter.notifyDataSetChanged();
	}
}

