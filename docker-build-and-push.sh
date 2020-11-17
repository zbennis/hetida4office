#!/bin/sh
docker build -f Dockerfile.h4o.connector.mqtt -t h4o/connector-mqtt:latest .
docker build -f Dockerfile.h4o.kafka.consumer -t h4o/kafka-consumer:latest .
