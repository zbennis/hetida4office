package de.neustasdwest.h4o.testconsumer;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

@SpringBootApplication
public class H4OKafkaConsumerApplication {

    public static void main(final String[] args) {
        SpringApplication.run(H4OKafkaConsumerApplication.class, args);
    }
}
