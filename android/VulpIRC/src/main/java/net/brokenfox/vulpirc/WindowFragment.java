package net.brokenfox.vulpirc;

import android.app.Activity;
import android.content.Context;
import android.support.v4.app.Fragment;
import android.os.Bundle;
import android.text.InputType;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.*;

/**
 * Created by ninji on 2/3/14.
 */
public class WindowFragment extends Fragment implements WindowData.WindowListener, TextView.OnEditorActionListener {
	private MessagesAdapter mMessagesAdapter;
	private ListView mMessagesList;
	private EditText mInput;
	protected WindowData mData = null;

	public WindowFragment() {
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
		Context ctx = container.getContext();

		LinearLayout l = new LinearLayout(ctx);
		l.setOrientation(LinearLayout.VERTICAL);

		float density = getResources().getDisplayMetrics().density;
		int padding = (int)(4 * density + 0.5f);

		mMessagesList = new ListView(ctx);
		mMessagesList.setStackFromBottom(true);
		mMessagesList.setTranscriptMode(AbsListView.TRANSCRIPT_MODE_NORMAL);
		mMessagesList.setDivider(null);
		mMessagesList.setDividerHeight(0);
		mMessagesList.setPadding(padding, padding, padding, padding);

		mInput = new EditText(ctx);
		mInput.setImeOptions(EditorInfo.IME_ACTION_SEND | EditorInfo.IME_FLAG_NO_FULLSCREEN);
		mInput.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL);
		mInput.setOnEditorActionListener(this);

		LinearLayout.LayoutParams mlParams = new LinearLayout.LayoutParams(
				ViewGroup.LayoutParams.MATCH_PARENT,
				ViewGroup.LayoutParams.MATCH_PARENT);
		mlParams.weight = 1;

		LinearLayout.LayoutParams inputParams = new LinearLayout.LayoutParams(
				ViewGroup.LayoutParams.MATCH_PARENT,
				ViewGroup.LayoutParams.WRAP_CONTENT);

		l.addView(mMessagesList, mlParams);
		l.addView(mInput, inputParams);

		return l;
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setRetainInstance(true);
	}

	@Override
	public void onActivityCreated(Bundle savedInstanceState) {
		super.onActivityCreated(savedInstanceState);

		Bundle b = getArguments();
		int winID = b.getInt("winID");
		mData = Connection.get().findWindowByID(winID);

		mMessagesAdapter = new MessagesAdapter();
		mMessagesList.setAdapter(mMessagesAdapter);

		mData.registerListener(this);
	}

	@Override
	public void onDestroy() {
		if (mData != null)
			mData.deregisterListener(this);
		super.onDestroy();
	}


	@Override
	public void handleMessagesChanged() {
		mMessagesAdapter.notifyDataSetChanged();
	}

	@Override
	public boolean onEditorAction(TextView textView, int i, KeyEvent keyEvent) {
		if (i == EditorInfo.IME_ACTION_SEND) {
			sendEnteredThing();
			return true;
		}

		return false;
	}



	private void sendEnteredThing() {
		CharSequence text = mInput.getText().toString();
		mInput.getText().clear();

		mData.sendUserInput(text);
	}



	private class MessagesAdapter extends BaseAdapter implements ListAdapter {
		@Override
		public int getCount() {
			return mData.messages.size() + mData.pendingMessages.size();
		}

		@Override
		public Object getItem(int i) {
			int msgCount = mData.messages.size();
			if (i < msgCount)
				return mData.messages.get(i);
			else
				return mData.pendingMessages.get(i - msgCount);
		}

		@Override
		public long getItemId(int i) {
			return i;
		}

		@Override
		public View getView(int i, View view, ViewGroup viewGroup) {
			int msgCount = mData.messages.size();

			TextView tv;

			if (view != null && view instanceof TextView) {
				tv = (TextView)view;
			} else {
				tv = new TextView(viewGroup.getContext());
			}

			if (i < msgCount) {
				tv.setText(mData.messages.get(i));
				tv.setBackgroundColor(0);
			} else {
				tv.setText(mData.pendingMessages.get(i - msgCount).text);
				tv.setBackgroundColor(0xFF300000);
			}
			return tv;
		}
	}
}