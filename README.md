pebble-homecontrol
==================

Remote app for my HomeControl automation server on Pebble, using Katharine's httpebble. 
The config.bin file is notably absent from this repository; the format is a simple packed struct with 2 128-byte cstrings: the URL for your server's Pebble interface (generally http://your_server_ip_or_domain:port/pebble), and the password for a user on the server named "PEBBLE". This will likely change to something more sensible later.
