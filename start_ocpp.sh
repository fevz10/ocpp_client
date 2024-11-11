#!/bin/bash

while [ 1 ]
do
        # Define the process name
        process_name="/root/ocpp_client_cs"

        # Check if the process is running
        if ps aux | grep -v grep | grep "$process_name" > /dev/null; then
                echo "Process $process_name is already running."
                sleep 5
        else
                echo "Process $process_name is not running. Starting it..."
                # Run the process
                /root/ocpp_client_cs2 &
                echo "Process $process_name started."
        fi
done
