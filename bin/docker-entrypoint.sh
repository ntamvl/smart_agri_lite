#!/bin/bash
set -e

# Remove the PID file if it exists
rm -f /app/tmp/pids/server.pid

# Execute the main application command
exec "$@"
