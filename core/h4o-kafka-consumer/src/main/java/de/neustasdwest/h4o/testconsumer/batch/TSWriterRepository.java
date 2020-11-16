package de.neustasdwest.h4o.testconsumer.batch;

import de.neustasdwest.h4o.common.model.Measurement;

import java.util.List;

public interface TSWriterRepository {
    public int[][] batchInsertMeasurements(List<Measurement> measurements);
}
