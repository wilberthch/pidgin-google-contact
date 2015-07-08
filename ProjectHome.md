This plugin integrates pidgin (and other libpurple based IM clients) with the IM fields in the Google contacts API. It is convenient for keeping pidgin buddy lists in sync across computers and with Google contacts.

## Installation Instructions ##
Download and unzip the zip file, and copy the appropriate version of the plugin to the pidgin plugin directory. On Windows (Vista and 7), this usually means copy google-contact.dll to C:\Users\USERNAME\AppData\Roaming\.purple\plugins\, and on Linux, it usually means copying google-contact.so to ~/.purple/plugins/

## Configuration Instructions ##
The plugin currently has one user-definable option - the number of days between runs. By default, if the plugin failed in the previous run for some reason, or if it ended up making some changes, it will run upon startup if it is enabled. However, if nothing was changed in a previous successful run, the plugin will only be run after this number of days. To have the plugin run every time it is started regardless of previous runs, this value can be set to 0.

No other configuration is necessary. The plugin uses the login information for the Google talk accounts to connect to the Google contact database. Therefore, a Google talk account is necessary for the plugin to function.