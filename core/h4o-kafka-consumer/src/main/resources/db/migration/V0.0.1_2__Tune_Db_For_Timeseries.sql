-- Tune timescaledb for timeseries
SELECT create_hypertable('timeseries.measurements', 'timestamp');
ALTER TABLE timeseries.measurements SET (timescaledb.compress,timescaledb.compress_segmentby = 'channel_id');
SELECT add_compress_chunks_policy('timeseries.measurements', INTERVAL '7 days');

SELECT create_hypertable('timeseries.substitutions', 'timestamp');
ALTER TABLE timeseries.substitutions SET (timescaledb.compress,timescaledb.compress_segmentby = 'channel_id');
SELECT add_compress_chunks_policy('timeseries.substitutions', INTERVAL '7 days');

SELECT create_hypertable('timeseries.messages', 'timestamp');
ALTER TABLE timeseries.messages SET (timescaledb.compress,timescaledb.compress_segmentby = 'channel_id');
SELECT add_compress_chunks_policy('timeseries.messages', INTERVAL '7 days');