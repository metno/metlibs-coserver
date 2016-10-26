coserver client library
=======================

server URL configuration
------------------------

a list of server URLs can be passed via the constructor of `CoClient`,
or configured via the method `setServerUrls`, or configured by default via
environment variables or `client.ini` files:

  * first, a server list is read from the environment variable
    `COSERVER_URLS`, which should be a space-separated list of urls, for
    example `co4://coserver1.my.org/ co4://coserver2.my.org/ localhost`
  * if this list is empty, a single server url is set from `COSERVER_HOST`
  * if the list is still empty, it is filled from the ini file
    `$HOME/.coserver/client.ini` using keys `server_0`, `server_1` and so on
    in the `[server]` section
  * if the list is still empty, `/etc/coserver/client.ini` (actually,
    `${sysconfdir}/coserver/client.ini`) is read
  * if the list i
     * then use defaults
     * configuration options: server list, attempt_to_start_server, server_command

client configuration
--------------------

The client tries to start a server if it does not receive a response
from an URL is regards as local. The command to start the server may
be configured via `setServerCommand`, or via `client.ini` using the
key `server_command`. This is actually the filename of the executable,
which will be passed arguments `-d`, `-u`, and `<URL>` as `coserver4`
expects them.

Attempts to start a server can be disabled with
`setAttemptToStartServer` or `attempt_to_start_server = False` in
`client.ini`.
