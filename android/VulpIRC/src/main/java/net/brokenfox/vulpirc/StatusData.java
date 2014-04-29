package net.brokenfox.vulpirc;

import android.support.v4.app.Fragment;

/**
 * Created by ninji on 4/6/14.
 */
public class StatusData extends WindowData {
	@Override
	protected Fragment instantiateFragmentClass() {
		return new StatusFragment();
	}
}
