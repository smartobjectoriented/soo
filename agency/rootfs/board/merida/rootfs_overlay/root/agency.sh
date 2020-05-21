#!/bin/bash

# Set the Smart Object name
/root/soo_name.sh

# Set up wifi
/root/wifimanager.sh

# Start the agency
killall -USR1 agency
