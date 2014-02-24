VulpIRC
=======

A hybrid IRC client/bouncer designed for mobile platforms.

VulpIRC currently includes a back-end bouncer/server which runs on POSIX
and Windows systems, a client for Android, and a prototype
desktop client written using PyQt (for testing purposes).

(Possible future plans include a polished desktop client, an iOS client, a
standalone Android client using a modified core compiled via JNI, and a ZNC
module to allow VulpIRC clients to connect to a running ZNC instance, but none
of these are final - simply ideas I'd like to explore in the future.)

It's currently functional for basic usage, though it's lacking lots of
features and needs tons of polish. With any luck, eventually it will grow to
become a full (and hopefully awesome) client ;)

More info in the introduction blog post:
http://blog.brokenfox.net/post/77022394144/introducing-vulpirc

This code is placed under the MIT License - see the `LICENSE` file for more
details.

Copyright 2013-2014 Ninjifox/Treeki


Quick Start
-----------

See `MANUAL.md` for setup and usage instructions.


Currently Implemented Features
------------------------------

This is not an extensive feature list for VulpIRC - these are just the things
I've implemented already. There's tons more to go after this...

### Bouncer

- Written in C++ with minimal dependencies (gnutls for SSL/TLS support, which
  is optional)
- Supports SSL/TLS for client connections and for IRC server connections
- Runs on POSIX and Windows systems (mainly developed and tested on Linux,
  but I plan to try and make sure it works on other systems)
- Single user (multiple user support planned)
- Multiple clients and IRC servers
- Sessions are automatically suspended in case of connection loss, and can be
  resumed for as long as they remain active
- Login authentication

### Android Client

- Designed for and compatible with Android 4.x
- Swipe left/right through open tabs or see a full list in a pull-out drawer
- Displays formatting (bold, italic, underline, colours)
- Compatible with Samsung's Multi-Window
- So featureless that it's actually pretty fast and speedy ;)





