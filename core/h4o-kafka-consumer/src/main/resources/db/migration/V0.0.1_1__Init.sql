CREATE SCHEMA if not exists metadata;
CREATE SCHEMA if not exists timeseries;

DROP TABLE IF EXISTS timeseries.messages cascade;
DROP TABLE IF EXISTS timeseries.measurements cascade;
DROP TABLE IF EXISTS timeseries.substitutions cascade;
DROP TABLE IF EXISTS metadata.channels cascade;
DROP TABLE IF EXISTS metadata.things cascade;
DROP TABLE IF EXISTS metadata.thing_nodes cascade;


CREATE TABLE metadata.thing_nodes
(
    id          varchar(127) primary key,
    parent_id   varchar(127) REFERENCES metadata.thing_nodes (id),
    name        varchar(127) not null,
    description varchar(255)
);

CREATE TABLE metadata.things
(
    id            varchar(127) primary key,
    name          varchar(127) not null,
    description   varchar(255)
);

CREATE TABLE metadata.thing_nodes_to_things
(
    thing_node_id varchar(127) not null REFERENCES metadata.thing_nodes (id),
    thing_id      varchar(127) not null REFERENCES metadata.things (id),
    primary key (thing_node_id, thing_id)
);

CREATE TABLE metadata.channels
(
    id          varchar(127) primary key,
    thing_id    varchar(127) REFERENCES metadata.things (id),
    name        varchar(127) not null,
    description varchar(255),
    unit        varchar(10),
    is_writable bool
);
CREATE TABLE timeseries.messages
(
    channel_id varchar(127) not null references metadata.channels (id),
    timestamp  timestamp,
    message    varchar(255) not null,
    PRIMARY KEY(timestamp, channel_id)
);

CREATE TABLE timeseries.measurements
(
    channel_id  varchar(127)   not null references metadata.Channels (id),
    timestamp   timestamp,
    measurement numeric(12, 6) not null,
    PRIMARY KEY(timestamp, channel_id)
);

CREATE TABLE timeseries.substitutions
(
    channel_id  varchar(127)   not null references metadata.Channels (id),
    timestamp   timestamp,
    measurement numeric(12, 6) not null,
    PRIMARY KEY(timestamp, channel_id)
);

-- Insert devices structure

INSERT INTO metadata.thing_nodes (id, parent_id, name, description)
VALUES ('H4O', null, 'H4O Geräte', 'H4O - Geräte - Description');

INSERT INTO metadata.things(id, name, description)
VALUES ('S1', 'LA_DEVICE_1', 'Lüftungsampel 1');

INSERT INTO metadata.thing_nodes_to_things(thing_node_id, thing_id)
VALUES ('H4O', 'S1');

INSERT INTO metadata.channels(id, thing_id, name, description, unit, is_writable)
VALUES ('CO2', 'S1', 'CO2', 'CO2 Description', 'unit', true);
INSERT INTO metadata.channels(id, thing_id, name, description, unit, is_writable)
VALUES ('TEMP', 'S1', 'TEMP', 'TEMP Description', 'unit', true);

-- INSERT INTO metadata.channels(id, thing_id, name, description, unit, is_writable)
-- VALUES ('ENVIRONMENT-TEMPERATURE', 'LA1', 'ENVIRONMENT-TEMPERATURE', 'ENVIRONMENT TEMPERATURE Description', 'unit', true);
-- INSERT INTO metadata.channels(id, thing_id, name, description, unit, is_writable)
-- VALUES ('SENSOR-TEMPERATURE', 'LA1', 'SENSOR-TEMPERATURE', 'SENSOR TEMPERATURE Description', 'unit', true);
-- INSERT INTO metadata.channels(id, thing_id, name, description, unit, is_writable)
-- VALUES ('HUMIDITY', 'LA1', 'HUMIDITY', 'Humidity Description', 'unit', true);
-- INSERT INTO metadata.channels(id, thing_id, name, description, unit, is_writable)
-- VALUES ('PRESSURE', 'LA1', 'PRESSURE', 'Pressure Beschreibung', 'unit', true);
