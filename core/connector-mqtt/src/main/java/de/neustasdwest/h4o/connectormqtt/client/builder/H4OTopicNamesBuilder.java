package de.neustasdwest.h4o.connectormqtt.client.builder;

import lombok.extern.log4j.Log4j2;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;

@Log4j2
@Component
public class H4OTopicNamesBuilder implements TopicNamesBuilder{
    private final String topicSuffix;
    private final String[] topicsPrefixes;
    private final String[] devicesNames;

    public H4OTopicNamesBuilder( @Value("${h4o.device.topic.suffix}") final String topicSuffix,
                                 @Value("${h4o.device.topic.prefixes}")final String[] topicsPrefixes,
                                 @Value("${h4o.devices.names}") final String[] devicesNames) {
        this.topicSuffix = topicSuffix;
        this.topicsPrefixes = topicsPrefixes;
        this.devicesNames = devicesNames;
    }

    @Override
    public Set<String> buildTopicNames() {
        Objects.requireNonNull(topicSuffix,"Topic suffix must not be null");
        Objects.requireNonNull(topicsPrefixes, "Topics prefixes must not be null");
        Objects.requireNonNull(devicesNames, "Devices names must not be null");
        log.info("Generating topic names...");
        final Set<String> topics = new HashSet<>();
        Arrays.asList(topicsPrefixes).forEach(prefix -> {
            // TODO : build topic names for more devices
            final String newTopicName = String.join("/", topicSuffix, devicesNames[0], prefix);
            log.info(newTopicName);
            topics.add(newTopicName);
        });
        return topics;
    }
}
