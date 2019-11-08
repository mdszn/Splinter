# Splinter

## To build

*make* will build the splinter program as well as the shell invoked.

## To run,

In order to run *Splinter*, you will need to use your IPv4 Address for the connection address.

Example:

`./splinter start -a 127.0.0.1 -p 5731`

Once the server has started up, you can connect the client to it the same way

Example:

`./splinter connect -a 127.0.0.1 -p 5731`

## Shell commands

`_globon` : Turn on shell globbing (on by default)

`_globoff` : Turn off shell globbing
