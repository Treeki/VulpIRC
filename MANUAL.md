Thank you for trying out VulpIRC - or at least being interested in it enough
to read this document ;)

This client is currently extremely barebones in parts, since I've been slowly
fleshing out bits as I go along and there's areas I just haven't got to yet -
for instance, most `/commands` are currently not implemented, requiring you to
enter raw IRC commands into the server status window instead.


Quick Start Guide
-----------------

### Bouncer

- *(You'll need a C++11-capable compiler, and gnutls installed)*
- Run `bouncer/build.sh` to compile a dynamically-linked binary
- ..or alternatively, run `bouncer/build_static.sh` to compile a statically
  linked binary without gnutls
- Start `bouncer/binary/vulpircd` or `bouncer/binary/vulpircd_static`
- VulpIRC will be listening on port 5454, with the default settings

### Test Client

- *(You'll need Python 3 and PyQt5)*
- Run `python3 python_client.py <hostname> [<password>]`
- Click Connect and then Login

### Android Client

- *(You'll need the Android SDK and the Support Library installed)*
- Compile and install the app inside the `android/` directory
- Enter your server login details into the app:
  - Username can be anything; it's currently ignored
  - Password is whatever's configured inside `config.ini`
  - Address is an IP or hostname to find the server on
  - Port should be 5454
  - SSL is currently not supported; the choice is ignored
- ...and you'll be in!
- Choose Disconnect from the action overflow menu to end the session

### Notes

- For security, you should set a password: run the `save` command at least
  once to generate a `config.ini` file, modify it to add a password, and then
  start up the server again
- The Android client does not yet support debug window commands, so you can
  only add new servers by editing config.ini or by running `addsrv` from the
  desktop test client


Control Commands
----------------

### Debug Window

- `addsrv` - adds a new, blank IRC network
- `save` - saves the current configuration to `config.ini`
- `quit` - terminates the server
- `all <text>` - sends some text to the debug window of all connected clients

### Server Status Window

- `/connect` - connect to the network
- `/disconnect` - disconnect from the network
- `/defaultnick <nick>` - set your default nickname
- `/altnick <nick>` - set your alternate nickname
- `/server <hostname>` - set the server address
- `/port [+]<port>` - set the server port; prefix with + to use SSL/TLS
- `/username <name>` - set your IRC username
- `/realname <name>` - set your IRC realname
- `/password [<pass>]` - set or clear your IRC server password

Anything typed into the server status window without an / prefix will be sent
directly to the IRC server.

