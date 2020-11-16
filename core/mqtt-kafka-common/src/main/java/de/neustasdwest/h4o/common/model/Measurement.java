package de.neustasdwest.h4o.common.model;

import lombok.*;

import java.math.BigDecimal;
import java.time.Instant;

@Getter
@Setter
@ToString
@AllArgsConstructor
@NoArgsConstructor
@Builder
public class Measurement {
    private String channelId;
    private Instant timestamp;
    private BigDecimal measurement;
}
