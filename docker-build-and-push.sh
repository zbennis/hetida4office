#!/bin/sh
docker build -f Dockerfile.h4o.connector.mqtt -t "hetida4office/connector-mqtt:$1" .
docker build -f Dockerfile.h4o.kafka.consumer -t "hetida4office/kafka-consumer:$1" .
docker push "hetida4office/connector-mqtt:$1"
docker push "hetida4office/kafka-consumer:$1"
