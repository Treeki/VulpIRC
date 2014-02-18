package net.brokenfox.vulpirc;

import android.os.Bundle;

/**
 * Created by ninji on 2/6/14.
 */
public class ChannelFragment extends WindowFragment implements ChannelData.ChannelListener {
	private ChannelData mChannelData;

	@Override
	public void onActivityCreated(Bundle savedInstanceState) {
		super.onActivityCreated(savedInstanceState);

		mChannelData = (ChannelData)mData;
	}

	@Override
	public void handleUsersChanged() {
		// boop
	}
}
