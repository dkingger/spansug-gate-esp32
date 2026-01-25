#!/bin/bash
# Clear all retained MQTT messages

echo "Clearing all retained MQTT messages under spansug/..."

mosquitto_sub -h localhost -t 'spansug/#' --retained-only -W 2 -v | while read line; do
    topic=$(echo "$line" | awk '{print $1}')
    if [ -n "$topic" ]; then
        mosquitto_pub -h localhost -t "$topic" -r -n
        echo "Cleared: $topic"
    fi
done

echo "Done!"
